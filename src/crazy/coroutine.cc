#include "coroutine.h"
#include <exception>
#include <functional>

namespace crazy {

	static ConfigValue<uint64_t>::Ptr g_coroutine_stack_size = 
		Config::Lookup("coroutine.stack_size", uint64_t( 1024 * 1024 ), "coroutine stack size");

	static thread_local uint64_t t_scoroutine_count = 0;
	static thread_local uint64_t t_scoroutine_id = 0;
	static thread_local Coroutine* t_coroutine = nullptr;
	static thread_local Coroutine::Ptr t_master_coroutine = nullptr;

	Coroutine::Coroutine() {
		SetThis(this);
		if (getcontext(&m_ctx)) {
			CRAZY_ASSERT_WITH_LOG(false, "getcontext() fail");
		}
		++t_scoroutine_count;
	}
	Coroutine::Coroutine(std::function<void()> cb, size_t stack_size,  bool using_caller) 
		: m_cb(cb)
		, m_stackSize(stack_size) 
		, m_coroutineId(++t_scoroutine_id) {
		++t_scoroutine_count;
		m_stackSize = m_stackSize == 0 ? g_coroutine_stack_size->GetValue() : m_stackSize;	
		
		m_pStack = Allocator::Alloc(m_stackSize);
	       	if (getcontext(& m_ctx)) {
			CRAZY_ASSERT_WITH_LOG(false, "getcontext() fail");	
		}
		m_ctx.uc_link = nullptr;
		m_ctx.uc_stack.ss_size = m_stackSize;
		m_ctx.uc_stack.ss_sp = m_pStack;

		makecontext(&m_ctx, &Coroutine::MasterCoroutine, 0);
	}
	Coroutine::~Coroutine() {
		--t_scoroutine_count;
		if (m_pStack) {
			Allocator::Dealloc(m_pStack, m_stackSize);
		} else {
			auto cur_coroutine = t_coroutine;
			if (this == cur_coroutine) {
				SetThis(nullptr);
			}
		}
	}
	void Coroutine::Back() {
		SetThis(t_master_coroutine.get());
		if (swapcontext(&m_ctx, &t_master_coroutine->m_ctx)) {
			CRAZY_ASSERT_WITH_LOG(false, "swapcontext() fail");
		}	
	}
	void Coroutine::SwapOut() {	
		t_coroutine->m_status = CoroutineStatus::Ready;
		t_master_coroutine->m_status = CoroutineStatus::Running;
		SetThis(t_master_coroutine.get());
		if (swapcontext(&m_ctx, &t_master_coroutine->m_ctx)) {
			CRAZY_ASSERT_WITH_LOG(false, "swapcontext() fail");
		}
	}
	void Coroutine::SwapIn() {
		t_coroutine->m_status = CoroutineStatus::Running;
		t_master_coroutine->m_status = CoroutineStatus::Ready;
		SetThis(this);
		if (swapcontext(&t_master_coroutine->m_ctx, &m_ctx)) {
			CRAZY_ASSERT_WITH_LOG(false, "swapcontext() fail");	
		}
	}
	CoroutineStatus Coroutine::GetCoroutineStatus() {
		return m_status;
	}
	uint64_t Coroutine::GetCoroutineId() {
		if (t_coroutine) {
			return t_coroutine->m_coroutineId;
		}
		return 0;
	}
	void Coroutine::Yield() {
		auto cur_coroutine = GetThis();
		cur_coroutine->SwapOut();
	}
	void Coroutine::Resume() {
		auto cur_coroutine = GetThis();
		cur_coroutine->SwapIn();
	}
	void Coroutine::SetThis(Coroutine* coroutine) {
		t_coroutine = coroutine;
	}
	Coroutine::Ptr Coroutine::GetThis() {
		if (t_coroutine) {
			return t_coroutine->shared_from_this();
		}
		Coroutine::Ptr master_coroutine(new Coroutine);
		CRAZY_ASSERT(master_coroutine.get() == t_coroutine);
		t_master_coroutine = master_coroutine;
		return t_master_coroutine;
	}
	void Coroutine::MasterCoroutine() {	
		auto cur_coroutine = GetThis();
		CRAZY_ASSERT(cur_coroutine);
		try {
			if (cur_coroutine->m_cb) {
				cur_coroutine->m_cb();
			}
			cur_coroutine->m_status = CoroutineStatus::Suspend;
			cur_coroutine->m_cb = nullptr;
		} catch (std::exception& e) {
			CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "coroutine exception : " 
				<< cur_coroutine->m_coroutineId
				<< " , what : " << e.what();
		} catch (...) {
			CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "coroutine exception : " 
				<< cur_coroutine->m_coroutineId;
		}

		auto raw_ptr = cur_coroutine.get();
		cur_coroutine.reset();
		raw_ptr->Back();

		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "never call this";
	}
}
