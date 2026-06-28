#include "crazy.h"

class TestService : public crazy::ActorInterface {
public:
	using crazy::ActorInterface::ActorInterface;
	/**
	 * @brief 初始化.
	 */
	void init() override {
	}
	/**
	 * @brief 获取帮助手册.
	 */
	std::map<std::string, std::string> helps() override {
		return {};
	}

protected:
	/**
	 * @brief 处理命令行消息.
	 * @param message 消息
	 */
	void handleCommandLineMessgaBase(crazy::MessageBase::ptr request, crazy::MessageBase::ptr response) override {
	}
	/**
	 * @brief 处理普通消息.
	 * @param message 消息
	 */
	void handleMessgaBase(crazy::MessageBase::ptr message) override {
		if (message->getCmd() == 999) {
			auto response = message->createResponse();
			response->setData("123123123123123123123");
			sendMessage(response);
		}
	}
};

int32_t main(int32_t argc, char** argv) {
	crazy::Application app(argc, argv);

	app.registerActor<crazy::ServiceActor>("service_actor");
	app.registerActor<TestService>("test_service");

	app.exec();
}
