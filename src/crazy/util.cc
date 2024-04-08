#include "util.h"

#include <sys/syscall.h>
#include <sys/types.h>

#include "coroutine.h"

namespace crazy {


	const uint64_t GetCurrentSS() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count() / 1000;
	}
	const uint64_t GetCurrentMS() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
	}
	const uint64_t GetCurrentUS() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count() / 1000;
	}
	const uint64_t GetCoroutineId() {
		return Coroutine::GetThis()->GetCoroutineId();
	}
	const uint64_t GetThreadId() {
		return syscall(SYS_gettid);
	}
}
