#pragma once
#if !defined(__GXX_EXPERIMENTAL_CXX0X__) && !defined(_MSC_VER)                 \
    && _cplusplus < 201103L
#pragma error("c++11 features is not enabled with this compiler")
#endif
/// check if g++ is before 4.6
#if defined(__GNUC__) && !defined(__clang_version__)
#if __GNUC__ == 4 && __GNUC_MINOR__ < 6
#pragma error("Will need g++-4.6 is needed to enable c++11 features")
#endif
#endif


#ifdef _MSC_VER
#define XINLINE __forceinline
#pragma warning(disable : 4068)
#else
#define XINLINE inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#define noexcept_true throw ()
#define noexcept_false
#define noexcept(a) noexcept_##a
#endif

#define RDC_THROW_EXCEPTION noexcept(false)
#define RDC_NO_EXCEPTION  noexcept(true)

#ifdef _WIN32
#define LOGERROR(msg)                                                          \
    LOG_F(ERROR, "Socket %s Error:WSAError-code=%d", msg, WSAGetLastError();
#else
#define LOGERROR(msg)                                                          \
    LOG_F(ERROR, "Socket %s Error: %s", msg, strerror(errno));
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#define constexpr const
#define alignof __alignof
#endif

/*! \brief helper macro to generate string concat */
#define RDC_STR_CONCAT_(__x, __y) __x##__y
#define RDC_STR_CONCAT(__x, __y) RDC_STR_CONCAT_(__x, __y)


/*! \brief whether RTTI is enabled */
#ifndef RDC_ENABLE_RTTI
#define RDC_ENABLE_RTTI 1
#endif
