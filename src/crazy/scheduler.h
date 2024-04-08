/**
 * @file scheduler.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_SCHEDULER_H____
#define ____CRAZY_SCHEDULER_H____

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <thread>
#include <vector>

#include "config.h"
#include "noncopyable.h"
#include "processer.h"
#include "singleton.h"

#define SCHEDULER() crazy::SingletonPtr<crazy::Scheduler>().GetInstance() 
#define co crazy::CoroutineTaskWrap()-

namespace crazy {
		
	class Scheduler final 
		: public Noncopyable
		  , public std::enable_shared_from_this<Scheduler> {
	public:
		using Ptr = std::shared_ptr<Scheduler>;
		Scheduler();
		~Scheduler();
		const size_t GetProcesserSize() const;
		void SetProcesserSize(const size_t size);
		const size_t GetCoroutineSize() const;
		void AddTask(CoroutineTask::Ptr task);
		void AddTask(std::function<void ()> func);
		void Start();
		void Stop();
		void LoadBalance();
	private:
		size_t m_processerCount = 0;
		std::vector<Processer::Ptr> m_processers;
		std::vector<CoroutineTask::Ptr> m_taskBuffer;
	}; 
	
	struct CoroutineTaskWrap {
		void operator-(std::function<void()> func) {
			crazy::CoroutineTask::Ptr task(new crazy::CoroutineTask());
			task->coroctine = std::make_shared<crazy::Coroutine>(func);
			task->func = func;
			SCHEDULER()->AddTask(task);
		}
	};
}

#endif // ! ____CRAZY_SCHEDULER_H____

