#include "comm/checkpointer.h"
#include "comm/communicator_manager.h"

namespace rdc {
namespace comm {
CheckPointer::CheckPointer() {
    comm_ = CommunicatorManager::Get()->NewCommunicator("CheckPoint");
}
std::unordered_map<std::string, GlobalState> CheckPointer::states() const {
    return global_states_;
}
/**
 * @brief: move state the the government of checkpointer
 *
 * @tparam StateTy state type, either LocalState or Global State
 * @param state state to be appended
 */
void CheckPointer::AddGlobalState(const std::string name,
                                  const GlobalState& global_state) {
    global_states_[name] = global_state;
}

void CheckPointer::AddGlobalState(const std::string name, void* ptr,
                                  size_t size) {
    GlobalState global_state(name, ptr, size);
    global_states_[name] = global_state;
}
void CheckPointer::AddLocalState(const std::string name,
                                 const LocalState& local_state) {
    local_states_[name] = local_state;
}

void CheckPointer::AddLocalState(const std::string name, void* ptr,
                                 size_t size) {
    LocalState local_state(name, ptr, size);
    local_states_[name] = local_state;
}
void CheckPointer::CheckPoint() {
    for (auto&& global_state_with_name : global_states_) {
        global_state_with_name.second.DoCheckPoint();
    }
    for (auto&& local_state_with_name : local_states_) {
        local_state_with_name.second.DoCheckPoint();
    }
}

int CheckPointer::LoadCheckPoint() {
    for (auto&& global_state_with_name : global_states_) {
        global_state_with_name.second.LoadCheckPoint();
    }
    for (auto&& local_state_with_name : local_states_) {
        local_state_with_name.second.LoadCheckPoint();
    }
    return 0;
}

}  // namespace comm
}  // namespace rdc
