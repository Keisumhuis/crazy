#include "../src/crazy.h"
#include "crazy/uri.h"

int main() {
	auto ptr = crazy::Uri::Create("https://baidu.com/cn/chart?userid=121212i#dasjkdlaslkdasld=234234");
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "fragment = " << ptr->GetFragment();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "host = " << ptr->GetHost();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "path = " << ptr->GetPath();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "port = " << ptr->GetPort();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "query = " << ptr->GetQuery();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "scheme = " << ptr->GetScheme();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "userinfo = " << ptr->GetUserInfo();
}
