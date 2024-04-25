#include "../src/crazy.h"
#include "crazy/coroutine.h"
#include "crazy/thread.h"
#include "crazy/util.h"
#include <iostream>

void RunInCoroutine() {
	for (size_t i = 0; i < 5; ++i) {
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "run in coroutine begin";
		crazy::Coroutine::YieldToReady();
	}
}

int main () {
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "main begin";
	crazy::Coroutine::GetThis();
	crazy::Coroutine::Ptr coroutine(new crazy::Coroutine(&RunInCoroutine)); 	
	for (size_t i = 0; i < 5; ++i) {
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "run in main begin";
		coroutine->SwapIn();
	}
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "main end";
}
