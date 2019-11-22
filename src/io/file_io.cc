#include "io/file_io.h"

namespace rdc {

FileStream::FileStream(const std::string& filename, const FileMode& mode)
    : filename_(filename), mode_(mode) {
    const char* access_mode = "";
    switch (mode) {
        case FileMode::kRead:
            access_mode = "r";
            break;
        case FileMode::kWrite:
            access_mode = "w";
            break;
        case FileMode::kReadWrite:
            access_mode = "w+";
            break;
        default:
            access_mode = "w+";
    }
    fp_ = std::fopen(filename.c_str(), access_mode);
}

FileStream::~FileStream() {
    std::fclose(fp_);
}

size_t FileStream::Read(void* ptr, size_t size) {
    size_t nread = std::fread(ptr, sizeof(char), size, fp_);
    if (nread < size) {
        VLOG_F(2, "read encontered end of file");
    }
    return nread;
}

void FileStream::Write(const void* ptr, size_t size) {
    if (size == 0)
        return;
    size_t nwrite = std::fwrite(ptr, sizeof(char), size, fp_);
    if (nwrite < size) {
        VLOG_F(2, "write encontered end of file");
    }
}

void FileStream::Seek(size_t pos) {
    std::fseek(fp_, pos, 0);
}

size_t FileStream::Tell() {
    return std::ftell(fp_);
}

bool FileStream::AtEnd(void) const {
    return feof(fp_);
}

}  // namespace rdc
