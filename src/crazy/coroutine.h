/**
 * @file coroutine.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_COROUTINE_H____
#define ____CRAZY_COROUTINE_H____

#include <ucontext.h>

#include <atomic>
#include <functional>
#include <memory>

#include "allocator.h"
#include "assert.h"
#include "config.h"
#include "logger.h"

namespace crazy {

	enum class CoroutineStatus {
		Init, Ready, Running, Suspend
	};

	class Coroutine : public std::enable_shared_from_this<Coroutine>{
	private:
		Coroutine();
	public:
		using Ptr = std::shared_ptr<Coroutine>;
		Coroutine(std::function<void()> cb, size_t stack_size = 0, bool using_caller = false);
		~Coroutine();
		void Back();
		void SwapOut();
		void SwapIn();
		CoroutineStatus GetCoroutineStatus();
	public:		
		static uint64_t GetCoroutineId();
		static void Yield();
		static void Resume();
		static void SetThis(Coroutine* coroutine);
		static Coroutine::Ptr GetThis();
		static void MasterCoroutine();
	private:
		ucontext_t m_ctx;
		int32_t m_coroutineId = 0;
		std::function<void()> m_cb;
		size_t m_stackSize = 0;
		void* m_pStack = nullptr;
		CoroutineStatus m_status = CoroutineStatus::Init;
	};
}

#endif // ! ____CRAZY_COROUTINE_H____

