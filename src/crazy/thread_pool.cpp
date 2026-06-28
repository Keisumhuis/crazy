#include "crazy/thread_pool.h"

namespace crazy {
	ThreadPool::ThreadPool(uint32_t size) {
		threadSize_ = size < 1 ? 1 : size;
	}
	ThreadPool::~ThreadPool() {
		stop();
	}
	void ThreadPool::start() {
		for (uint32_t i = 0; i < threadSize_; ++i) {
			auto actor = std::make_shared<ActorInterface>("thread_pool_" + std::to_string(i));
			threads_.push_back(actor);
			actor->start();
		}
	}
	void ThreadPool::stop() {
		for (auto& itThread : threads_) {
			itThread->stop();
		}
	}
	void ThreadPool::enqueueRunnable(std::function<void()> runnable) {
		getActorImplement()->enqueueFunction(runnable);
	}
	ActorInterface::ptr ThreadPool::getActorImplement() {
		return threads_[index_++ % threadSize_];
	}
}
