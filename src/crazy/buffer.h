/**
 * @file buffer.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 动态字节缓冲区（读写分离 + 自动扩容 + 内存缩容）
 * @version 1.0
 * @date 2025
 * @copyright Copyright (c) 2025
 */
#pragma once

#include <stdint.h>
#include <vector>

namespace crazy {
    /**
     * @brief 高性能字节缓冲区
     * 采用读写指针分离设计，支持自动扩容、内存紧缩、数据前移复用
     * 适用于网络IO、数据序列化、二进制流处理
     */
    class Buffer final {
    public:
        /// 默认初始化大小 4KB
        static const uint32_t kInitializeSize = 1024 * 4;

    public:
        /**
         * @brief 构造函数
         * @param initializeSize 初始缓冲区容量
         */
        explicit Buffer(uint32_t initializeSize = kInitializeSize);
        /**
         * @brief 从已有数据构造缓冲区
         * @param data 数据指针
         * @param length 数据长度
         */
        explicit Buffer(char* data, uint32_t length);
        /**
         * @brief 追加单个字节
         * @param data 字节
         */
        void append(const char& data);
        /**
         * @brief 追加字节数组
         * @param data 数据指针
         * @param length 数据长度
         */
        void append(const char* data, uint32_t length);
        /**
         * @brief 追加vector<char>数据
         * @param data 字节向量
         */
        void append(const std::vector<char>& data);
        /**
         * @brief 获取可读字节数
         * @return 可读数据长度
         */
        uint32_t readableCount() const;
        /**
         * @brief 获取尾部可写字节数
         * @return 可写空间大小
         */
        uint32_t writableCount() const;
        /**
         * @brief 获取可读起始指针
         * @return 可读数据首地址
         */
        char* readBegin();
        /**
         * @brief 获取可写起始指针
         * @return 可写空间首地址
         */
        char* writeBegin();
        /**
         * @brief 标记已读取size字节
         * @param size 已读长度
         */
        void readed(uint32_t size);
        /**
         * @brief 标记已写入size字节
         * @param size 已写长度
         */
        void written(uint32_t size);
        /**
         * @brief 确保可写空间足够
         * @param count 需要的可写长度
         */
        void ensureWritableCount(int32_t count);
        /**
         * @brief 紧缩缓冲区：数据前移 + 内存缩容（释放多余内存）
         */
        void shrink();
        /**
         * @brief 重置缓冲区（清空数据+释放内存）
         */
        void reset();

    private:
        /// 可读起始位置
        uint32_t readableBegin_ = 0;
        /// 可写起始位置
        uint32_t writableBegin_ = 0;
        /// 底层存储
        std::vector<char> buffer_;
    };

} // namespace crazy
