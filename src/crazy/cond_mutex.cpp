#include "crazy/cond_mutex.h"

namespace crazy {
	void CondMutex::lock() {
		mutex_.lock();
	}
	void CondMutex::unlock() {
		mutex_.unlock();
	}
	bool CondMutex::tryLock() {
		return mutex_.try_lock();
	}
	void CondMutex::wait() {
		std::unique_lock<std::mutex> lock(mutex_, std::adopt_lock);
		condition_variable_.wait(lock);
		lock.release();
	}
	void CondMutex::signal() {
		condition_variable_.notify_one();
	}
	void CondMutex::signalAll() {
		condition_variable_.notify_all();
	}
	CondMutexGuard::CondMutexGuard(CondMutex& condMutex) 
		: condMutex_(condMutex) {
			condMutex.lock();
	}
	CondMutexGuard::~CondMutexGuard() {
		condMutex_.unlock();
	}
}
