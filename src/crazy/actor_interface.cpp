#include "crazy/actor_interface.h"

#include "crazy/application.h"
#include "crazy/common.h"
#include "crazy/logger.h"
#include "crazy/utils.h"

namespace crazy {
	ActorInterface::ActorInterface(const std::string& name)
		: name_(name) {
	}
	ActorInterface::~ActorInterface() {
		stop();
		if (thread_ && thread_->joinable()) {
			if (thread_->get_id() == std::this_thread::get_id()) {
				thread_->detach();
			}
			else {
				thread_->join();
			}
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
		running_.store(true);
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
		if (IsCommandLineMessage(message)) {
			commandLineMessageQueue_.push_back(message);
		}
		else {
			messageQueue_.push_back(message);
		}
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
	void ActorInterface::registerEventOnThread(int32_t fd,
		SelectorEventType type, std::function<void()> callback) {
		enqueueFunction([this, fd, type, callback = std::move(callback)]() mutable {
			registerEvent(fd, type, std::move(callback));
		});
	}
	void ActorInterface::unregisterEventOnThread(int32_t fd, SelectorEventType type) {
		enqueueFunction([this, fd, type]() {
			unregisterEvent(fd, type);
		});
	}
	void ActorInterface::cancelEventOnThread(int32_t fd, std::function<void()> callback) {
		enqueueFunction([this, fd, callback = std::move(callback)]() {
			cancelEvent(fd);
			if (callback) {
				callback();
			}
		});
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
		if (commands.empty()) {
			return;
		}
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
			response->setSessionId(message->getSessionId());
			response->setComment(message->getComment());
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
		ThreadUtil::SetThreadName(name_);
		CRAZY_SYSTEM_INFO() << "actor start, actor name = " << name_ << ", thread id = " << std::this_thread::get_id();
		while (running_.load()) {
			try {

				select();

				if (!running_.load()) {
					break;
				}

				{
					CondMutexGuard guard(condMutex_);
					if (running_.load() && commandLineMessageQueue_.empty() && messageQueue_.empty() && functionQueue_.empty()) {
						continue;
					}
				}

				{
					MessageBase::ptr message = nullptr;
					{
						CondMutexGuard guard(condMutex_);
						if (!commandLineMessageQueue_.empty()) {
							message = std::move(commandLineMessageQueue_.front());
							commandLineMessageQueue_.pop_front();
						}
					}
					if (message) {
						onRecvMessgaBase(message);
					}
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
	bool ActorInterface::IsCommandLineMessage(const MessageBase::ptr& message) {
		return message && (message->getCmd() == InternalCommand::command_line_request || message->getCmd() == InternalCommand::command_line_response);
	}
}
