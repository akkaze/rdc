#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdint>
#include <cstring>

#include "common/status.h"
#include "core/work_request.h"
#include "rdc.h"
#include "sys/error.h"
#include "transport/channel.h"
#include "transport/tcp/tcp_adapter.h"
#include "transport/tcp/tcp_channel.h"
namespace rdc {
TcpChannel::TcpChannel() {
    this->adapter_ = nullptr;
    this->sock_ = TcpSocket();
    this->set_kind(ChannelKind::kRead);
    this->set_error_detected(false);
}

TcpChannel::TcpChannel(const ChannelKind& kind) {
    this->adapter_ = nullptr;
    this->sock_ = TcpSocket();
    this->set_kind(kind);
    this->set_error_detected(false);
}

TcpChannel::TcpChannel(TcpAdapter* adapter, const ChannelKind& kind) {
    this->adapter_ = adapter;
    this->sock_ = TcpSocket();
    this->set_kind(kind);
    this->set_error_detected(false);
    this->adapter_->AddChannel(this);
}

TcpChannel::TcpChannel(TcpAdapter* adapter, const int& sockfd,
                       const ChannelKind& kind) {
    this->adapter_ = adapter;
    this->sock_ = TcpSocket(sockfd);
    this->set_kind(kind);
    this->set_error_detected(false);
    this->adapter_->AddChannel(this);
}

TcpChannel::TcpChannel(TcpAdapter* adapter, const TcpSocket& sock,
                       const ChannelKind& kind) {
    this->adapter_ = adapter;
    this->sock_ = sock;
    this->set_kind(kind);
    this->set_error_detected(false);
    this->adapter_->AddChannel(this);
}

TcpChannel::~TcpChannel() {
    if (!closing_.load(std::memory_order_acquire)) {
        this->Close();
    }
}

void TcpChannel::Close() {
    closing_.store(true, std::memory_order_release);
    if (this->adapter_) {
        adapter_->RemoveChannel(this);
    }
    if (!sock_.IsClosed()) {
        sock_.Close();
    }
    LOG_F(INFO, "channel with parent communicator %s from %d to %d is closed",
          comm().c_str(), GetRank(), peer_rank());
}

void TcpChannel::ModifyKind(const ChannelKind& kind) {
    this->set_kind(kind);
    if (this->adapter_) {
        adapter_->ModifyChannel(this, kind);
    }
}

bool TcpChannel::Connect(const std::string& hostname, const uint32_t& port) {
    VLOG_F(2, "Trying to connect to process on host %s and port %d",
           hostname.c_str(), port);
    if (!sock_.Connect(hostname, port)) {
        return false;
    }
    sock_.SetNonBlock(true);
    if (this->adapter_ == nullptr) {
        this->set_adapter(TcpAdapter::Get());
        this->adapter_->AddChannel(this);
    }
    return true;
}

WorkCompletion* TcpChannel::ISend(const Buffer sendbuf) {
    uint64_t send_req_id = WorkRequestManager::Get()->NewWorkRequest(
        WorkType::kSend, sendbuf.addr(), sendbuf.size_in_bytes());
    auto wc = WorkCompletion::New(send_req_id);
    auto& send_req = WorkRequestManager::Get()->GetWorkRequest(send_req_id);
    do {
        const auto& write_nbytes = sock_.Send(
            send_req.pointer_at<uint8_t>(send_req.processed_bytes_upto_now()),
            send_req.remain_nbytes());
        if (write_nbytes > 0) {
            if (send_req.AddBytes(write_nbytes)) {
                WorkRequestManager::Get()->set_status(send_req.id(),
                                                      WorkStatus::kFinished);
                send_req.Notify();
            }
        } else if (write_nbytes == -1 && errno == EAGAIN) {
            send_reqs_.Push(send_req_id);
            this->AddEventOfInterest(ChannelKind::kWrite);
            break;
        } else {
            WorkRequestManager::Get()->set_status(send_req.id(),
                                                  WorkStatus::kError);
            send_req.Notify();
            send_reqs_.Pop();
        }
    } while (send_req.status() != WorkStatus::kFinished);
    return wc;
}

WorkCompletion* TcpChannel::IRecv(Buffer recvbuf) {
    uint64_t recv_req_id = WorkRequestManager::Get()->NewWorkRequest(
        WorkType::kRecv, recvbuf.addr(), recvbuf.size_in_bytes());
    auto wc = WorkCompletion::New(recv_req_id);
    recv_reqs_.Push(recv_req_id);
    return wc;
}

void TcpChannel::ReadCallback() {
    uint64_t recv_req_id = -1;
    if (closing_.load(std::memory_order_acquire)) {
        return;
    }
    if (!recv_reqs_.WaitAndPeek(recv_req_id,
                                std::chrono::milliseconds(kCommTimeoutMs))) {
        if (closing_.load(std::memory_order_acquire)) {
            return;
        }
        return;
    } 
    auto& recv_req = WorkRequestManager::Get()->GetWorkRequest(recv_req_id);
    if (this->error_detected()) {
        WorkRequestManager::Get()->set_status(recv_req_id, WorkStatus::kError);
        LOG_F(ERROR, "error detected %s", sys::GetLastErrorString().c_str());
        recv_req.Notify();
        recv_reqs_.Pop();
    }
    auto read_nbytes = sock_.Recv(
        recv_req.pointer_at<uint8_t>(recv_req.processed_bytes_upto_now()),
        recv_req.remain_nbytes());
    if (read_nbytes == -1 && errno != EAGAIN) {
        this->set_error_detected(true);
        LOG_F(ERROR, "error detected %s", sys::FormatError(errno).c_str());
        WorkRequestManager::Get()->set_status(recv_req.id(),
                                              WorkStatus::kError);
        recv_req.Notify();
        recv_reqs_.Pop();
    }
    if (recv_req.AddBytes(read_nbytes)) {
        WorkRequestManager::Get()->set_status(recv_req.id(),
                                              WorkStatus::kFinished);
        recv_req.Notify();
        recv_reqs_.Pop();
    }
    return;
}

void TcpChannel::WriteCallback() {
    uint64_t send_req_id = -1;
    if (!send_reqs_.WaitAndPeek(send_req_id,
                                std::chrono::milliseconds(kCommTimeoutMs))) {
        if (closing_.load(std::memory_order_acquire)) {
            return;
        }
        return;
    }
    auto& send_req = WorkRequestManager::Get()->GetWorkRequest(send_req_id);
    if (this->error_detected()) {
        WorkRequestManager::Get()->set_status(send_req_id, WorkStatus::kError);
        send_req.Notify();
        send_reqs_.Pop();
    }

    auto write_nbytes = sock_.Send(
        send_req.pointer_at<uint8_t>(send_req.processed_bytes_upto_now()),
        send_req.remain_nbytes());
    if (write_nbytes == -1 && errno != EAGAIN) {
        this->set_error_detected(true);
        WorkRequestManager::Get()->set_status(send_req.id(),
                                              WorkStatus::kError);
        send_req.Notify();
        send_reqs_.Pop();
    }
    if (send_req.AddBytes(write_nbytes)) {
        WorkRequestManager::Get()->set_status(send_req.id(),
                                              WorkStatus::kFinished);
        send_req.Notify();
        send_reqs_.Pop();
    }
    return;
}

void TcpChannel::DeleteEventOfInterest(const ChannelKind& kind) {
    mu_.lock();
    if (kind == ChannelKind::kRead) {
        if (this->kind() == ChannelKind::kReadWrite) {
            this->set_kind(ChannelKind::kWrite);
        } else if (this->kind() == ChannelKind::kRead) {
            this->set_kind(ChannelKind::kNone);
        } else {
            LOG_F(FATAL, "cannot delete %s from channel, currently of kind %s",
                  ChannelKindToString(kind).c_str(),
                  ChannelKindToString(this->kind()).c_str());
        }
    } else if (kind == ChannelKind::kWrite) {
        if (this->kind() == ChannelKind::kReadWrite) {
            this->set_kind(ChannelKind::kRead);
        } else if (this->kind() == ChannelKind::kWrite) {
            this->set_kind(ChannelKind::kNone);
        } else {
            LOG_F(FATAL, "cannot delete %s from channel, currently of kind %s",
                  ChannelKindToString(kind).c_str(),
                  ChannelKindToString(this->kind()).c_str());
        }
    } else if (kind == ChannelKind::kReadWrite) {
        if (this->kind() == ChannelKind::kReadWrite) {
            this->set_kind(ChannelKind::kNone);
        } else {
            LOG_F(FATAL, "cannot delete %s from channel, currently of kind %s",
                  ChannelKindToString(kind).c_str(),
                  ChannelKindToString(this->kind()).c_str());
        }
    }
    ModifyKind(this->kind());
    mu_.unlock();
}

void TcpChannel::AddEventOfInterest(const ChannelKind& kind) {
    if (closing_.load(std::memory_order_acquire)) {
        return;
    }
    mu_.lock();
    if (kind == ChannelKind::kRead) {
        if (this->kind() == ChannelKind::kNone) {
            this->set_kind(ChannelKind::kRead);
        } else if (this->kind() == ChannelKind::kWrite) {
            this->set_kind(ChannelKind::kReadWrite);
        } else {
            LOG_F(FATAL, "cannot add %s to channel, currently of kind %s",
                  ChannelKindToString(kind).c_str(),
                  ChannelKindToString(this->kind()).c_str());
        }
    } else if (kind == ChannelKind::kWrite) {
        if (this->kind() == ChannelKind::kNone) {
            this->set_kind(ChannelKind::kWrite);
        } else if (this->kind() == ChannelKind::kRead) {
            this->set_kind(ChannelKind::kReadWrite);
        } else {
            LOG_F(FATAL, "cannot add %s to channel, currently of kind %s",
                  ChannelKindToString(kind).c_str(),
                  ChannelKindToString(this->kind()).c_str());
        }
    } else if (kind == ChannelKind::kReadWrite) {
        if (this->kind() == ChannelKind::kNone) {
            this->set_kind(ChannelKind::kReadWrite);
        } else {
            LOG_F(FATAL, "cannot add %s to channel, currently of kind %s",
                  ChannelKindToString(kind).c_str(),
                  ChannelKindToString(this->kind()).c_str());
        }
    }
    ModifyKind(this->kind());
    mu_.unlock();
}
}  // namespace rdc
