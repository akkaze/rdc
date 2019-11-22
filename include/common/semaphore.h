#pragma once

#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <memory>
#include <type_traits>
#if defined(_WIN32)
// Avoid including windows.h in a header; we only need a handful of
// items, so we'll redeclare them here (this is relatively safe since
// the API generally has to remain stable between Windows versions).
// I know this is an ugly hack but it still beats polluting the global
// namespace with thousands of generic names or adding a .cpp for nothing.
extern "C" {
struct _SECURITY_ATTRIBUTES;
__declspec(dllimport) void* __stdcall CreateSemaphoreW(
    _SECURITY_ATTRIBUTES* lpSemaphoreAttributes, long lInitialCount,
    long lMaximumCount, const wchar_t* lpName);
__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    void* hHandle, unsigned long dwMilliseconds);
__declspec(dllimport) int __stdcall ReleaseSemaphore(void* hSemaphore,
                                                     long lReleaseCount,
                                                     long* lpPreviousCount);
}
#elif defined(__MACH__)
#include <mach/mach.h>
#elif defined(__unix__)
#include <semaphore.h>
#include <fcntl.h>
#endif

// portable + lightweight semaphore implementations, originally from
// https://github.com/preshing/cpp11-on-multicore/blob/master/common/sema.h
// LICENSE:
// Copyright (c) 2015 Jeff Preshing
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//	claim that you wrote the original software. If you use this software
//	in a product, an acknowledgement in the product documentation would be
//	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
#if defined(_WIN32)
class Semaphore {
private:
    void* m_hSema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initial_count = 0);

    ~Semaphore();

    void Wait();

    bool TryWait();

    bool TimedWait(std::uint64_t usecs);

    void signal(int count = 1);
};
#elif defined(__MACH__)
//---------------------------------------------------------
// Semaphore (Apple iOS and OSX)
// Can't use POSIX semaphores due to
//---------------------------------------------------------
class Semaphore {
private:
    semaphore_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initial_count = 0);

    ~Semaphore();

    void Wait();

    bool TryWait();

    bool TimedWait(std::uint64_t timeout_usecs);

    void Signal();

    void Signal(int count);
};
#elif defined(__unix__)
//---------------------------------------------------------
// Semaphore (POSIX, Linux)
//---------------------------------------------------------
class Semaphore {
private:
    sem_t* m_sema;
    bool m_alloc_sema;
    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

public:
    Semaphore(int initial_count = 0);

    Semaphore(const std::string& name, int initial_count = 0);

    ~Semaphore();

    void Wait();

    bool TryWait();

    bool TimedWait(std::uint64_t usecs);

    void Signal();

    void Signal(int count);
};
#else
#error Unsupported platform! (No semaphore wrapper available)
#endif

//---------------------------------------------------------
// LightweightSemaphore
//---------------------------------------------------------
class LightweightSemaphore {
public:
    typedef std::make_signed<std::size_t>::type ssize_t;

private:
    std::atomic<ssize_t> m_count;
    Semaphore m_sema;

    bool WaitWithPartialSpinning(std::int64_t timeout_usecs = -1);

    ssize_t WaitManyWithPartialSpinning(ssize_t max,
                                        std::int64_t timeout_usecs = -1);

public:
    LightweightSemaphore(ssize_t initial_count = 0);

    bool TryWait();

    void Wait();

    bool Wait(std::int64_t timeout_usecs);

    // Acquires between 0 and (greedily) max, inclusive
    ssize_t TryWaitMany(ssize_t max);

    // Acquires at least one, and (greedily) at most max
    ssize_t WaitMany(ssize_t max, std::int64_t timeout_usecs);

    ssize_t WaitMany(ssize_t max);

    void Signal(ssize_t count = 1);

    ssize_t AvailableApprox() const;
};
