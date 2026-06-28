#include "crazy.h"

class TestClient : public crazy::ActorInterface {
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
		return {
			{"send_test", "发送测试指令"},
		};
	}

protected:
	/**
	 * @brief 处理命令行消息.
	 * @param message 消息
	 */
	void handleCommandLineMessgaBase(crazy::MessageBase::ptr request, crazy::MessageBase::ptr response) override {
		auto commands = crazy::StringUtil::Split(request->getData());
		if ("send_test" == commands[0]) {
			auto message = std::make_shared<crazy::MessageBase>();
			message->setCmd(999);
			sendMessage(message);
			response->setData("执行成功");
		}
	}
	/**
	 * @brief 处理普通消息.
	 * @param message 消息
	 */
	void handleMessgaBase(crazy::MessageBase::ptr message) override {
		if (message->getCmd() == 999) {
			CRAZY_ROOT_DEBUG() << message->getData();
		}
	}
};


int32_t main(int32_t argc, char** argv) {
	crazy::Application app(argc, argv);

	app.registerActor<crazy::ClientActor>("client");

	app.exec();
}
