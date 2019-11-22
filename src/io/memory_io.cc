#include "io/memory_io.h"

namespace rdc {
MemoryFixedSizeStream::MemoryFixedSizeStream(void *p_buffer, size_t buffer_size)
    : p_buffer_(reinterpret_cast<char *>(p_buffer)), buffer_size_(buffer_size) {
    curr_ptr_ = 0;
}
MemoryFixedSizeStream::~MemoryFixedSizeStream(void) {
}
size_t MemoryFixedSizeStream::Read(void *ptr, size_t size) {
    CHECK_F(curr_ptr_ + size <= buffer_size_,
            "read can not have position excceed buffer length");
    size_t nread = std::min(buffer_size_ - curr_ptr_, size);
    if (nread != 0)
        std::memcpy(ptr, p_buffer_ + curr_ptr_, nread);
    curr_ptr_ += nread;
    return nread;
}
void MemoryFixedSizeStream::Write(const void *ptr, size_t size) {
    if (size == 0)
        return;
    CHECK_F(curr_ptr_ + size <= buffer_size_,
            "write position exceed fixed buffer size");
    std::memcpy(p_buffer_ + curr_ptr_, ptr, size);
    curr_ptr_ += size;
}

void MemoryFixedSizeStream::Seek(size_t pos) {
    curr_ptr_ = static_cast<size_t>(pos);
}

size_t MemoryFixedSizeStream::Tell() {
    return curr_ptr_;
}

bool MemoryFixedSizeStream::AtEnd() const {
    return curr_ptr_ == buffer_size_;
}

void* MemoryFixedSizeStream::inner_buffer() const {
    return p_buffer_;
}

size_t MemoryFixedSizeStream::inner_buffer_size() const {
    return buffer_size_;
}

MemoryUnfixedSizeStream::MemoryUnfixedSizeStream(std::string *p_buffer)
    : p_buffer_(p_buffer) {
    curr_ptr_ = 0;
}

MemoryUnfixedSizeStream::~MemoryUnfixedSizeStream(void) {
}

size_t MemoryUnfixedSizeStream::Read(void *ptr, size_t size) {
    CHECK_F(curr_ptr_ <= p_buffer_->length(),
            "read can not have position excceed buffer length");
    size_t nread = std::min(p_buffer_->length() - curr_ptr_, size);
    if (nread != 0)
        std::memcpy(ptr, &(*p_buffer_)[0] + curr_ptr_, nread);
    curr_ptr_ += nread;
    return nread;
}

void MemoryUnfixedSizeStream::Write(const void *ptr, size_t size) {
    if (size == 0)
        return;
    if (curr_ptr_ + size > p_buffer_->length()) {
        p_buffer_->resize(curr_ptr_ + size);
    }
    std::memcpy(&(*p_buffer_)[0] + curr_ptr_, ptr, size);
    curr_ptr_ += size;
}

void MemoryUnfixedSizeStream::Seek(size_t pos) {
    curr_ptr_ = static_cast<size_t>(pos);
}

size_t MemoryUnfixedSizeStream::Tell(void) {
    return curr_ptr_;
}

bool MemoryUnfixedSizeStream::AtEnd(void) const {
    return curr_ptr_ == p_buffer_->length();
}

}  // namespace rdc
