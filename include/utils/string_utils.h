/**
 *  Copyright (c) 2018 by Contributors
 * @file   string_utils.h
 * @brief  string utilities
 */
#pragma once

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rdc {
namespace str_utils {
/*! @brief message buffer length */
const int kPrintBuffer = 1 << 12;

/*! @brief trim string from leftmost */
void Ltrim(std::string& s);

void Ltrim(std::string& str, const char& drop);

void Ltrim(std::string& str, const std::string& drop);

/*! @brief trim string from rightmost */
void Rtrim(std::string& s);

void Rtrim(std::string& str, const char& drop);

void Rtrim(std::string& str, const std::string& drop);
/*! @brief trim string from both ends */
void Trim(std::string& s);

void Trim(std::string& str, const char& drop);

void Trim(std::string& str, const std::string& drop);
/*! @brief c++11 version of string format */
std::string SPrintf(const char* fmt, ...);

int SScanf(const std::string& str, const char* fmt, ...);

template <typename Container>
std::string ConcatToString(const Container& container) {
    std::string str = "";
    for (const auto& item : container) {
        str += std::to_string(item);
        str += '\t';
    }
    return str;
}

bool StartsWith(const std::string& str, const std::string& prefix);

bool EndsWith(const std::string& str, const std::string& suffix);

std::vector<std::string> Split(const std::string& str, const char& sep = ' ');
}  // namespace str_utils
}  // namespace rdc
