#include "transport/ipc/ipc_channel.h"
#include "comm/tracker.h"
#include "transport/ipc/ipc_adapter.h"
#include "transport/ipc/ipc_utils.h"

namespace rdc {
WorkCompletion* IpcChannel::ISend(Buffer sendbuf) {
    uint64_t send_req_id = WorkRequestManager::Get()->NewWorkRequest(
        WorkType::kSend, sendbuf.addr(), sendbuf.size_in_bytes());
    auto wc = WorkCompletion::New(send_req_id);
    auto& send_req = WorkRequestManager::Get()->GetWorkRequest(send_req_id);

    char memfile_name_cstr[kMemFileNameSize];
    std::memset(memfile_name_cstr, 0, kMemFileNameSize);
    int peer_rank = this->peer_rank();
    int rank = comm::Tracker::Get()->rank();

    std::string memfile_name =
        compose_memfile_name(rank, peer_rank, send_counter_++);

    std::memcpy(memfile_name_cstr, memfile_name.c_str(), memfile_name.size());
    
    adapter_->SetPrivData(rank, peer_rank, memfile_name_cstr);

    auto shm =
        std::make_shared<Shmem>(memfile_name_cstr, send_req.size_in_bytes());
    shm->Create();
    std::memcpy(shm->Data(), send_req.pointer_at<uint8_t>(0),
                send_req.size_in_bytes());

    WorkRequestManager::Get()->set_status(send_req.id(), WorkStatus::kFinished);
    send_req.Notify();
    return wc;
}

WorkCompletion* IpcChannel::IRecv(Buffer recvbuf) {
    uint64_t recv_req_id = WorkRequestManager::Get()->NewWorkRequest(
        WorkType::kRecv, recvbuf.addr(), recvbuf.size_in_bytes());
    auto wc = WorkCompletion::New(recv_req_id);
    auto& recv_req = WorkRequestManager::Get()->GetWorkRequest(recv_req_id);

    int peer_rank = this->peer_rank();
    int rank = comm::Tracker::Get()->rank();

    char memfile_name_cstr[kMemFileNameSize];
    adapter_->SetPrivData(rank, peer_rank, memfile_name_cstr);

    //auto&& info info_from_memfile_name(memfile_name);
    auto shm =
        std::make_shared<Shmem>(memfile_name_cstr, recv_req.size_in_bytes());
    shm->Open();
    recvbuf.set_addr(shm->Data());
    recvbuf.set_shm(shm);
    WorkRequestManager::Get()->set_status(recv_req.id(), WorkStatus::kFinished);
    recv_req.Notify();
    return wc;
}

void IpcChannel::Close() {
    return;
}

bool IpcChannel::Connect(const std::string& host, const uint32_t& port) {
    return true;
}

}  // namespace rdc
