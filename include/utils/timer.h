/*!
 * Copyright by Contributors
 * \file timer.h
 * \brief This file defines the utils for timing
 * \author AnKun
 */
#pragma once
#include <chrono>
#include "utils/utils.h"

namespace rdc {
namespace utils {
/*!
 * \brief return time in microseconds
 */
inline uint64_t GetTimeInUs() {
    using namespace std::chrono;
    return duration_cast<microseconds>(high_resolution_clock::now()
            .time_since_epoch()).count();
}
inline double GetTimeInMs() {
    return static_cast<double>(GetTimeInUs()) / 1e3;
}

inline double GetTime() {
    return static_cast<double>(GetTimeInUs()) / 1e6;
}
}  // namespace utils
}  // namespace rdc
