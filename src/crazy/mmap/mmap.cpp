#include "crazy/mmap/mmap.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace crazy {
#ifdef _WIN32
	static uint64_t get_file_size(void* hFile) {
		LARGE_INTEGER size;
		if (!GetFileSizeEx(static_cast<HANDLE>(hFile), &size)) {
			throw std::runtime_error("GetFileSizeEx failed");
		}
		return static_cast<uint64_t>(size.QuadPart);
	}
#else
	static uint64_t get_file_size(int fd) {
		struct stat sb;
		if (fstat(fd, &sb) != 0) {
			throw std::runtime_error("fstat failed: " + std::string(strerror(errno)));
		}
		return static_cast<uint64_t>(sb.st_size);
	}
#endif
	MmapInterface::MmapInterface(const std::string& filepath, uint64_t size) {
		open(filepath, size);
	}
	MmapInterface::~MmapInterface() {
		close();
	}
	void MmapInterface::open(const std::string& filepath, uint64_t requested_size) {
		ensure_not_opened();
		filepath_ = filepath;

#ifdef _WIN32
		fileHandle_ = CreateFileA(filepath_.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (fileHandle_ == INVALID_HANDLE_VALUE) {
			DWORD err = GetLastError();
			throw std::runtime_error("Failed to open/create file: " + filepath_ +
				" (Windows error: " + std::to_string(err) + ")");
		}
		uint64_t actual_file_size = get_file_size(fileHandle_);

		if (requested_size > 0) {
			size_ = actual_file_size > requested_size ? actual_file_size : requested_size;
		}
		else {
			size_ = actual_file_size;
		}

		// 仅当需要扩展时才调用 SetEndOfFile
		if (size_ > actual_file_size) {
			LARGE_INTEGER li;
			li.QuadPart = static_cast<LONGLONG>(size_);
			if (!SetFilePointerEx(static_cast<HANDLE>(fileHandle_), li, nullptr, FILE_BEGIN)) {
				CloseHandle(static_cast<HANDLE>(fileHandle_));
				fileHandle_ = INVALID_HANDLE_VALUE;
				throw std::runtime_error("SetFilePointerEx failed on Windows");
			}
			if (!SetEndOfFile(static_cast<HANDLE>(fileHandle_))) {
				CloseHandle(static_cast<HANDLE>(fileHandle_));
				fileHandle_ = INVALID_HANDLE_VALUE;
				throw std::runtime_error("SetEndOfFile failed on Windows");
			}
		}

#else
		fd_ = ::open(filepath_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (fd_ == -1) {
			throw std::runtime_error("Failed to open/create file: " + filepath_ +
				" error: " + std::strerror(errno));
		}

		uint64_t actual_file_size = get_file_size(fd_);

		if (requested_size > 0) {
			size_ = std::max(actual_file_size, requested_size);
		}
		else {
			size_ = actual_file_size;
		}

		if (size_ > actual_file_size) {
			if (ftruncate(fd_, static_cast<off_t>(size_)) != 0) {
				::close(fd_);
				fd_ = -1;
				throw std::runtime_error("ftruncate failed: " + std::string(strerror(errno)));
			}
		}
#endif
		if (size_ > 0) {
			mmap();
		}
		else {
			data_ = nullptr;
		}

		is_opened_ = true;
	}

	void MmapInterface::close() {
		if (!is_opened_) {
			return;
		}

		unmmap();

#ifdef _WIN32
		if (mappingHandle_ != nullptr) {
			CloseHandle(static_cast<HANDLE>(mappingHandle_));
			mappingHandle_ = nullptr;
		}
		if (fileHandle_ != INVALID_HANDLE_VALUE) {
			CloseHandle(static_cast<HANDLE>(fileHandle_));
			fileHandle_ = INVALID_HANDLE_VALUE;
		}
#else
		if (fd_ != -1) {
			::close(fd_);
			fd_ = -1;
		}
#endif

		data_ = nullptr;
		size_ = 0;
		filepath_.clear();
		is_opened_ = false;
	}
	void* MmapInterface::data() const {
		return data_;
	}
	uint64_t MmapInterface::size() const {
		return size_;
	}
	const std::string& MmapInterface::filepath() const {
		return filepath_;
	}
	void MmapInterface::mmap() {
		if (size_ == 0) {
			data_ = nullptr;
			return;
		}
#ifdef _WIN32
		mappingHandle_ = CreateFileMappingA(
			static_cast<HANDLE>(fileHandle_),
			nullptr,
			PAGE_READWRITE,
			static_cast<DWORD>(size_ >> 32),
			static_cast<DWORD>(size_ & 0xFFFFFFFFU),
			nullptr
		);
		if (mappingHandle_ == nullptr) {
			DWORD err = GetLastError();
			throw std::runtime_error("CreateFileMapping failed (error: " + std::to_string(err) + ")");
		}

		data_ = MapViewOfFile(
			static_cast<HANDLE>(mappingHandle_),
			FILE_MAP_WRITE | FILE_MAP_READ,
			0, 0,
			static_cast<SIZE_T>(size_)
		);
		if (data_ == nullptr) {
			DWORD err = GetLastError();
			CloseHandle(static_cast<HANDLE>(mappingHandle_));
			mappingHandle_ = nullptr;
			throw std::runtime_error("MapViewOfFile failed (error: " + std::to_string(err) + ")");
		}
#else
		data_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
		if (data_ == MAP_FAILED) {
			throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
		}
#endif
	}
	void MmapInterface::unmmap() {
		sync();
		if (data_ == nullptr) {
			return;
		}
#ifdef _WIN32
		{
			UnmapViewOfFile(data_);
			data_ = nullptr;
		}
#else
		{
			if (data_ != MAP_FAILED) {
				::munmap(data_, size_);
			}
			data_ = nullptr;
		}
#endif
	}

	void MmapInterface::remmap(uint64_t new_size) {
		if (!is_opened_) {
			throw std::logic_error("Cannot remmap: file not opened");
		}
		if (new_size == 0) {
			throw std::invalid_argument("New size must be > 0");
		}
		if (new_size == size_) {
			return;
		}

		unmmap();

#ifdef _WIN32
		{
			LARGE_INTEGER li;
			li.QuadPart = static_cast<LONGLONG>(new_size);
			if (!SetFilePointerEx(static_cast<HANDLE>(fileHandle_), li, nullptr, FILE_BEGIN)) {
				throw std::runtime_error("SetFilePointerEx failed during remmap on Windows");
			}
			if (!SetEndOfFile(static_cast<HANDLE>(fileHandle_))) {
				throw std::runtime_error("SetEndOfFile failed during remmap on Windows");
			}
		}
#else
		{
			if (ftruncate(fd_, static_cast<off_t>(new_size)) != 0) {
				throw std::runtime_error("ftruncate failed during remmap: " + std::string(strerror(errno)));
			}
		}
#endif
		size_ = new_size;
		mmap();
	}
	void MmapInterface::ensure_not_opened() const {
		if (is_opened_) {
			throw std::logic_error("File already opened");
		}
	}
	void MmapInterface::sync() {
		if (!is_opened_ || data_ == nullptr || size_ == 0) {
			return;
		}

#ifdef _WIN32
		if (!FlushViewOfFile(data_, static_cast<SIZE_T>(size_))) {
			DWORD err = GetLastError();
		}
		if (!FlushFileBuffers(static_cast<HANDLE>(fileHandle_))) {
		}
#else
		if (::msync(data_, size_, MS_SYNC) != 0) {
		}
#endif
	}
} // namespace crazy
