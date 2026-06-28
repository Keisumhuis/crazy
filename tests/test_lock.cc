#include "crazy.h"

void test_cond_mutex() {
    auto begin = crazy::GetCurrentNS();
    int64_t index = 0;
    crazy::CondMutex lock;
    for (auto i = 0; i < 1000000; ++i) {  // 减少循环次数
        crazy::CondMutexGuard guard(lock);
        ++index;
    }
    auto cost = double(crazy::GetCurrentNS() - begin) / 1000000;
    CRAZY_ROOT_DEBUG() << "CondMutex cost: " << cost << " ns per lock";
}

void test_std_mutex() {
    auto begin = crazy::GetCurrentNS();
    int64_t index = 0;
    std::mutex lock;
    for (auto i = 0; i < 1000000; ++i) {
        std::lock_guard<std::mutex> guard(lock);
        ++index;
    }
    auto cost = double(crazy::GetCurrentNS() - begin) / 1000000;
    CRAZY_ROOT_DEBUG() << "std::mutex cost: " << cost << " ns per lock";
}

void test_atomic_lock() {
    auto begin = crazy::GetCurrentNS();
    int64_t index = 0;
    crazy::AtomicLock lock;
    for (auto i = 0; i < 1000000; ++i) {
        crazy::AtomicLockGuard guard(lock);
        ++index;
    }
    auto cost = double(crazy::GetCurrentNS() - begin) / 1000000;
    CRAZY_ROOT_DEBUG() << "AtomicLock cost: " << cost << " ns per lock";
}

int32_t main() {
    CRAZY_ROOT_DEBUG() << "=== Lock Performance Test ===";
    test_std_mutex();
    test_atomic_lock();
    test_cond_mutex();
    return 0;
}
