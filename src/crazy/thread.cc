#include "thread.h"
#include <cstring>
#include <stdexcept>

namespace crazy {

	static thread_local std::string t_thread_name = "UNKNOW";
	static thread_local Thread* t_pthread = nullptr;
	
	Thread::Thread(std::function<void ()> func, const std::string& name) 
		: m_cbfunc(func), m_name(name) {
		m_name = name.empty() ? "UNKNOW" : name;
		auto rt = pthread_create(&m_thread, nullptr, &Thread::Run, this);
		if (rt) {
			CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "pthread create thread fail, rt = " << rt;
			throw std::logic_error("pthread create thread fail");
		}
		m_sem.Wait();
	}
	Thread::~Thread() {
		if (m_thread) {
			pthread_detach(m_thread);
		}
	}
	void Thread::join() {
		auto rt = pthread_join(m_thread, nullptr);
		if (rt) {
			CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "pthread join thread fail";
			throw std::logic_error("pthread join thread fail");
		}
		m_thread = 0;
	}
	const Thread* Thread::GetThis() {
		return t_pthread;
	}
	const std::string Thread::GetName() {
		return t_thread_name;
	}
	void Thread::SetName(const std::string& name) {
		if (t_pthread) {
			t_pthread->m_name = name;
			t_thread_name = name;
			pthread_setname_np(t_pthread->m_thread, t_pthread->m_name.substr(0, 15).c_str());
		}
	}
	void* Thread::Run(void *arg) {
		auto _thread = static_cast<Thread*>(arg);
		t_pthread = _thread;
		t_thread_name = _thread->m_name;

		pthread_setname_np(_thread->m_thread, _thread->m_name.substr(0, 15).c_str());
		std::function<void()> cb;
		cb.swap(_thread->m_cbfunc);
		_thread->m_sem.Post();

		cb();
		return nullptr;
	}

}
