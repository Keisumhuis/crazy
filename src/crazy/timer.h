/**
 * @file timer.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_TIMER_H____
#define ____CRAZY_TIMER_H____

#include <functional>
#include <memory>
#include <set>

#include "mutex.h"
#include "noncopyable.h"
#include "util.h"

namespace crazy {
	
	struct TimerCompare;
	struct TimerManager;
	class Timer final 
		: public std::enable_shared_from_this<Timer> {
	private:
		friend struct TimerCompare;
		friend class TimerManager;
		using Ptr = std::shared_ptr<Timer>;
		Timer(uint64_t ms, std::function<void ()> cb
				, bool recurring = false, TimerManager* mgr = nullptr);
	public:
		void Cancel();
		void Refresh();
	private:
		uint64_t m_ms;
		uint64_t m_timestamp;
		bool m_recurring = false;
		std::function<void()> m_cb;
		TimerManager* m_manager;
	};

	struct TimerCompare {
		bool operator()(const Timer::Ptr& lhs, const Timer::Ptr& rhs) const;
	};

	class TimerManager : public std::enable_shared_from_this<TimerManager> {
	public:
		friend class Timer;
		using Ptr = std::shared_ptr<TimerManager>;
		TimerManager();
		virtual void AddTimer(uint64_t ms, std::function<void ()> cb, bool recurring = false);
		bool HasTimer() const;
		uint64_t GetNextTimer();
		void Trigger();
	private:
		Mutex m_mutex;
		std::set<Timer::Ptr, TimerCompare> m_timers;
		uint64_t m_previouseTime = 0;
	};
}

#endif // ! ____CRAZY_TIMER_H____

