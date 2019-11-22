#include "comm/communicator_manager.h"
#include "comm/communicator_robust.h"
#include "comm/tracker.h"
#include "common/threadpool.h"
#include "transport/tcp/tcp_adapter.h"

namespace rdc {
namespace comm {

std::atomic<CommunicatorManager*> CommunicatorManager::instance(nullptr);
std::atomic<bool> CommunicatorManager::created(false);
std::mutex CommunicatorManager::create_mutex;
// util to parse data with unit suffix
inline size_t ParseUnit(const char* name, const char* val) {
    char unit;
    unsigned long amt;  // NOLINT(*)
    int n = sscanf(val, "%lu%c", &amt, &unit);
    size_t amount = amt;
    if (n == 2) {
        switch (unit) {
            case 'B':
                return amount;
            case 'K':
                return amount << 10UL;
            case 'M':
                return amount << 20UL;
            case 'G':
                return amount << 30UL;
            default:
                LOG_F(ERROR, "invalid format for %s", name);
                return 0;
        }
    } else if (n == 1) {
        return amount;
    } else {
        LOG_F(ERROR,
              "invalid format for %s,"
              "shhould be {integer}{unit}, unit can be {B, KB, MB, GB}",
              name);
        return 0;
    }
}

CommunicatorManager::CommunicatorManager() {
    // 32 K items
    reduce_ring_mincount_ = 1;
    // reduce_ring_mincount_ = 1 << 15;

    // setup possible enviroment variable of intrest
    env_vars_.push_back("rdc_reduce_buffer");
    env_vars_.push_back("rdc_reduce_ring_mincount");
    env_vars_.push_back("RDC_NUM_ATTEMPT");
    env_vars_.push_back("RDC_TRACKER_URI");
    env_vars_.push_back("RDC_TRACKER_PORT");
    env_vars_.push_back("RDC_HEARTBEAT_INTERVAL");
    env_vars_.push_back("RDC_RESTART");
    env_vars_.push_back("WORKER_CONNECT_RETRY");
    this->SetParam("rdc_reduce_buffer", "256MB");
}
CommunicatorManager* CommunicatorManager::Get() {
    bool created_ = created.load(std::memory_order_relaxed);
    CommunicatorManager* tmp = instance.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if (created_ == false) {
        std::lock_guard<std::mutex> lock(create_mutex);
        created_ = created.load(std::memory_order_relaxed);
        if (created_ == false) {
            tmp = new CommunicatorManager;
            std::atomic_thread_fence(std::memory_order_release);
            instance.store(tmp, std::memory_order_relaxed);
            created.store(true, std::memory_order_relaxed);
        }
    }
    return tmp;
}

void CommunicatorManager::Release() {
    bool created_ = false;
    while ((created_ = created.load(std::memory_order_acquire)) == false) {
        continue;
    }
    auto&& instance_ = instance.load(std::memory_order_relaxed);
    delete instance_;
    instance.store(nullptr, std::memory_order_relaxed);
}

void CommunicatorManager::Init(int argc, char** argv) {
    // setup from enviroment variables
    // handler to get variables from env
    LOG_F(INFO, "Initialize");
    for (size_t i = 0; i < env_vars_.size(); ++i) {
        const char* value = Env::Get()->Find(env_vars_[i].c_str());
        if (value != nullptr) {
            LOG_F(INFO, "%s %s", env_vars_[i].c_str(), value);
            this->SetParam(env_vars_[i].c_str(), value);
        }
    }
    // pass in arguments override env variable.
    for (int i = 0; i < argc; ++i) {
        char name[256], val[256];
        if (sscanf(argv[i], "%[^=]=%s", name, val) == 2) {
            this->SetParam(name, val);
        }
    }

    // note tracker will be connected from here
    logging::set_thread_name("communicator manager");
    logging::add_file(
        str_utils::SPrintf("log/%d", Tracker::Get()->rank()).c_str(),
        logging::Truncate, logging::Verbosity_MAX);
    logging::g_stderr_verbosity = 1;
    deamon_.reset(new Deamon);

    checkpointer_.reset(new CheckPointer);
}

void CommunicatorManager::Finalize() {
    for (auto&& comm : communicators_) {
        if (comm.second->name() == kMainCommName) {
            comm.second->Shutdown();
        }
    }

    TcpAdapter::Get()->Shutdown();
    Tracker::Get()->set_tracker_connected(false);
    deamon_->Shutdown();
    Tracker::Get()->Release();
}

void CommunicatorManager::ResetAllCommunicators() {
    for (auto&& comm_name : comm_names_) {
        communicators_[comm_name]->ResetLinks();
    }
    for (auto&& comm_name : comm_names_) {
        communicators_[comm_name]->ReConnectLinks(std::make_tuple(
            Tracker::Get()->num_conn(), Tracker::Get()->num_accept()));
    }
}

void CommunicatorManager::SetParam(const char* name, const char* val) {
    if (!strcmp(name, "RDC_TRACKER_URI")) {
        this->tracker_uri_ = val;
    }
    if (!strcmp(name, "RDC_TRACKER_PORT")) {
        this->tracker_port_ = atoi(val);
    }
    if (!strcmp(name, "RDC_HEARTBEAT_INTERVAL")) {
        this->heartbeat_interval_ = atoi(val);
    }
    if (!strcmp(name, "RDC_RESTART")) {
        this->set_restart(atoi(val));
    }
    if (!strcmp(name, "rdc_world_size")) {
        this->world_size_ = atoi(val);
    }
    if (!strcmp(name, "rdc_reduce_ring_mincount")) {
        this->reduce_ring_mincount_ = ParseUnit(name, val);
    }
    if (!strcmp(name, "RDC_WORKER_CONNECT_RETRY")) {
        this->connect_retry_ = atoi(val);
    }
}

std::shared_ptr<ICommunicator> CommunicatorManager::NewCommunicator(
    const std::string& name) {
    comm_names_.emplace_back(name);
    // increase volumn of threadpool
    if (GetAdapter()->backend() == kTcp) {
        ThreadPool::Get()->AddWorkers(Env::Get()->GetEnv("RDC_NUM_WORKERS", 0));
    }
    std::unique_lock<utils::SpinLock> comm_lock(comm_lock_);
    if (communicators_.count(name)) return nullptr;
    comm_lock.unlock();
    auto comm = std::make_shared<Communicator>();
    comm->set_name(name);
    // std::unique_lock<utils::SpinLock> lock(*tracker_lock_);
    while (!Tracker::Get()->tracker_connected()) {
        continue;
    }
    // connection in current communicator
    comm->Init(Tracker::Get()->world_size(), Tracker::Get()->num_conn(),
               Tracker::Get()->num_accept());
    //    conn_lock_.unlock();
    // add this communicator to the goverment of main communicator
    comm_lock.lock();
    this->communicators_[name] = comm;
    return comm;
}

ICommunicator* CommunicatorManager::GetCommunicator(const std::string& name) {
    CHECK(communicators_.count(name));
    return communicators_[name].get();
}

std::unordered_map<std::string, std::shared_ptr<ICommunicator>>
CommunicatorManager::communicators() const {
    return communicators_;
}

void CommunicatorManager::AddCommunicator(
    const std::string& name,
    const std::shared_ptr<ICommunicator>& communicator) {
    communicators_[name] = communicator;
}

void CommunicatorManager::AddGlobalState(const std::string& name, void* ptr,
                                         size_t size) {
    checkpointer_->AddGlobalState(name, ptr, size);
}
void CommunicatorManager::AddLocalState(const std::string& name, void* ptr,
                                        size_t size) {
    checkpointer_->AddLocalState(name, ptr, size);
}

void CommunicatorManager::CheckPoint() {
    checkpointer_->CheckPoint();
}

int CommunicatorManager::LoadCheckPoint() {
    return checkpointer_->LoadCheckPoint();
}
}  // namespace comm
}  // namespace rdc
