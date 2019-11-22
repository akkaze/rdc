#pragma once
#include "io/io.h"
namespace rdc {
enum class FileMode { kRead, kWrite, kReadWrite };

class FileStream : public SeekStream {
public:
    FileStream() = default;
    explicit FileStream(const std::string& filename, const FileMode& mode);
    FileStream(const std::string& filename)
        : FileStream(filename, FileMode::kReadWrite) {
    }
    virtual ~FileStream();

    size_t Read(void* ptr, size_t size);

    void Write(const void* ptr, size_t size);
    void Seek(size_t pos);

    size_t Tell();

    bool AtEnd() const;

private:
    /*! @brief filename in filesys*/
    std::string filename_;
    /*! @brief filepointer*/
    FILE* fp_;
    FileMode mode_;
};
}  // namespace rdc
