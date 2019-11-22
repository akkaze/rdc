#pragma once
#include "io/io.h"
namespace rdc {
class MemoryFixedSizeStream : public SeekStream {
public:
    MemoryFixedSizeStream() = default;

    MemoryFixedSizeStream(void *p_buffer, size_t buffer_size);

    virtual ~MemoryFixedSizeStream();

    size_t Read(void *ptr, size_t size);

    void Write(const void *ptr, size_t size);

    void Seek(size_t pos);
    size_t Tell();

    bool AtEnd() const;

    void *inner_buffer() const;

    size_t inner_buffer_size() const;

private:
    /*! @brief in memory buffer */
    char *p_buffer_;
    /*! @brief current pointer */
    size_t buffer_size_;
    /*! @brief current pointer */
    size_t curr_ptr_;
};  // class MemoryFixSizeStream

/**
 * @brief: a in memory buffer that can be read and write as stream interface
 */
class MemoryUnfixedSizeStream : public SeekStream {
public:
    MemoryUnfixedSizeStream() = default;

    explicit MemoryUnfixedSizeStream(std::string *p_buffer);
    virtual ~MemoryUnfixedSizeStream();

    size_t Read(void *ptr, size_t size);

    void Write(const void *ptr, size_t size);
    void Seek(size_t pos);

    size_t Tell(void);

    bool AtEnd(void) const;

private:
    /*! @brief in memory buffer */
    std::string *p_buffer_;
    /*! @brief current pointer */
    size_t curr_ptr_;
};  // class MemoryStreamStream
}  // namespace rdc
