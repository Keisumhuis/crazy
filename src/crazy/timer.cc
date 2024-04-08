#include "timer.h"

namespace crazy {
	Timer::Timer(uint64_t ms, std::function<void ()> cb, bool recurring, TimerManager* mgr) 
		: m_ms(ms)
		, m_cb(cb)
		, m_recurring(recurring)
       		, m_timestamp(GetCurrentMS() + ms)
       		, m_manager(mgr) {
	}
	void Timer::Refresh() {
		m_timestamp = GetCurrentMS() + m_ms;
	}
	bool TimerCompare::operator()(const Timer::Ptr& lhs, const Timer::Ptr& rhs) const {
		if(!lhs && !rhs) {
        		return false;
    		}
    		if(!lhs) {
        		return true;
    		}
   		 if(!rhs) {
   		     return false;
   		 }
   		 if(lhs->m_timestamp < rhs->m_timestamp) {
   		     return true;
   		 }
   		 if(rhs->m_timestamp < lhs->m_timestamp) {
   		     return false;
   		 }
   		 return lhs.get() < rhs.get();	
	}
	TimerManager::TimerManager() {
	}
	void TimerManager::AddTimer(uint64_t ms, std::function<void ()> cb, bool recurring) {
		Mutex::Guard guard(m_mutex);
		Timer::Ptr timer(new Timer(ms, cb, recurring, this));
		m_timers.insert(timer);
	}
	bool TimerManager::HasTimer() const {
		Mutex::Guard guard(m_mutex);
		return !m_timers.empty();
	}
	uint64_t TimerManager::GetNextTimer() {
		Mutex::Guard guard(m_mutex);
		if (m_timers.empty()) {
			return 0;
		}
		return m_timers.begin()->get()->m_ms;
	}
	void TimerManager::Trigger() {
		Mutex::Guard guard(m_mutex);
		for (auto it = m_timers.begin(); it != m_timers.end(); ) {
			
			if ((*it)->m_timestamp <= GetCurrentMS()) {
				if ((*it)->m_cb) {
					(*it)->m_cb();
					if ((*it)->m_recurring) {
						(*it)->Refresh();
					} else {
						it = m_timers.erase(it);
					}
				}
			} else {
				break;
			}
			++it;
		}
	}
}
