#include "crazy.h"

int main() {
	crazy::Config::LoadConfigPath("../../../");
	auto host = crazy::Config::GetString("server", "host", "123.123.123.123");
	auto quoted_with_spaces = crazy::Config::GetString("special_cases", "single_quoted");
	return 0;
}
