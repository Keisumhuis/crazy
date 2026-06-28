#include "crazy/utils.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <direct.h>
#include <stdlib.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#endif

#include <string.h>

#include <chrono>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <system_error>

#include "crazy/uuid.h"

namespace crazy {
	const uint64_t GetCurrentSS() {
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
	const uint64_t GetCurrentMS() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
	const uint64_t GetCurrentUS() {
		return std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
	const uint64_t GetCurrentNS() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}
    const std::string CreateUUID() {
        std::random_device rd;
        std::mt19937 mt19937(rd());
        uuids::uuid_random_generator gen(mt19937);
        return uuids::to_string(gen());
    }
	std::string StringUtil::Trim(const std::string& str, const std::string& delimit) {
		auto begin = str.find_first_not_of(delimit);
		if (begin == std::string::npos) {
			return "";
		}
		auto end = str.find_last_not_of(delimit);
		return str.substr(begin, end - begin + 1);
	}
	std::string StringUtil::TrimLeft(const std::string& str, const std::string& delimit) {
		auto begin = str.find_first_not_of(delimit);
		if (begin == std::string::npos) {
			return "";
		}
		return str.substr(begin);
	}
	std::string StringUtil::TrimRight(const std::string& str, const std::string& delimit) {
		auto end = str.find_last_not_of(delimit);
		if (end == std::string::npos) {
			return "";
		}
		return str.substr(0, end);
	}
	std::vector<std::string> StringUtil::Split(const std::string& str, const std::string& delimiter) {
		std::vector<std::string> result;
		if (delimiter.empty()) {
			result.push_back(str);
			return result;
		}

		size_t start = 0;
		size_t end = str.find(delimiter);
		size_t delimiterLength = delimiter.length();

		while (end != std::string::npos) {
			result.push_back(str.substr(start, end - start));
			start = end + delimiterLength;
			end = str.find(delimiter, start);
		}
		result.push_back(str.substr(start));

		return result;
	}
    std::string StringUtil::ToUpper(const std::string& str) {
        auto result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
	std::string StringUtil::ToLower(const std::string& str) {
        auto result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
	std::string PathUtil::GetExecutablePath() {
#ifdef _WIN32
		char buffer[MAX_PATH];
		DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
		if (length == 0) {
			throw std::runtime_error("Failed to get executable path");
		}
		return std::string(buffer, length);
#else
		char result[PATH_MAX];
		ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
		if (count == -1) {
			throw std::runtime_error("Failed to get executable path");
		}
		return std::string(result, count);
#endif
	}
    std::string PathUtil::GetExecutableName() {
        auto executablePath = GetExecutablePath();
#if defined(_WIN32)
        size_t pos = executablePath.find_last_of("\\/");
        if (pos != std::string::npos) {
            return executablePath.substr(pos + 1);
        }
        return "";
#else
        char* path_copy = strdup(executablePath.c_str());
        std::string name = basename(path_copy);
        free(path_copy);
        return name;
#endif
    }
	std::string PathUtil::GetExecutableDirectory() {
		std::string exePath = GetExecutablePath();
		return std::filesystem::path(exePath).parent_path().string();
	}
	std::string PathUtil::GetCurrentWorkingDirectory() {
#ifdef _WIN32
		char buffer[MAX_PATH];
		if (_getcwd(buffer, MAX_PATH) == nullptr) {
			throw std::runtime_error("Failed to get current working directory");
		}
		return buffer;
#else
		char buffer[PATH_MAX];
		if (getcwd(buffer, sizeof(buffer)) == nullptr) {
			throw std::runtime_error("Failed to get current working directory");
		}
		return buffer;
#endif
	}
	bool PathUtil::SetCurrentWorkingDirectory(const std::string& path) {
#ifdef _WIN32
		return _chdir(path.c_str()) == 0;
#else
		return chdir(path.c_str()) == 0;
#endif
	}
    std::string PathUtil::GetTempDirectory() {
#ifdef _WIN32
        char buffer[MAX_PATH];
        if (GetTempPathA(MAX_PATH, buffer) > 0) {
            return buffer;
        }
        return "C:\\Temp";
#else
        const char* tmp = getenv("TMPDIR");
        if (tmp) return tmp;

        tmp = getenv("TMP");
        if (tmp) return tmp;

        tmp = getenv("TEMP");
        if (tmp) return tmp;

        return "/tmp";
#endif
    }
	bool PathUtil::PathExists(const std::string& path) {
		return std::filesystem::exists(path);
	}
	bool PathUtil::IsAbsolutePath(const std::string& path) {
#ifdef _WIN32
		if (path.size() >= 2 && path[1] == ':') {
			return true;
		}
		if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
			return true;
		}
		return false;
#else
		return !path.empty() && path[0] == '/';
#endif
	}
    std::string PathUtil::JoinPath(const std::string& path1, const std::string& path2) {
        return (std::filesystem::path(path1) / path2).string();
    }

	std::string PathUtil::GetFileName(const std::string& path) {
		return std::filesystem::path(path).filename().string();
	}
	std::string PathUtil::GetDirectoryName(const std::string& path) {
		return std::filesystem::path(path).parent_path().string();
	}
	std::string PathUtil::GetFileExtension(const std::string& path) {
		return std::filesystem::path(path).extension().string();
	}
	std::string PathUtil::RemoveFileExtension(const std::string& path) {
		std::filesystem::path p(path);
		return p.replace_extension().string();
	}
    bool PathUtil::CreateDir(const std::string& path, bool create_parents) {
        if (create_parents) {
            return PathUtil::CreateDirectories(path);
        }

#ifdef _WIN32
        return CreateDirectoryA(path.c_str(), nullptr) != 0;
#else
        return mkdir(path.c_str(), 0755) == 0;
#endif
    }

    bool PathUtil::CreateDirectories(const std::string& path) {
        if (path.empty()) return false;
        if (PathExists(path)) return IsDirectory(path);

        try {
            return std::filesystem::create_directories(path);
        }
        catch (...) {
            return false;
        }
    }

    bool PathUtil::RemoveDir(const std::string& path) {
        if (!PathUtil::PathExists(path)) return true;
        if (!PathUtil::IsDirectory(path)) return false;

#ifdef _WIN32
        return RemoveDirectoryA(path.c_str()) != 0;
#else
        return rmdir(path.c_str()) == 0;
#endif
    }

    bool PathUtil::RemoveDirectoryRecursive(const std::string& path) {
        if (!PathExists(path)) return true;
        if (!IsDirectory(path)) return false;

        try {
            return std::filesystem::remove_all(path) > 0;
        }
        catch (...) {
            return false;
        }
    }

    bool PathUtil::IsDirectory(const std::string& path) {
        try {
            return std::filesystem::is_directory(path);
        }
        catch (...) {
            return false;
        }
    }

    bool PathUtil::IsFile(const std::string& path) {
        try {
            return std::filesystem::is_regular_file(path);
        }
        catch (...) {
            return false;
        }
    }

    std::vector<std::string> PathUtil::ListDirectory(const std::string& path, bool recursive) {
        std::vector<std::string> result;

        if (!PathExists(path) || !IsDirectory(path)) {
            return result;
        }

        try {
            if (recursive) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    result.push_back(entry.path().string());
                }
            }
            else {
                for (const auto& entry : std::filesystem::directory_iterator(path)) {
                    result.push_back(entry.path().string());
                }
            }
        }
        catch (...) {
        }

        return result;
    }

    std::string PathUtil::CreateTempDirectory(const std::string& prefix) {
        std::string tempDir = PathUtil::GetTempDirectory();
        std::string basePath = JoinPath(tempDir, prefix);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10000, 99999);

        for (int attempt = 0; attempt < 10; ++attempt) {
            std::string randomSuffix = std::to_string(dis(gen));
            std::string fullPath = basePath + "_" + randomSuffix;

            if (PathUtil::CreateDir(fullPath, true)) {
                return fullPath;
            }

            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            fullPath = basePath + "_" + std::to_string(timestamp);

            if (PathUtil::CreateDir(fullPath, true)) {
                return fullPath;
            }
        }

        return "";
    }

    int64_t PathUtil::GetAvailableSpace(const std::string& path) {
#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;

        if (GetDiskFreeSpaceExA(path.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
            return static_cast<int64_t>(freeBytesAvailable.QuadPart);
        }
#else
        struct statvfs vfs;
        if (statvfs(path.c_str(), &vfs) == 0) {
            return static_cast<int64_t>(vfs.f_bavail) * vfs.f_frsize;
        }
#endif
        return -1;
    }

    bool PathUtil::CopyDirectory(const std::string& source, const std::string& destination) {
        if (!PathExists(source) || !IsDirectory(source)) {
            return false;
        }

        try {
            if (!CreateDirectories(destination)) {
                return false;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(source)) {
                std::string relativePath = entry.path().string().substr(source.length());
                std::string destPath = PathUtil::JoinPath(destination, relativePath);

                if (std::filesystem::is_directory(entry.status())) {
                    if (!CreateDir(destPath, true)) {
                        return false;
                    }
                }
                else if (std::filesystem::is_regular_file(entry.status())) {
                    std::filesystem::copy_file(entry.path(), destPath, std::filesystem::copy_options::overwrite_existing);
                }
            }

            return true;
        }
        catch (...) {
            return false;
        }
    }
}
