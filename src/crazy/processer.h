/**
 * @file processer.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_PROCESSER_H____
#define ____CRAZY_PROCESSER_H____

#include <atomic>
#include <exception>
#include <functional>
#include <memory>
#include <set>

#include "assert.h"
#include "config.h"
#include "coroutine.h"
#include "logger.h"
#include "mutex.h"
#include "noncopyable.h"
#include "thread.h"

namespace crazy {

	struct CoroutineTask final {
		using Ptr = std::shared_ptr<CoroutineTask>;
		Coroutine::Ptr coroctine;
		std::function<void ()> func;
		uint32_t thread_id = 0;
	};

	class Processer final : public std::enable_shared_from_this<Processer> {
		friend class Scheduler;
	public:
		using Ptr = std::shared_ptr<Processer>;
		explicit Processer(const uint32_t& maxRunningCount = 0
				,const uint32_t& scheduleInterval = 0);
		~Processer();
		static CoroutineTask::Ptr GetCurrentCoroutineTask();
		bool IsCoroutine() const;
		size_t GetCurrentCoroutineTaskSize() const;
		size_t GetCurrentRunningCoroutineTaskSize() const;
		uint32_t GetMaxRunningCoroutine() const;
		void SetMaxRunningCoroutine(const uint32_t& count);
		void AddTask(CoroutineTask::Ptr coroutineTask);
		void AddTask(std::set<CoroutineTask::Ptr> coroutinesTask);
		void Start();
		void Stop();
		bool IsStopping() const;
	private:
		static void Run(void* ptr);
		std::set<CoroutineTask::Ptr>& GetRunningTasks();
		std::set<CoroutineTask::Ptr>& GetWaitingTasks();
	private:
		int32_t m_thread_id;
		std::atomic<bool> m_isRunning;
		Mutex m_mutex;
		Semaphore m_sem;
		uint32_t m_maxRunningTask = 0;
		uint32_t m_scheduleInterval = 0;
		Thread::Ptr m_thread;
		CoroutineTask::Ptr m_currentCoroutineTask;
		std::set<CoroutineTask::Ptr> m_runningTask;
		std::set<CoroutineTask::Ptr> m_waitingTask;
	};

	struct ProcesserCompare {
		bool operator()(const Processer::Ptr lhs, const Processer::Ptr rhs) {
			return lhs->GetCurrentCoroutineTaskSize() < rhs->GetCurrentCoroutineTaskSize();
		}
	};

}

#endif // ! ____CRAZY_PROCESSER_H____
