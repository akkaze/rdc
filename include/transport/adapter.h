#pragma once
#include <string>
#include <tuple>
#include "core/logging.h"
#include "utils/string_utils.h"
namespace rdc {

const uint32_t kNumBacklogs = 128;

enum Backend {
    kTcp = 0,
    kRdma = 2,
    kIpc = 3,
};
/* !\brief parse a address before connecting, a valid address is represented
 * as a tuple (backend, host, port) and will be represented as a string like
 * "tcp:127.0.0.1:8000"*/
std::tuple<std::string, std::string, uint32_t> ParseAddress(
    const std::string& addr_str);

std::tuple<Backend, std::string, uint32_t> ParseAddr(
    const std::string& addr_str);

std::string GetBackendString(const Backend& backend);

class IChannel;
class IAdapter {
public:
    virtual IChannel* Accept() = 0;
    virtual void Listen(const int& port) = 0;
    void set_backend(const Backend& backend) {
        backend_ = backend;
    }
    Backend backend() const {
        return backend_;
    }
    std::string backend_str() const {
        return GetBackendString(backend_);
    }

private:
    Backend backend_;
};

IAdapter* GetAdapter();
}  // namespace rdc
