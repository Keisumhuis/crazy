#include "scheduler.h"
#include <algorithm>

namespace crazy {

	static ConfigValue<uint32_t>::Ptr g_scheduler_processer_size = Config::Lookup (
			"scheduler.processer_size", (uint32_t)4
			, "processer size in scheduler");

	Scheduler::Scheduler() {
		SetProcesserSize(std::thread::hardware_concurrency());
		Start();
	}
	Scheduler::~Scheduler() {
		Stop();
	}
	const size_t Scheduler::GetProcesserSize() const {
		return m_processerCount;
	}
	void Scheduler::SetProcesserSize(const size_t size) {
		CRAZY_ASSERT(size > 0);
		if (m_processerCount > size) {
			for (size_t i = 0; i < m_processerCount - size; ++i) {
				Mutex::Guard gurad(m_processers[i]->m_mutex);
				for (auto& it : m_processers[i]->GetRunningTasks()) {
					m_taskBuffer.push_back(it);
					m_processers[i]->GetRunningTasks().erase(it);
				}
				for (auto& it : m_processers[i]->GetWaitingTasks()) {
					m_taskBuffer.push_back(it);
					m_processers[i]->GetWaitingTasks().erase(it);
				}	
			}
		} else if (m_processerCount < size) {
			for (size_t i = 0; i < size - m_processerCount; ++i) {
				m_processers.push_back(Processer::Ptr(new Processer(256, 10)));
			}
		}
		m_processerCount = size;
		LoadBalance();
	}
	const size_t Scheduler::GetCoroutineSize() const {
		size_t count = 0;
		for (auto& it : m_processers) {
			count += it->GetCurrentCoroutineTaskSize();
		}
		return count;
	}
	void Scheduler::AddTask(CoroutineTask::Ptr task) {
		CRAZY_ASSERT(m_processers.size() > 0);
		std::sort(m_processers.begin(), m_processers.end(), ProcesserCompare());		
		m_processers[0]->AddTask(task);
	}
	void Scheduler::AddTask(std::function<void ()> func) {
		crazy::CoroutineTask::Ptr task(new crazy::CoroutineTask());
		task->coroctine = std::make_shared<crazy::Coroutine>(func);
		task->func = func;
		AddTask(task);
	}
	void Scheduler::Start() {
		for (auto& it : m_processers) {
			it->Start();
		}
	}
	void Scheduler::Stop() {
		for (auto& it : m_processers) {
			it->Stop();
		}
	}
	void Scheduler::LoadBalance() {
		for (auto& it : m_processers) {
			it->m_mutex.Lock();
			for (auto& itTask : it->GetRunningTasks()) {
				m_taskBuffer.push_back(itTask);
				it->GetRunningTasks().erase(itTask);
			}
			for (auto& itTask : it->GetWaitingTasks()) {
				m_taskBuffer.push_back(itTask);
				it->GetWaitingTasks().erase(itTask);
			}
		}
		for (size_t i = 0
			; i < (m_taskBuffer.size() / m_processers.size())
			; i+= m_processers.size()) {
			for (size_t j = 0; j < m_processers.size(); ++j) {
				m_processers[j]->AddTask(m_taskBuffer[j + i]);
			}	
		}
		
		for (auto& it : m_taskBuffer) {
			m_processers[0]->AddTask(it);
		}
		m_taskBuffer.clear();

		for (auto& it : m_processers) {
			it->m_mutex.UnLock();
		}
	}
}
