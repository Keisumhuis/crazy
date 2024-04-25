#include "stream.h"

namespace crazy {
    int32_t Stream::ReadFixSize(void* buffer, size_t length) {
        size_t offset = 0;
        int64_t left = length;
        while(left > 0) {
            int64_t len = Read((char*)buffer + offset, left);
            if(len <= 0) {
                return len;
            }
            offset += len;
            left -= len;
        }
        return length;
    }
	int32_t Stream::WriteFixSize(const void* buffer, size_t length) {
        size_t offset = 0;
        int64_t left = length;
        while(left > 0) {
            int64_t len = Write((const char*)buffer + offset, left);
            if(len <= 0) {
                return len;
            }
            offset += len;
            left -= len;
        }
        return length;
    }
}