#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <string>
#include <cassert>
#include <atomic>
#include "crazy.h"

using namespace crazy;

// 测试断言宏
#define TEST_ASSERT(cond, msg)                                     \
    do                                                             \
    {                                                              \
        if (!(cond))                                               \
        {                                                          \
            std::cerr << "❌ 测试失败: " << msg << " 在 "          \
                      << __FILE__ << ":" << __LINE__ << std::endl; \
            return false;                                          \
        }                                                          \
    } while (0)

#define TEST_SUITE(name)                                     \
    std::cout << "\n========== " << name << " ==========\n"; \
    bool result = true;                                      \
    do

#define TEST_END(name)                           \
    while (0)                                    \
        ;                                        \
    if (result)                                  \
        std::cout << "✅ " << name << " 通过\n"; \
    else                                         \
        std::cout << "❌ " << name << " 失败\n"; \
    std::cout << std::string(40, '=') << "\n"

//------------------------------------------------------------------------------
// 基本读写测试
bool testBasicReadWrite()
{
    TEST_SUITE("基本读写测试")
    {
        MVCCLockWapper<int> counter(0);

        TEST_ASSERT(counter.read() == 0, "初始值应为0");

        {
            auto tx = counter.beginWrite();
            tx.set(42);
        }
        TEST_ASSERT(counter.read() == 42, "写入后值应为42");

        {
            auto tx = counter.beginWrite();
            tx.get() = 100;
            tx.get() += 50;
        }
        TEST_ASSERT(counter.read() == 150, "多次修改后值应为150");

        const auto& const_counter = counter;
        TEST_ASSERT(const_counter.read() == 150, "const对象读取应返回相同值");
    }
    TEST_END("基本读写测试");
    return result;
}

//------------------------------------------------------------------------------
// 手动提交和放弃测试
bool testManualCommitAndAbort()
{
    TEST_SUITE("手动提交和放弃测试")
    {
        MVCCLockWapper<std::string> str("hello");

        {
            auto tx = str.beginWrite();
            tx.set("world");
            tx.commit();
            TEST_ASSERT(!tx.isActive(), "提交后事务应非活动");
        }
        TEST_ASSERT(str.read() == "world", "手动提交后值应更新");

        {
            auto tx = str.beginWrite();
            tx.set("aborted");
            tx.abort();
            TEST_ASSERT(!tx.isActive(), "放弃后事务应非活动");
        }
        TEST_ASSERT(str.read() == "world", "放弃修改后值应保持不变");

        {
            auto tx = str.beginWrite();
            tx.set("auto commit");
        }
        TEST_ASSERT(str.read() == "auto commit", "析构时自动提交应生效");
    }
    TEST_END("手动提交和放弃测试");
    return result;
}

//------------------------------------------------------------------------------
// 获取当前值测试
bool testGetCurrentValue()
{
    TEST_SUITE("获取当前值测试")
    {
        MVCCLockWapper<int> counter(10);

        {
            auto tx = counter.beginWrite();
            TEST_ASSERT(tx.getCurrentValue() == 10, "事务中getCurrentValue应返回旧值");
            tx.set(20);
            TEST_ASSERT(tx.getCurrentValue() == 10, "未提交时getCurrentValue仍应返回旧值");
        }
        TEST_ASSERT(counter.read() == 20, "提交后读取应返回新值");
    }
    TEST_END("获取当前值测试");
    return result;
}

//------------------------------------------------------------------------------
// 移动语义测试（使用 vector 验证复杂类型）
bool testMoveSemantics()
{
    TEST_SUITE("移动语义测试")
    {
        MVCCLockWapper<std::vector<int>> vec;

        // 初始化 [1,2]
        {
            auto tx = vec.beginWrite();
            std::vector<int> v;
            v.push_back(1);
            v.push_back(2);
            tx.set(v);
        }

        // 测试移动构造
        {
            auto tx1 = vec.beginWrite();
            auto old = tx1.getCurrentValue();  // [1,2]
            auto newv = old;
            newv.push_back(3);                  // [1,2,3]
            tx1.set(newv);

            auto tx2 = std::move(tx1);
            TEST_ASSERT(!tx1.isActive(), "移动后原事务应非活动");
            TEST_ASSERT(tx2.isActive(), "新事务应活动");
            // tx2 析构时提交
        }
        auto result = vec.read();
        TEST_ASSERT(result.size() == 3, "向量大小应为3");
        TEST_ASSERT(result[0] == 1 && result[1] == 2 && result[2] == 3, "元素应为1,2,3");

        {
            // 先创建 txB 并使其非活动
            auto txB = vec.beginWrite();
            txB.abort();  // 现在 txB 非活动，锁已释放

            // 再创建 txA（此时锁可用）
            auto txA = vec.beginWrite();
            auto old = txA.getCurrentValue();    // [1,2,3]
            auto newA = old;
            newA.push_back(4);                    // [1,2,3,4]
            txA.set(newA);

            txB = std::move(txA);                  // 移动赋值给非活动的 txB
            TEST_ASSERT(!txA.isActive(), "txA 应非活动");
            TEST_ASSERT(txB.isActive(), "txB 应活动");
            // txB 析构时提交 [1,2,3,4]
        }
        result = vec.read();
        TEST_ASSERT(result.size() == 4, "向量大小应为4");
        TEST_ASSERT(result[3] == 4, "第四个元素应为4");
    }
    TEST_END("移动语义测试");
    return result;
}

//------------------------------------------------------------------------------
// 版本号跟踪测试
bool testVersionTracking()
{
    TEST_SUITE("版本号跟踪测试")
    {
        MVCCLockWapper<int> counter(0);

        int64_t v1, v2, v3;

        auto val1 = counter.readWithVersion(v1);
        TEST_ASSERT(val1 == 0, "初始值应为0");
        TEST_ASSERT(v1 == 0, "初始版本号应为0");

        {
            auto tx = counter.beginWrite();
            tx.set(100);
        }

        auto val2 = counter.readWithVersion(v2);
        TEST_ASSERT(val2 == 100, "更新后值应为100");
        TEST_ASSERT(v2 == 1, "版本号应递增为1");

        TEST_ASSERT(counter.isUpdated(v1), "应检测到从v1更新");
        TEST_ASSERT(!counter.isUpdated(v2), "不应检测到从v2更新");

        {
            auto tx = counter.beginWrite();
            tx.set(200);
        }

        counter.readWithVersion(v3);
        TEST_ASSERT(counter.isUpdated(v2), "应检测到从v2更新");
        TEST_ASSERT(v3 == 2, "版本号应递增为2");

        // 两次写后版本号为2，当前索引应为0（两个槽交替使用）
        TEST_ASSERT(counter.getCurrentVersion() == 2, "当前版本号应为2");
        TEST_ASSERT(counter.getCurrentVersionIndex() == 0, "当前版本索引应为0");
    }
    TEST_END("版本号跟踪测试");
    return result;
}

//------------------------------------------------------------------------------
// 引用读取测试
bool testReadReference()
{
    TEST_SUITE("引用读取测试")
    {
        MVCCLockWapper<std::string> str("test");

        const auto& ref = str.readRef();
        TEST_ASSERT(ref == "test", "引用读取应返回正确值");

        {
            auto tx = str.beginWrite();
            tx.set("new value");
        }

        TEST_ASSERT(str.read() == "new value", "写入后值应更新");
    }
    TEST_END("引用读取测试");
    return result;
}

//------------------------------------------------------------------------------
// 并发读测试
bool testConcurrentReads()
{
    TEST_SUITE("并发读测试")
    {
        MVCCLockWapper<int> counter(0);
        std::vector<std::thread> readers;
        std::atomic<long long> total{ 0 };
        const int READER_COUNT = 10;
        const int READS_PER_THREAD = 10000;

        // 先写入一些值，确保读线程能读到变化的值
        for (int i = 1; i <= 5; ++i)
        {
            auto tx = counter.beginWrite();
            tx.set(i * 10);
        }

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < READER_COUNT; ++i)
        {
            readers.emplace_back([&counter, &total, READS_PER_THREAD]() {
                for (int j = 0; j < READS_PER_THREAD; ++j) {
                    total.fetch_add(counter.read());
                    std::this_thread::yield();
                }
                });
        }

        for (auto& t : readers) t.join();

        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "  总读取次数: " << READER_COUNT * READS_PER_THREAD
            << ", 累计值: " << total.load() << ", 耗时: " << ms.count() << "ms\n";
        TEST_ASSERT(total.load() > 0, "应有读取值");
    }
    TEST_END("并发读测试");
    return result;
}

//------------------------------------------------------------------------------
// 并发写测试
bool testConcurrentWrites()
{
    TEST_SUITE("并发写测试")
    {
        MVCCLockWapper<int> counter(0);
        std::vector<std::thread> writers;
        const int WRITER_COUNT = 5;
        const int WRITES_PER_THREAD = 100;

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < WRITER_COUNT; ++i)
        {
            writers.emplace_back([&counter, i, WRITES_PER_THREAD]() {
                for (int j = 0; j < WRITES_PER_THREAD; ++j) {
                    auto tx = counter.beginWrite();
                    tx.set(i * 1000 + j);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
                });
        }

        for (auto& t : writers) t.join();

        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "  总写入次数: " << WRITER_COUNT * WRITES_PER_THREAD
            << ", 最终值: " << counter.read() << ", 耗时: " << ms.count() << "ms\n";
        TEST_ASSERT(counter.read() >= 0, "最终值应有效");
    }
    TEST_END("并发写测试");
    return result;
}

//------------------------------------------------------------------------------
// 读写混合并发测试
bool testReadWriteConcurrency()
{
    TEST_SUITE("读写混合并发测试")
    {
        MVCCLockWapper<int> counter(42);
        std::vector<std::thread> threads;
        std::atomic<int> read_errors{ 0 };
        const int THREAD_COUNT = 8;
        const int OPERATIONS_PER_THREAD = 5000;

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < THREAD_COUNT; ++i)
        {
            threads.emplace_back([&counter, &read_errors, i, OPERATIONS_PER_THREAD]() {
                for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                    if (j % 3 == 0) {  // 写操作
                        auto tx = counter.beginWrite();
                        tx.set(i * 100 + j);
                    }
                    else {            // 读操作
                        int val = counter.read();
                        if (val < 0) read_errors.fetch_add(1);
                    }
                    std::this_thread::yield();
                }
                });
        }

        for (auto& t : threads) t.join();

        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "  总操作数: " << THREAD_COUNT * OPERATIONS_PER_THREAD
            << ", 读错误: " << read_errors.load()
            << ", 最终值: " << counter.read()
            << ", 耗时: " << ms.count() << "ms\n";
        TEST_ASSERT(read_errors.load() == 0, "读操作不应出现错误值");
    }
    TEST_END("读写混合并发测试");
    return result;
}

//------------------------------------------------------------------------------
// 复杂类型测试
bool testComplexType()
{
    TEST_SUITE("复杂类型测试")
    {
        struct Person
        {
            std::string name;
            int age;
            std::vector<std::string> hobbies;

            bool operator==(const Person& other) const
            {
                return name == other.name &&
                    age == other.age &&
                    hobbies == other.hobbies;
            }
        };

        MVCCLockWapper<Person> person(Person{ "Alice", 25, {"reading"} });

        auto p1 = person.read();
        TEST_ASSERT(p1.name == "Alice" && p1.age == 25 && p1.hobbies.size() == 1,
            "初始值正确");

        {
            auto tx = person.beginWrite();
            auto old = tx.getCurrentValue();
            Person p = old;
            p.name = "Bob";
            p.age = 30;
            p.hobbies.push_back("swimming");
            p.hobbies.push_back("coding");
            tx.set(p);
        }

        auto p2 = person.read();
        TEST_ASSERT(p2.name == "Bob", "姓名应为Bob");
        TEST_ASSERT(p2.age == 30, "年龄应为30");
        TEST_ASSERT(p2.hobbies.size() == 3, "爱好数量应为3");
        TEST_ASSERT(p2.hobbies[1] == "swimming" && p2.hobbies[2] == "coding",
            "爱好内容正确");
    }
    TEST_END("复杂类型测试");
    return result;
}

//------------------------------------------------------------------------------
// 主函数
int main()
{
    std::cout << "\n🚀 开始MVCC锁单元测试\n";
    std::cout << std::string(50, '=') << "\n";

    int passed = 0, total = 0;

    auto run = [&](const std::string& name, auto func) {
        std::cout << "\n▶️ 运行测试: " << name << "\n";
        if (func()) passed++;
        total++;
        };

    run("基本读写测试", testBasicReadWrite);
    run("手动提交和放弃测试", testManualCommitAndAbort);
    run("获取当前值测试", testGetCurrentValue);
    run("移动语义测试", testMoveSemantics);
    run("版本号跟踪测试", testVersionTracking);
    run("引用读取测试", testReadReference);
    run("并发读测试", testConcurrentReads);
    run("并发写测试", testConcurrentWrites);
    run("读写混合并发测试", testReadWriteConcurrency);
    run("复杂类型测试", testComplexType);

    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "📊 测试汇总: " << passed << "/" << total << " 通过\n";
    return (passed == total) ? 0 : 1;
}
