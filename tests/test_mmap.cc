#include "crazy.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace crazy;

void test_basic() {
    std::cout << "=== Test 1: Basic write and read ===\n";
    const char* path = "test_vec1.bin";

    // 清理旧文件
    std::filesystem::remove(path);

    {
        MmapVector<int> vec(path, 2); // 初始容量=2

        assert(vec.size() == 0);
        assert(vec.capacity() == 2);

        vec[0] = 100;
        vec[1] = 200;

        assert(vec.size() == 2);
        assert(vec[0] == 100);
        assert(vec[1] == 200);
    }

    // 重新打开，验证持久化
    {
        MmapVector<int> vec(path, 0); // 容量参数在已有文件时被忽略

        assert(vec.size() == 2);
        assert(vec.capacity() == 2);
        assert(vec[0] == 100);
        assert(vec[1] == 200);
    }

    std::cout << "✅ Passed\n";
}

void test_auto_grow() {
    std::cout << "\n=== Test 2: Auto-grow on out-of-bound write ===\n";
    const char* path = "test_vec2.bin";
    std::filesystem::remove(path);

    {
        MmapVector<int> vec(path, 1); // 初始容量=1

        vec[0] = 10;
        assert(vec.size() == 1);
        assert(vec.capacity() == 1);

        // 写入索引 3 → 需要 capacity >= 4
        vec[3] = 40;

        // 检查自动扩容：至少 4，且 ≥ 1*2=2 → 实际应为 max(4, 2) = 4
        assert(vec.size() == 4);
        assert(vec.capacity() >= 4);
        assert(vec[0] == 10);
        assert(vec[3] == 40);

        // 再写入索引 10 → 触发二次扩容
        vec[10] = 110;
        assert(vec.size() == 11);
        assert(vec.capacity() >= 11);
        // 扩容策略：上次 capacity=4 → 新容量 = max(11, 4*2=8) = 11 或更大（如 16，若实现取上界）
        // 我们只断言 >=11
        assert(vec[10] == 110);
    }

    // 重新加载验证
    {
        MmapVector<int> vec(path, 0);
        assert(vec.size() == 11);
        assert(vec[0] == 10);
        assert(vec[3] == 40);
        assert(vec[10] == 110);
    }

    std::cout << "✅ Passed\n";
}

void test_at_and_const() {
    std::cout << "\n=== Test 3: at() and const access ===\n";
    const char* path = "test_vec3.bin";
    std::filesystem::remove(path);

    {
        MmapVector<int> vec(path, 5);
        vec[2] = 999;

        // at() 安全访问
        assert(vec.at(2) != nullptr);
        assert(*vec.at(2) == 999);
        assert(vec.at(10) == nullptr); // 越界

        // const 访问
        const auto& cvec = vec;
        assert(cvec[2] == 999);
        // cvec[5] = 1; // 编译错误（const）
    }

    std::cout << "✅ Passed\n";
}

void test_alignment_and_type() {
    std::cout << "\n=== Test 4: Alignment and type safety ===\n";
    static_assert(sizeof(MmapVectorHeader) == 16, "Header must be 16 bytes (2×uint64_t, packed)");

    std::cout << "✅ Header size correct, type check enforced\n";
}

int main() {
    test_alignment_and_type();
    test_basic();
    test_auto_grow();
    test_at_and_const();

    std::cout << "\n🎉 All tests passed!\n";

    // 清理测试文件（可选）
    std::filesystem::remove("test_vec1.bin");
    std::filesystem::remove("test_vec2.bin");
    std::filesystem::remove("test_vec3.bin");

    return 0;
}
