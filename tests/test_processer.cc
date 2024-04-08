#include "../src/crazy.h"
#include "crazy/coroutine.h"
#include "crazy/processer.h"
#include <functional>
#include <memory>

void test_coroutine() {
	for (size_t i = 0; i < 5; ++i) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "coroutinei run count : " << ++i;
		crazy::Coroutine::Yield();
	}
}

int main() {
	crazy::Processer processer(256, 10);
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "begin main";
	for (auto i = 0; i < 10; ++i) {
		crazy::CoroutineTask::Ptr task(new crazy::CoroutineTask());
		task->coroctine = std::make_shared<crazy::Coroutine>(std::bind(&test_coroutine));
		task->func = &test_coroutine;
		processer.AddTask(task);
	}
	processer.Start();

	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "end main";
	while (1) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "main while";
		sleep(1);
	}
}
