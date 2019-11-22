#include <cerrno>
#include <cstring>
#include <string>
namespace rdc {
namespace sys {
int GetLastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

std::string FormatError(const int& err_code) {
#ifdef _WIN32
    char* s = nullptr;
    DWORD err_len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&s,
        0, NULL);
    std::string err_str(err_len, 0);
    std::memcpy(const_cast<char*>(err_len.c_str()), s, err_len);
    return err_str;
#else
    return std::strerror(err_code);
#endif
}

std::string GetLastErrorString() { return FormatError(GetLastError()); }
}  // namespace sys
}  // namespace rdc
