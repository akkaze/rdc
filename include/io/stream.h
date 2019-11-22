#pragma once

#include <istream>
#include <ostream>
#include <streambuf>

namespace rdc {
class Stream;
/*!
 * @brief a std::ostream class that can can wrap Stream objects,
 *  can use ostream with that output to underlying Stream
 *
 */
class ostream : public std::basic_ostream<char> {
public:
    /*!
     * @brief construct std::ostream type
     * @param stream the Stream output to be used
     * @param buffer_size internal streambuf size
     */
    explicit ostream(Stream *stream, size_t buffer_size = (1 << 10))
        : std::basic_ostream<char>(NULL), buf_(buffer_size) {
        this->set_stream(stream);
    }
    // explictly synchronize the buffer
    virtual ~ostream() {
        buf_.pubsync();
    }
    /*!
     * @brief set internal stream to be stream, reset states
     * @param stream new stream as output
     */
    void set_stream(Stream *stream) {
        buf_.set_stream(stream);
        this->rdbuf(&buf_);
    }

    /*! @return how many bytes we written so far */
    size_t bytes_written(void) const {
        return buf_.bytes_out();
    }

private:
    // internal streambuf
    class OutBuf : public std::streambuf {
    public:
        explicit OutBuf(size_t buffer_size)
            : stream_(NULL), buffer_(buffer_size), bytes_out_(0) {
            if (buffer_size == 0) buffer_.resize(2);
        }
        // set stream to the buffer
        void set_stream(Stream *stream);

        size_t bytes_out() const {
            return bytes_out_;
        }

    private:
        /*! @brief internal stream by StreamBuf */
        Stream *stream_;
        /*! @brief internal buffer */
        std::vector<char> buffer_;
        /*! @brief number of bytes written so far */
        size_t bytes_out_;
        // override sync
        int_type sync(void);
        // override overflow
        int_type overflow(int c);
    };
    /*! @brief buffer of the stream */
    OutBuf buf_;
};

/*!
 * @brief a std::istream class that can can wrap Stream objects,
 *  can use istream with that output to underlying Stream
 */
class istream : public std::basic_istream<char> {
public:
    /*!
     * @brief construct std::ostream type
     * @param stream the Stream output to be used
     * @param buffer_size internal buffer size
     */
    explicit istream(Stream *stream, size_t buffer_size = (1 << 10))
        : std::basic_istream<char>(NULL), buf_(buffer_size) {
        this->set_stream(stream);
    }
    virtual ~istream() {
    }
    /*!
     * @brief set internal stream to be stream, reset states
     * @param stream new stream as output
     */
    void set_stream(Stream *stream) {
        buf_.set_stream(stream);
        this->rdbuf(&buf_);
    }
    /*! @return how many bytes we read so far */
    size_t bytes_read(void) const {
        return buf_.bytes_read();
    }

private:
    // internal streambuf
    class InBuf : public std::streambuf {
    public:
        explicit InBuf(size_t buffer_size)
            : stream_(NULL), bytes_read_(0), buffer_(buffer_size) {
            if (buffer_size == 0) buffer_.resize(2);
        }
        // set stream to the buffer
        void set_stream(Stream *stream);
        // return how many bytes read so far
        size_t bytes_read(void) const {
            return bytes_read_;
        }

    private:
        /*! @brief internal stream by StreamBuf */
        Stream *stream_;
        /*! @brief how many bytes we read so far */
        size_t bytes_read_;
        /*! @brief internal buffer */
        std::vector<char> buffer_;
        // override underflow
        int_type underflow();
    };
    /*! @brief input buffer */
    InBuf buf_;
};
}  // namespace rdc
