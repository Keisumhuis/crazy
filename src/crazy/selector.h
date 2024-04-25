/**
 * @file selector.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_SELECTOR_H____
#define ____CRAZY_SELECTOR_H____

#include <algorithm>
#include <functional>
#include <memory>
#include <map>

#include "assert.h"
#include "mutex.h"
#include "noncopyable.h"
#include "singleton.h"
#include "socket.h"
#include "thread.h"
#include "timer.h"

#define SELECTOR() 	crazy::Singleton<crazy::Selector>::GetInstance()
#define TIMER_MANAGER() crazy::Singleton<crazy::Selector>::GetInstance()

namespace crazy {

	enum class SelectEvent : int32_t {
		None 	= 0x0000,
		Read 	= 0x0001,
		Write 	= 0x0004,
		Close 	= 0x2000
	};
	struct AsyncEvent : public std::enable_shared_from_this<AsyncEvent>
			    , Noncopyable {
		using Ptr = std::shared_ptr<AsyncEvent>;
		Mutex mutex;
		int32_t fd;
		SelectEvent event = SelectEvent::None;
		std::function<void ()> in, out; 
	};

	class Selector final : public TimerManager, Noncopyable {
	public:
		using Ptr = std::shared_ptr<Selector>;
		Selector();
		~Selector();
		void Start();
		void Stop();	
		std::map<int32_t, AsyncEvent::Ptr> Steal(size_t count);
		std::map<int32_t, AsyncEvent::Ptr> StealAll();
		size_t GetAsyncEventCount() const;
		virtual void RegisterEvent(int32_t fd, SelectEvent events
				, std::function<void ()>&& cb);	
		virtual void UnregisterEvent(int32_t fd, SelectEvent events);
		virtual void CancelAllEvent(int32_t fd);
		virtual void Tickle();
		virtual void AddTimer(uint64_t ms, std::function<void ()> cb, bool recurring = false);
		bool IsStopping() const;
	protected:
		std::map<int32_t, AsyncEvent::Ptr>& GetEvents();
		virtual void Run();
	private:
		int32_t m_fd;
		int32_t m_pipe[2];
		Mutex m_mutex;
		bool m_stopping = true;	
		Thread::Ptr m_thread;
		std::map<int32_t, AsyncEvent::Ptr> m_events; 
	};
	
	struct SelectorCompare {
		bool operator()(Selector::Ptr lhs, Selector::Ptr rhs) const {
			return lhs < rhs ? true : false;
		}
	};
}

#endif // ! ____CRAZY_SELECTOR_H____

