#include "processer.h"
#include "scheduler.h"

namespace crazy {

		static ConfigValue<uint32_t>::Ptr g_max_processer_coroutine = Config::Lookup(
				"processer.max_running_task", uint32_t(256)
				, "in this processer max running  coroutine count");

		static ConfigValue<uint32_t>::Ptr g_schedule_interval = Config::Lookup(
				"processer.schedule_interval", uint32_t(10)
				, "in this processer schedule interval time (ms)");

		static thread_local Processer* t_processer = nullptr;

		Processer::Processer(const uint32_t& maxRunningCount
				,const uint32_t& scheduleInterval) 
			:m_maxRunningTask(maxRunningCount)
       			,m_scheduleInterval(scheduleInterval) {
			m_maxRunningTask = !maxRunningCount 
				? g_max_processer_coroutine->GetValue() : m_maxRunningTask;
			m_scheduleInterval = !m_scheduleInterval
				? g_schedule_interval->GetValue() : m_scheduleInterval;
		}
		Processer::~Processer() {
			m_thread->join();
		}
		CoroutineTask::Ptr Processer::GetCurrentCoroutineTask() {
			if (t_processer) {
				return t_processer->m_currentCoroutineTask;
			}
			return nullptr;
		}
		bool Processer::IsCoroutine() const {
			return m_currentCoroutineTask ? true : false;
		}
		size_t Processer::GetCurrentCoroutineTaskSize() const {
			return m_runningTask.size() + m_waitingTask.size();
		}
		size_t Processer::GetCurrentRunningCoroutineTaskSize() const {
			return m_runningTask.size();
		}
		uint32_t Processer::GetMaxRunningCoroutine() const {
			return m_maxRunningTask;
		}
		void Processer::SetMaxRunningCoroutine(const uint32_t& count) {
			m_maxRunningTask = count;
		}
		void Processer::AddTask(CoroutineTask::Ptr coroutineTask) {
			Mutex::Guard guard(m_mutex);
			m_waitingTask.insert(coroutineTask);
			m_sem.Post();
		}
		void Processer::AddTask(std::set<CoroutineTask::Ptr> coroutineTasks) {
			Mutex::Guard guard(m_mutex);
			for (auto it : coroutineTasks) {
				m_waitingTask.insert(it);
			}
			m_sem.Post();
		}
		void Processer::Start() {
			m_isRunning = true;
			m_thread = std::make_shared<Thread>(std::bind(&Processer::Run, this));
		}
		void Processer::Stop() {
			m_isRunning = false;
			m_sem.Post();
		}
		bool Processer::IsStopping() const {
			return !m_isRunning;
		}
		void Processer::Run(void* Ptr) {
			try {
				CRAZY_ASSERT(Ptr);
				t_processer = static_cast<Processer*>(Ptr);
				Coroutine::GetThis();
				while (t_processer->m_isRunning) {
				
					while(t_processer->m_runningTask.empty() && 
						t_processer->m_waitingTask.empty()) {
						t_processer->m_sem.Wait();
					}

					if (!t_processer->m_isRunning) {
						CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "current processer status is stopping"
							<< ", current running coroctine task count = "
							<< t_processer->m_runningTask.size()
							<< ", current waiting coroutine task count = "
							<< t_processer->m_waitingTask.size();
						break;
					}
					
					if (t_processer->m_runningTask.size() < t_processer->m_maxRunningTask &&
						t_processer->m_waitingTask.size() > 0) {
						for (auto it = t_processer->m_waitingTask.begin();
								it != t_processer->m_waitingTask.end();) {
							t_processer->m_runningTask.insert(*it);
							t_processer->m_waitingTask.erase(it++);
							if (t_processer->m_runningTask.size() >= 
									t_processer->m_maxRunningTask) {
								break;
							}
						}
					}	

					for (auto it = t_processer->m_runningTask.begin(); 
							it != t_processer->m_runningTask.end();) {
						CRAZY_INFO(CRAZY_ROOT_LOGGER()) << " ";
						(*it)->coroctine->SwapIn();
						if ((*it)->coroctine->GetCoroutineStatus()
								== CoroutineStatus::Suspend) {
							it = t_processer->m_runningTask.erase(it);
							continue;
						}
						++it;
					}
				}

			} catch (std::exception& e) {
				CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "Processer::Run() exception: "
					<< e.what() << " current running coroctine tasks count =  "
					<< t_processer->m_runningTask.size() << " waiting coroctine task count = "
					<< t_processer->m_waitingTask.size();
			} catch (...) {
				CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "Processer::Run() "
					<< " current running coroctine tasks count =  "
					<< t_processer->m_runningTask.size() << ", waiting coroctine task count = "
					<< t_processer->m_waitingTask.size();
			}
		}
		std::set<CoroutineTask::Ptr>& Processer::GetRunningTasks() {
			return m_runningTask;
		}
		std::set<CoroutineTask::Ptr>& Processer::GetWaitingTasks() {
			return m_waitingTask;
		}
}
