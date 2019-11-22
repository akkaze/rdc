/*!
 * Copyright by Contributors
 * \file mpi.h
 * \brief stubs to be compatible with MPI
 *
 * \author Ankun Zheng
 */
#pragma once
namespace rdc {
namespace mpi {
/*!\brief enum of all operators */
enum OpType {
    kMax = 0,
    kMin = 1,
    kSum = 2,
    kBitwiseOR = 3
};
/*!\brief enum of supported data types */
enum DataType {
    kChar = 0,
    kUChar = 1,
    kInt = 2,
    kUInt = 3,
    kLong = 4,
    kULong = 5,
    kFloat = 6,
    kDouble = 7,
    kLongLong = 8,
    kULongLong = 9
};
// MPI data type to be compatible with existing MPI interface
class Datatype {
public:
    size_t type_size;
    explicit Datatype(size_t type_size) : type_size(type_size) {
    }
};

// template function to translate type to enum indicator
template<typename DType>
inline DataType GetType(void);
template<>
inline DataType GetType<char>(void) {
    return kChar;
}
template<>
inline DataType GetType<unsigned char>(void) {
    return kUChar;
}
template<>
inline DataType GetType<int>(void) {
    return kInt;
}
template<>
inline DataType GetType<unsigned int>(void) { // NOLINT(*)
    return kUInt;
}
template<>
inline DataType GetType<long>(void) {  // NOLINT(*)
    return kLong;
}
template<>
inline DataType GetType<unsigned long>(void) { // NOLINT(*)
    return kULong;
}
template<>
inline DataType GetType<float>(void) {
    return kFloat;
}
template<>
inline DataType GetType<double>(void) {
    return kDouble;
}
template<>
inline DataType GetType<long long>(void) { // NOLINT(*)
    return kLongLong;
}
template<>
inline DataType GetType<unsigned long long>(void) { // NOLINT(*)
    return kULongLong;
}
}  // namespace mpi

namespace op {
struct Max {
    static const mpi::OpType kType = mpi::kMax;
    template<typename DType>
    inline static void Reduce(DType &dst, const DType &src) { // NOLINT(*)
        if (dst < src) dst = src;
    }
};
struct Min {
    static const mpi::OpType kType = mpi::kMin;
    template<typename DType>
    inline static void Reduce(DType &dst, const DType &src) { // NOLINT(*)
        if (dst > src) dst = src;
    }
};
struct Sum {
    static const mpi::OpType kType = mpi::kSum;
    template<typename DType>
    inline static void Reduce(DType &dst, const DType &src) { // NOLINT(*)
        dst += src;
    }
};
struct BitOR {
    static const mpi::OpType kType = mpi::kBitwiseOR;
    template<typename DType>
    inline static void Reduce(DType &dst, const DType &src) { // NOLINT(*)
        dst |= src;
    }
};
template<typename OP, typename DType>
inline void Reducer(const void *src_, void *dst_, uint64_t len) {
    const DType *src = (const DType*)src_;
    DType *dst = (DType*)dst_;  // NOLINT(*)
    for (uint64_t i = 0U; i < len; ++i) {
        OP::Reduce(dst[i], src[i]);
    }
}
}  // namespace op
}  // namespace  rdc
