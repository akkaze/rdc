#pragma once

#include <cstdint>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif  // _WIN32

namespace rdc {

enum ShmError {
    kOK = 0,
    kErrorCreationFailed = 100,
    kErrorMappingFailed = 110,
    kErrorOpeningFailed = 120,
};

class Shm {
public:
    /**
     * @brief: create share memory with specific size
     *
     * @param path should only contain alpha-numeric characters
     * @param size size of share memory
     */
    explicit Shm(std::string path, size_t size);

    /**
     * @brief: create a shared memory area and open it for writing
     *
     * @return error indicates whether creation is successful
     */
    inline ShmError Create() {
        return CreateOrOpen(true);
    };

    /**
     * @brief: open an existing shared memory for reading
     *
     * @return error indicates whether opening is successful
     */
    inline ShmError Open() {
        return CreateOrOpen(false);
    };

    inline size_t Size() {
        return size_;
    };
    inline const std::string& Path() {
        return path_;
    }
    inline uint8_t* Data() {
        return data_;
    }

    ~Shm();

private:
    ShmError CreateOrOpen(bool create);

    std::string path_;
    uint8_t* data_ = nullptr;
    size_t size_ = 0;
#if defined(_WIN32)
    HANDLE handle_;
#else
    int fd_ = -1;
#endif
};
}  // namespace shoom
