#include "crazy/file_lock.h"
#include "crazy/logger.h"

namespace crazy {
    FileLock::FileLock(const std::string& filePath) {
        setFilePath(filePath);
    }

    FileLock::~FileLock() {
        if (file_) {
            unlock();
#if _WIN32
            CloseHandle(file_);
#else
            fclose(file_);
#endif
            file_ = nullptr;
        }
    }
    void FileLock::setFilePath(const std::string& filePath) {
        if (!filePath_.empty() && file_) {
            unlock();
#if _WIN32
            CloseHandle(file_);
#else
            fclose(file_);
#endif
            file_ = nullptr;
        }
        filePath_ = filePath;
    }
    bool FileLock::lock() {
        if (filePath_.empty()) {
            CRAZY_SYSTEM_ERROR() << "lock file path is empty";
            return false;
        }
#if _WIN32
        file_ = CreateFileA(filePath_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (file_ == INVALID_HANDLE_VALUE) {
            CRAZY_SYSTEM_ERROR() << "open lock file fail, path = " << filePath_;
            file_ = nullptr;
            return false;
        }

        OVERLAPPED overlapped = { 0 };
        if (!LockFileEx(file_, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
            0, 0, 0, &overlapped)) {
            CRAZY_SYSTEM_ERROR() << "lock file fail, path = " << filePath_;
            CloseHandle(file_);
            file_ = nullptr;
            return false;
        }
#else
        file_ = fopen(filePath_.c_str(), "a+");
        if (!file_) {
            CRAZY_SYSTEM_ERROR() << "open lock file fail, path = " << filePath_;
            return false;
        }

        int fd = fileno(file_);
        if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
            CRAZY_SYSTEM_ERROR() << "lock file fail, path = " << filePath_;
            fclose(file_);
            file_ = nullptr;
            return false;
        }
#endif
        CRAZY_SYSTEM_DEBUG() << "file lock success, path = " << filePath_;
        return true;
    }
    bool FileLock::unlock() {
        if (!file_) {
            return true;
        }

#if _WIN32
        OVERLAPPED overlapped = { 0 };
        if (!UnlockFileEx(file_, 0, 0, 0, &overlapped)) {
            CRAZY_SYSTEM_ERROR() << "unlock file fail, path = " << filePath_;
            return false;
        }
#else
        int fd = fileno(file_);
        if (flock(fd, LOCK_UN) == -1) {
            CRAZY_SYSTEM_ERROR() << "unlock file fail, path = " << filePath_;
            return false;
        }
#endif
        CRAZY_SYSTEM_DEBUG() << "file unlock success, path = " << filePath_;
        return true;
    }
    bool FileLock::tryLock() {
        if (filePath_.empty()) {
            return false;
        }
#if _WIN32
        HANDLE hFile = CreateFileA(filePath_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, // 不共享
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }

        OVERLAPPED overlapped = { 0 };
        BOOL result = LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
            0, 0, 0, &overlapped);

        if (result) {
            if (file_) {
                unlock();
                CloseHandle(file_);
            }
            file_ = hFile;
            return true;
        }
        else {
            CloseHandle(hFile);
            return false;
        }
#else
        FILE* testFile = fopen(filePath_.c_str(), "a+");
        if (!testFile) {
            return false;
        }

        int fd = fileno(testFile);
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            if (file_) {
                unlock();
                fclose(file_);
            }
            file_ = testFile;
            return true;
        }
        else {
            fclose(testFile);
            return false;
        }
#endif
    }
}
