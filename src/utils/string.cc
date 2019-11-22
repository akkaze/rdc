#include "utils/string_utils.h"
namespace rdc {
namespace str_utils {
void Trim(std::string& str) {
    Ltrim(str);
    Rtrim(str);
}

void Trim(std::string& str, const char& drop) {
    Ltrim(str, drop);
    Rtrim(str, drop);
}

void Trim(std::string& str, const std::string& drop) {
    Ltrim(str, drop);
    Rtrim(str, drop);
}

void Ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](const char& ch) {
                return !std::isspace(ch);
            }));
}

void Ltrim(std::string& s, const std::string& drop) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [drop](const char& ch) {
                for (auto&& drop_item : drop) {
                    if (ch == drop_item) {
                        return true;
                    }
                }
                return false;
            }));
}

void Ltrim(std::string& s, const char& drop) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [drop](const char& ch) {
                return ch != drop;
            }));
}

void Rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](const char& ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

void Rtrim(std::string& s, const char& drop) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [drop](const char& ch) { return ch != drop; })
                .base(),
            s.end());
}

void Rtrim(std::string& s, const std::string& drop) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [drop](const char& ch) {
                             for (auto&& drop_item : drop) {
                                 if (ch == drop_item) {
                                     return true;
                                 }
                             }
                             return false;
                         })
                .base(),
            s.end());
}

std::string SPrintf(const char* fmt, ...) {
    std::string msg(kPrintBuffer, '\0');
    va_list args;
    va_start(args, fmt);
    vsnprintf(&msg[0], kPrintBuffer, fmt, args);
    va_end(args);
    Rtrim(msg, '\0');
    return msg;
}

int SScanf(const std::string& str, const char* fmt, ...) {
    char* cstr = new char[str.size() + 1];
    std::memset(cstr, 0, str.size() + 1);
    std::memcpy(cstr, str.c_str(), str.size());
    va_list args;
    va_start(args, fmt);
    auto ret = vsscanf(cstr, fmt, args);
    va_end(args);
    delete[] cstr;
    return ret;
}

bool StartsWith(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) {
        return false;
    }
    auto len = prefix.size();
    for (auto i = 0U; i < len; i++) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

bool EndsWith(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) {
        return false;
    }
    auto str_len = str.size();
    auto suf_len = suffix.size();
    for (auto i = 0U; i < suf_len; i++) {
        if (str[str_len - i - 1] != suffix[suf_len - i - 1]) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> Split(const std::string& str, const char& sep) {
    std::string::size_type begin = 0;
    std::vector<std::string> result;

    while ((begin = str.find_first_not_of(sep, begin)) != std::string::npos) {
        auto end = str.find_first_of(sep, begin);
        result.push_back(str.substr(begin, end - begin));
        begin = end;
    }
    return result;
}

}  // namespace str_utils
}  // namespace rdc
