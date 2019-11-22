#pragma once
namespace rdc {
namespace sys {
int GetLastError();

std::string FormatError(const int& err_code);

std::string GetLastErrorString();
}  // namespace sys
}  // namespace rdc
