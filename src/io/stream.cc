#include "io/io.h"
#include "io/stream.h"
namespace rdc {
// implementations for ostream
void ostream::OutBuf::set_stream(Stream *stream) {
    if (stream_ != NULL) this->pubsync();
    this->stream_ = stream;
    this->setp(&buffer_[0], &buffer_[0] + buffer_.size() - 1);
}

int ostream::OutBuf::sync(void) {
    if (stream_ == NULL) return -1;
    std::ptrdiff_t n = pptr() - pbase();
    stream_->Write(pbase(), n);
    this->pbump(-static_cast<int>(n));
    bytes_out_ += n;
    return 0;
}

int ostream::OutBuf::overflow(int c) {
    *(this->pptr()) = c;
    std::ptrdiff_t n = pptr() - pbase();
    this->pbump(-static_cast<int>(n));
    if (c == EOF) {
        stream_->Write(pbase(), n);
        bytes_out_ += n;
    } else {
        stream_->Write(pbase(), n + 1);
        bytes_out_ += n + 1;
    }
    return c;
}

// implementations for istream
void istream::InBuf::set_stream(Stream *stream) {
    stream_ = stream;
    this->setg(&buffer_[0], &buffer_[0], &buffer_[0]);
}
int istream::InBuf::underflow() {
    char *bhead = &buffer_[0];
    if (this->gptr() == this->egptr()) {
        size_t sz = stream_->Read(bhead, buffer_.size());
        this->setg(bhead, bhead, bhead + sz);
        bytes_read_ += sz;
    }
    if (this->gptr() == this->egptr()) {
        return traits_type::eof();
    } else {
        return traits_type::to_int_type(*gptr());
    }
}

}  // namespace rdc
