#include "crazy/actor_interface.h"

#include "crazy/application.h"
#include "crazy/common.h"
#include "crazy/logger.h"

namespace crazy {
	ActorInterface::ActorInterface(const std::string& name)
		: name_(name) {
	}
	ActorInterface::~ActorInterface() {
		if (thread_ && thread_->joinable()) {
			thread_->join();
		}
	}
	const std::string& ActorInterface::getName() const {
		return name_;
	}
	void ActorInterface::init() {
	}
	void ActorInterface::start() {
		if (running_.load()) {
			return;
		}
		thread_ = std::make_unique<std::thread>(&ActorInterface::run, this);
	}
	void ActorInterface::stop() {
		if (!running_.load()) {
			return;
		}
		running_.store(false);
		wakeup();
	}
	std::map<std::string, std::string> ActorInterface::helps() {
		return {
			{"print_message_queue_size", "打印消息队列长度"},
			{"print_async_task_queue_size", "打印异步任务队列长度"},
		};
	}
	void ActorInterface::enqueueMessage(MessageBase::ptr message) {
		CondMutexGuard guard(condMutex_);
		messageQueue_.push_back(message);
		wakeup();
	}
	void ActorInterface::enqueueFunction(std::function<void()> function) {
		CondMutexGuard guard(condMutex_);
		functionQueue_.push_back(function);
		wakeup();
	}
	void ActorInterface::registerAsyncTask(std::function<void()> function) {
		CondMutexGuard guard(condMutex_);
		Application::application()->enqueueRunnable(function);
		wakeup();
	}
	uint32_t ActorInterface::messageQueueSize() {
		CondMutexGuard guard(condMutex_);
		return messageQueue_.size();
	}
	uint32_t ActorInterface::asyncTaskQueueSize() {
		CondMutexGuard guard(condMutex_);
		return functionQueue_.size();
	}
	void ActorInterface::handleCommandLineMessgaBase(MessageBase::ptr request, MessageBase::ptr response) {
		auto commands = StringUtil::Split(request->getData());
		if ("print_message_queue_size" == commands[0]) {
			response->setData("当前消息队列长度：" + std::to_string(messageQueueSize()));
		}
		else if ("print_async_task_queue_size" == commands[0]) {
			response->setData("当前异步任务队列长度：" + std::to_string(asyncTaskQueueSize()));
		}
	}
	void ActorInterface::handleMessgaBase(MessageBase::ptr message) {
	}
	void ActorInterface::onRecvMessgaBase(MessageBase::ptr message) {
		if (InternalCommand::command_line_request == message->getCmd()) {
			auto response = std::make_shared<MessageBase>();
			response->setCmd(InternalCommand::command_line_response);
			response->setData("no matching command was found.");
			handleCommandLineMessgaBase(message, response);
			sendMessage(response);
		}
		else {
			handleMessgaBase(message);
		}
	}
	void ActorInterface::run() {
		running_.store(true);
		CRAZY_SYSTEM_INFO() << "actor start, actor name = " << name_ << ", thread id = " << std::this_thread::get_id();
		while (running_.load()) {
			try {

				select();

				if (!running_.load()) {
					break;
				}

				if (running_.load() && messageQueue_.empty() && functionQueue_.empty()) {
					continue;
				}

				{
					MessageBase::ptr message = nullptr;
					{
						CondMutexGuard guard(condMutex_);
						if (!messageQueue_.empty()) {
							message = std::move(messageQueue_.front());
							messageQueue_.pop_front();
						}
					}
					if (message) {
						onRecvMessgaBase(message);
					}
				}

				{
					std::function<void()> function;
					{
						CondMutexGuard guard(condMutex_);
						if (!functionQueue_.empty()) {
							function = std::move(functionQueue_.front());
							functionQueue_.pop_front();
						}
					}
					if (function) {
						function();
					}
				}
			}
			catch (const std::exception& e) {
				CRAZY_SYSTEM_ERROR() << "actor error, name = " << name_ << ", exception = " << e.what();
			}
			catch (...) {
				CRAZY_SYSTEM_ERROR() << "actor error, name = " << name_ << ", can't catch exception";
			}
		}
		running_.store(false);
	}
	void ActorInterface::sendMessage(MessageBase::ptr message) {
		message->setSource(name_);
		Application::application()->routeMessage(name_, message);
	}
}
