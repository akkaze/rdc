#include <cstring>
#include "transport/tcp/tcp_adapter.h"
#ifdef RDC_USE_RDMA
#include "transport/rdma/rdma_adapter.h"
#endif
#include "common/env.h"
#include "utils/utils.h"

namespace rdc {

std::tuple<std::string, std::string, uint32_t> ParseAddress(
    const std::string& addr_str) {
    auto addr_parts = str_utils::Split(addr_str, ':');
    std::string backend = addr_parts[0];
    std::string host = addr_parts[1];
    uint32_t port = std::atoi(addr_parts[2].c_str());
    return std::make_tuple(backend, host, port);
}

std::tuple<Backend, std::string, uint32_t> ParseAddr(
    const std::string& addr_str) {
    std::string backend_str;
    std::string host;
    uint32_t port;
    std::tie(backend_str, host, port) = ParseAddress(addr_str);
    Backend backend;
    if (backend_str == "tcp") {
        backend = kTcp;
    } else if (backend_str == "rdma") {
        backend = kRdma;
    } else if (backend_str == "ipc") {
        backend = kIpc;
    } else {
        backend = kTcp;
    }
    return std::make_tuple(backend, host, port);
}

std::string GetBackendString(const Backend& backend) {
    std::string backend_str;
    if (backend == kTcp) {
        backend_str = "tcp";
    } else if (backend == kRdma) {
        backend_str = "rdma";
    } else if (backend == kIpc) {
        backend_str = "ipc";
    } else {
        backend_str = "tcp";
    }
    return backend_str;
}
IAdapter* GetAdapter() {
    const char* backend = Env::Get()->Find("RDC_BACKEND");
    if (backend == nullptr) {
        return TcpAdapter::Get();
    }
    if (std::strncmp(backend, "TCP", 3) == 0) {
        return TcpAdapter::Get();
    }
#ifdef RDC_USE_RDMA
    if (std::strncmp(backend, "RDMA", 4) == 0) {
        return RdmaAdapter::Get();
    }
#endif
    return nullptr;
}
}  // namespace rdc
