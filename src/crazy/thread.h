/**
 * @file thread.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief 
 */
#ifndef ____CRAZY_THREAD_H____
#define ____CRAZY_THREAD_H____

#include <pthread.h>

#include <functional>
#include <memory>

#include "logger.h"
#include "mutex.h"
#include "noncopyable.h"

namespace crazy {

	class Thread : public Noncopyable {
	public:
		using Ptr = std::shared_ptr<Thread>;
		Thread(std::function<void ()> func, const std::string& name = "");
		~Thread();
		void join();
		const pid_t getId() const { return m_id; }
		const std::string getName() const { return m_name; }
		static const Thread* GetThis();
		static const std::string GetName();
		static void SetName(const std::string& name);

	private:
		static void* Run(void *arg);

	private:
		pid_t m_id = -1;
		Semaphore m_sem;	
		std::string m_name;
		pthread_t m_thread = 0;
		std::function<void ()> m_cbfunc;
	};

}

#endif // ! ____CRAZY_THREAD_H____
