/*!
 *  Copyright (c) 2014 by Contributors
 * \file utils.h
 * \brief simple utils to support the code
 * \author Ankun Zheng
 */
#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#ifdef DEBUG
#include "core/logging.h"
#endif
namespace rdc {
/*! \brief namespace for helper utils of the project */
namespace utils {

// easy utils that can be directly accessed in xgboost
/*! \brief get the beginning address of a vector */
template <typename T>
inline T* BeginPtr(std::vector<T>& vec) {  // NOLINT(*)
    if (vec.size() == 0) {
        return nullptr;
    } else {
        return &vec[0];
    }
}
/*! \brief get the beginning address of a vector */
template <typename T>
inline const T* BeginPtr(const std::vector<T>& vec) {  // NOLINT(*)
    if (vec.size() == 0) {
        return nullptr;
    } else {
        return &vec[0];
    }
}
inline char* BeginPtr(std::string& str) {  // NOLINT(*)
    if (str.length() == 0)
        return nullptr;
    return &str[0];
}
inline const char* BeginPtr(const std::string& str) {
    if (str.length() == 0)
        return nullptr;
    return &str[0];
}
inline void* IncrVoidPtr(void* ptr, size_t step) {
    return reinterpret_cast<void*>(reinterpret_cast<int8_t*>(ptr) + step);
}
inline const void* IncrConstVoidPtr(const void* ptr, size_t step) {
    return reinterpret_cast<const void*>(
        reinterpret_cast<int8_t*>(const_cast<void*>(ptr)) + step);
}
/*! divide a range approximately into equally parts */
inline std::vector<std::pair<int, int>> Split(int begin, int end, int nparts) {
    std::vector<std::pair<int, int>> ranges(nparts);
    int len = end - begin;
    int k = len / nparts;
    int m = len % nparts;
    for (int i = 0; i < nparts; i++) {
        int rbegin = begin + i * k + std::min(i, m);
        int rend = begin + (i + 1) * k + std::min(i + 1, m);
        ranges[i] = std::make_pair(rbegin, rend);
    }
    return ranges;
}

/*! brief alloc a chunk of memory*/
inline void* AllocTemp(const uint64_t& nbytes) {
    void* buf = std::malloc(nbytes);
    return buf;
}
/*! brief warpper of free functon from stdc */
inline void Free(void* ptr) {
    return std::free(ptr);
}

/*! brief zero a chunk of memory */
inline void ZeroBuf(void* buf, size_t nbytes) {
    std::memset(buf, 0, nbytes);
    return;
}
template <typename T, typename... Args>
inline std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#ifdef DEBUG
#define PeekFirstElementAsChar(addr) \
    LOG(INFO) << static_cast<const char*>(addr)[0];
#endif
}  // namespace utils
}  // namespace rdc
