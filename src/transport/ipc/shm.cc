#include "transport/ipc/shm.h"

#if defined(_WIN32)
#include <io.h>  // CreateFileMappingA, OpenFileMappingA, etc.
namespace rdc {

Shm::Shm(std::string path, size_t size) : path_(path), size_(size){};

ShmError Shm::CreateOrOpen(bool create) {
    if (create) {
        DWORD size_high_order = 0;
        DWORD size_low_order = static_cast<DWORD>(size_);
        handle_ =
            CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                               size_high_order, size_low_order, path_.c_str());

        if (!handle_) {
            return kErrorCreationFailed;
        }
    } else {
        handle_ = OpenFileMappingA(FILE_MAP_READ, FALSE, path_.c_str());

        if (!handle_) {
            return kErrorOpeningFailed;
        }
    }

    DWORD access = create ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

    data_ = static_cast<uint8_t*>(MapViewOfFile(handle_, access, 0, 0, size_));

    if (!data_) {
        return kErrorMappingFailed;
    }

    return kOK;
}

/**
 * Destructor
 */
Shm::~Shm() {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }

    CloseHandle(handle_);
}
}  // namespace rdc
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <errno.h>
#endif  // __APPLE__

#include <stdexcept>
namespace rdc {
Shm::Shm(std::string path, size_t size) : size_(size) {
    path_ = "/" + path;
};

ShmError Shm::CreateOrOpen(bool create) {
    if (create) {
        // shm segments persist across runs, and macOS will refuse
        // to ftruncate an existing shm segment, so to be on the safe
        // side, we unlink it beforehand.
        // TODO(zhengankun) check errno while ignoring ENOENT?
        int ret = shm_unlink(path_.c_str());
        if (ret < 0) {
            if (errno != ENOENT) {
                return kErrorCreationFailed;
            }
        }
    }

    int flags = create ? (O_CREAT | O_RDWR) : O_RDONLY;

    fd_ = shm_open(path_.c_str(), flags, 0755);
    if (fd_ < 0) {
        if (create) {
            return kErrorCreationFailed;
        } else {
            return kErrorOpeningFailed;
        }
    }

    if (create) {
        // this is the only way to specify the size of a
        // newly-created POSIX shared memory object
        int ret = ftruncate(fd_, size_);
        if (ret != 0) {
            return kErrorCreationFailed;
        }
    }

    int prot = create ? (PROT_READ | PROT_WRITE) : PROT_READ;

    data_ =
        static_cast<uint8_t *>(mmap(nullptr, size_, prot, MAP_SHARED, fd_, 0));

    if (!data_) {
        return kErrorMappingFailed;
    }

    return kOK;
}

Shm::~Shm() {
    munmap(data_, size_);
    close(fd_);
    shm_unlink(path_.c_str());
}

}  // namespace rdc
#endif
