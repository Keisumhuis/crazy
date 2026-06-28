#include "buffer.h"
#include <algorithm>
#include <stdexcept>

namespace crazy {

    Buffer::Buffer(uint32_t initializeSize)
        : buffer_(initializeSize) {
    }

    Buffer::Buffer(char* data, uint32_t length)
        : buffer_(length) {
        if (data == nullptr && length > 0) {
            throw std::invalid_argument("data pointer is null");
        }
        append(data, length);
    }

    void Buffer::append(const char& data) {
        ensureWritableCount(1);
        buffer_[writableBegin_++] = data;
    }

    void Buffer::append(const char* data, uint32_t length) {
        if (data == nullptr && length > 0) {
            throw std::invalid_argument("data pointer is null");
        }
        ensureWritableCount(static_cast<int32_t>(length));
        std::copy(data, data + length, buffer_.begin() + writableBegin_);
        writableBegin_ += length;
    }

    void Buffer::append(const std::vector<char>& data) {
        const auto len = static_cast<uint32_t>(data.size());
        ensureWritableCount(static_cast<int32_t>(len));
        std::copy(data.begin(), data.end(), buffer_.begin() + writableBegin_);
        writableBegin_ += len;
    }

    uint32_t Buffer::readableCount() const {
        return writableBegin_ - readableBegin_;
    }

    uint32_t Buffer::writableCount() const {
        return static_cast<uint32_t>(buffer_.size()) - writableBegin_;
    }

    char* Buffer::readBegin() {
        return buffer_.data() + readableBegin_;
    }

    char* Buffer::writeBegin() {
        return buffer_.data() + writableBegin_;
    }
    void Buffer::readed(uint32_t size) {
        if (size > readableCount()) {
            throw std::out_of_range("read size exceeds readable bytes");
        }

        readableBegin_ += size;
        if (readableCount() == 0) {
            reset();
        }
    }

    void Buffer::written(uint32_t size) {
        writableBegin_ += size;
    }

    void Buffer::ensureWritableCount(int32_t count) {
        if (count <= 0) {
            return;
        }

        const uint32_t need = static_cast<uint32_t>(count);
        if (writableCount() >= need) {
            return;
        }

        const uint32_t total_free = readableBegin_ + writableCount();
        if (total_free >= need) {
            shrink();
            return;
        }
        const size_t new_size = writableBegin_ + need;
        buffer_.resize(new_size);
    }
    void Buffer::shrink() {
        if (readableBegin_ > 0) {
            std::copy(buffer_.begin() + readableBegin_,
                buffer_.begin() + writableBegin_,
                buffer_.begin());

            writableBegin_ -= readableBegin_;
            readableBegin_ = 0;
        }
    }
    void Buffer::reset() {
        readableBegin_ = 0;
        writableBegin_ = 0;
        buffer_.clear();
        buffer_.shrink_to_fit();
    }

} // namespace crazy
