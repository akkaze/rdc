#include "transport/tcp/tcp_adapter.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "common/status.h"
#include "common/threadpool.h"
#include "core/logging.h"
#include "sys/error.h"
#include "transport/tcp/tcp_channel.h"
static const uint32_t kNumMaxEvents = 1024;

namespace rdc {
static inline uint32_t channel_kind_to_epoll_event(
    const ChannelKind& channel_kind) {
    switch (channel_kind) {
        case ChannelKind::kRead:
            return EPOLLIN;
        case ChannelKind::kWrite:
            return EPOLLOUT;
        case ChannelKind::kReadWrite:
            return EPOLLIN | EPOLLOUT;
        case ChannelKind::kNone:
            return 0;
        default:
            return 0;
    }
}

static inline std::string channel_kind_to_string(ChannelKind channel_kind) {
    switch (channel_kind) {
        case ChannelKind::kRead:
            return "read";
        case ChannelKind::kWrite:
            return "write";
        case ChannelKind::kReadWrite:
            return "readwrite";
        case ChannelKind::kNone:
            return "none";
        default:
            return "none";
    }
}

static inline bool IsReadEvent(uint32_t events) {
    return (events & EPOLLIN || events & EPOLLPRI);
}
static inline bool IsWriteEvent(uint32_t events) {
    return events & EPOLLOUT;
}

static inline bool IsReadEventOnly(uint32_t events) {
    return IsReadEvent(events) && !IsWriteEvent(events);
}

static inline bool IsWriteEventOnly(uint32_t events) {
    return IsWriteEvent(events) && !IsReadEvent(events);
}
static inline bool IsReadEventWrite(uint32_t events) {
    return IsReadEvent(events) && IsWriteEvent(events);
}

static inline bool IsErrorEvent(uint32_t events) {
    return (events & EPOLLERR || events & EPOLLHUP || events & EPOLLRDHUP);
}
TcpAdapter::TcpAdapter() {
    this->set_backend(kTcp);
    this->listen_sock_ = TcpSocket();
    this->shutdown_called_ = false;
    this->shutdown_fd_ = -1;
    this->timeout_ = -1;
    this->epoll_fd_ = epoll_create(kNumMaxEvents);
    PollForever();
}

void TcpAdapter::PollForever() {
    loop_thrd = std::unique_ptr<std::thread>(new std::thread([this]() {
        logging::set_thread_name("tcppoller");
        LOG_F(2, "Tcp poller Started");
        while (true) {
            bool finished = Poll();
            if (finished)
                break;
        }
    }));
}
TcpAdapter::~TcpAdapter() {
    this->Shutdown();
    this->loop_thrd->join();
    CloseSocket(this->epoll_fd_);
    this->listen_sock_.Close();
}
void TcpAdapter::AddChannel(int32_t fd, TcpChannel* channel) {
    lock_.lock();
    channels_[fd] = channel;
    LOG_F(2, "Add new channel with fd : %d", fd);
    uint32_t flags = channel_kind_to_epoll_event(channel->kind());
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = fd;
    ev.events |= flags;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    lock_.unlock();
}

void TcpAdapter::AddChannel(TcpChannel* channel) {
    this->AddChannel(channel->sockfd(), channel);
}

void TcpAdapter::RemoveChannel(TcpChannel* channel) {
    lock_.lock();
    channels_.erase(channel->sockfd());
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, channel->sockfd(), nullptr);
    lock_.unlock();
}

void TcpAdapter::ModifyChannel(TcpChannel* channel,
                               const ChannelKind& target_kind) {
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.data.fd = channel->sockfd();
    uint32_t flags = channel_kind_to_epoll_event(target_kind);
    ev.events |= flags;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, channel->sockfd(), &ev);
}

void TcpAdapter::Shutdown() {
    shutdown_lock_.lock();
    if (!this->shutdown_called_) {
        this->shutdown_called_ = true;
        int pipe_fd[2];
        pipe(pipe_fd);
        int flags = EPOLLIN;
        this->shutdown_fd_ = pipe_fd[0];
        epoll_event ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.data.fd = shutdown_fd_;
        ev.events |= flags;
        epoll_ctl(this->epoll_fd_, EPOLL_CTL_ADD, this->shutdown_fd_, &ev);
        write(pipe_fd[1], "shutdown", 9);
    }
    shutdown_lock_.unlock();
}
/*!
 * /brief Function which processes the events from epoll_wait and calls the
 * appropriate callbacks, note only process events once if you need to use an
 * event loop use TcpAdapter_loop
 * /return whether poll finished
 */
bool TcpAdapter::Poll() {
    epoll_event events[kNumMaxEvents];
    int fds =
        epoll_wait(this->epoll_fd_, events, kNumMaxEvents, this->timeout_);
    for (int i = 0; i < fds; i++) {
        TcpChannel* channel = nullptr;
        lock_.lock();
        channel = this->channels_[events[i].data.fd];
        lock_.unlock();
        if (channel) {
            // shutdown or error
            if (IsErrorEvent(events[i].events)) {
                int32_t error = GetLastSocketError(events[i].data.fd);
                channel->set_error_detected(true);
                LOG_F(ERROR, "%s", sys::FormatError(error).c_str());
                return true;
            }

            // when data avaliable for read or urgent flag is set
            if (IsReadEvent(events[i].events)) {
                channel->DeleteEventOfInterest(ChannelKind::kRead);
                ThreadPool::Get()->AddTask([channel, this] {
                    channel->ReadCallback();
                    this->shutdown_lock_.lock();
                    if (!this->shutdown_called_) {
                        CHECK_NOTNULL(channel);
                        channel->AddEventOfInterest(ChannelKind::kRead);
                    }
                    this->shutdown_lock_.unlock();
                });
            }

            // when write possible
            if (IsWriteEvent(events[i].events)) {
                channel->DeleteEventOfInterest(ChannelKind::kWrite);
                ThreadPool::Get()->AddTask(
                    [channel] { channel->WriteCallback(); });
            }
        } else {  // shutdown pipe
            if (IsReadEvent(events[i].events)) {
                if (events[i].data.fd == this->shutdown_fd_) {
                    this->shutdown_ = true;
                }
            }
        }
    }  // for
    if (this->shutdown_) {
        VLOG_F(2, "Shutdown adapter");
        return true;
    }
    return false;
}
void TcpAdapter::Listen(const int& port) {
    VLOG_F(3, "Listening on port %d ", port);
    listen_sock_.TryBindHost(port);
    listen_sock_.SetReuseAddr(true);
    listen_sock_.Listen(kNumBacklogs);
    return;
}

IChannel* TcpAdapter::Accept() {
    // accept the connection
    // set flags to check
    VLOG_F(3, "Accpet a new connection");
    const auto& sock = listen_sock_.Accept();
    return new TcpChannel(this, sock, ChannelKind::kRead);
}
}  // namespace rdc
