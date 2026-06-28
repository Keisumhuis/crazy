#include "atomic_lock.h"

namespace crazy {
	AtomicLock::AtomicLock() noexcept {
		flag_.clear();
	}
	void AtomicLock::lock() noexcept {
		int backoff = 0;
		while (flag_.test_and_set(std::memory_order_acquire)) {
			++backoff;
			if (backoff < 4) {
				std::this_thread::yield();
			}
			else {
				std::this_thread::sleep_for(std::chrono::microseconds(50));
			}
		}
	}

	bool AtomicLock::try_lock() noexcept {
		return !flag_.test_and_set(std::memory_order_acquire);
	}

	void AtomicLock::unlock() noexcept {
		flag_.clear(std::memory_order_release);
	}
	AtomicLockGuard::AtomicLockGuard(AtomicLock& lock)
		: lock_(lock) {
			lock_.lock();
	}
	AtomicLockGuard::~AtomicLockGuard() noexcept {
		lock_.unlock();
	}
}
