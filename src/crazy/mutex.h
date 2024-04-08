/**
 * @file mutex.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_MUTEX_H____
#define ____CRAZY_MUTEX_H____

#include <pthread.h>
#include <semaphore.h>

#include <atomic>

#include "noncopyable.h"

namespace crazy {

	class Semaphore final : Noncopyable {
	public:
		Semaphore() {
			sem_init(&m_sem, 0, 0);
		}
		~Semaphore() {
			sem_destroy(&m_sem);
		}
		void Wait() {
			sem_wait(&m_sem);
		}
		void Post() {
			sem_post(&m_sem);
		}
	private:
		mutable sem_t m_sem;
	};

	class MutexGuard;
	class Mutex final {
	public:
		using Guard = MutexGuard;
		Mutex() {
			pthread_mutex_init(&m_mutex, nullptr);
		}
		~Mutex() {
			pthread_mutex_destroy(&m_mutex);
		}
		void Lock() {
			if(!pthread_mutex_lock(&m_mutex)) {
				m_isLock = true;
			}
		}
		void UnLock() {
			if(!pthread_mutex_unlock(&m_mutex)) {
				m_isLock = false;
			}
		}
		bool IsLock() const { return m_isLock; }
	private:
		pthread_mutex_t m_mutex;
		bool m_isLock = false;
	};

	class NullMutex final {
	public:
		void Lock() { m_isLock= true; }
		void UnLock() { m_isLock = false; }
		bool IsLock() const { return m_isLock; }
	private:
		bool m_isLock = false;
	};

	class MutexGuard final : Noncopyable {
	public:
		MutexGuard(const Mutex& m)
	       	: m_mutex(m) {
			m_mutex.Lock();	
		}
		~MutexGuard() {
			m_mutex.UnLock();
		}
	private:
		Mutex m_mutex;
	};

	class SpinLockGuard;
	class SpinLock final {
	public:
		using Gurad = SpinLockGuard;
		SpinLock() {
			pthread_spin_init(&m_spinlock, 0);
		}
		~SpinLock() {
			pthread_spin_destroy(&m_spinlock);
		}
		void Lock() {
			if(!pthread_spin_lock(&m_spinlock)) {
				m_isLock = true;
			}
		}
		void UnLock() {
			if(!pthread_spin_unlock(&m_spinlock)) {
				m_isLock = false;
			}
		}
		bool IsLock() const { return m_isLock; }
	private:
		pthread_spinlock_t m_spinlock;
		bool m_isLock = false;
	};

	class SpinLockGuard final : Noncopyable {
	public:
		SpinLockGuard(const SpinLock& m)
	       	: m_lock(m) {
			m_lock.Lock();	
		}
		~SpinLockGuard() {
			m_lock.UnLock();
		}
	private:
		SpinLock m_lock;
	};
	
	class RDMutexGuard;
	class WRMutexGuard;
	class RWMutex final {
	public:
		using RDGuard = RDMutexGuard;
		using WRGuard = WRMutexGuard;
		RWMutex() {
			pthread_rwlock_init(&m_rwlock, nullptr);
		}
		~RWMutex() {
			pthread_rwlock_destroy(&m_rwlock);
		}
		void RDLock() {
			if(!pthread_rwlock_rdlock(&m_rwlock)) {
				m_isLock = true;
			}
		}
		void WRLock() {
			if(!pthread_rwlock_wrlock(&m_rwlock)) {
				m_isLock = true;
			}
		}	
		void UnLock() {
			if(!pthread_rwlock_unlock(&m_rwlock)) {
				m_isLock = false;
			}
		}
		bool IsLock() const { return m_isLock; }
	private:
		pthread_rwlock_t m_rwlock;
		bool m_isLock = false;
	};
	
	class NullRWMutex final : Noncopyable {
	public:
		void RDLock() {	m_isLock = true; }
		void WRLock() { m_isLock = true; }	
		void UnLock() { m_isLock = false; }
		bool IsLock() const { return m_isLock; }
	private:
		bool m_isLock = false;
	};

	class RDMutexGuard final : Noncopyable {
	public:
		RDMutexGuard(const RWMutex& rwlock) 
			: m_rwlock(rwlock) {
			m_rwlock.RDLock();
		}
		~RDMutexGuard() {
			m_rwlock.UnLock();
		}
	private:
		RWMutex m_rwlock;
	};	
	
	class WRMutexGuard final : Noncopyable {
	public:
		WRMutexGuard(const RWMutex& rwlock) 
			: m_rwlock(rwlock) {
			m_rwlock.WRLock();
		}
		~WRMutexGuard() {
			m_rwlock.UnLock();
		}
	private:
		RWMutex m_rwlock;
	};

	class CASLock : Noncopyable {
	public:
    		CASLock() {
        		m_mutex.clear();
    		}
    		~CASLock() {}
    		void lock() {
        		while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));
    		}
    		void unlock() {
        		std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    		}
	private:
    		volatile std::atomic_flag m_mutex;
	};

}

#endif // ! ____CRAZY_MUTEX_H____
