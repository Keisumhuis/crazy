#include "crazy.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
	const char* getEnv(const char* name) {
		const char* value = std::getenv(name);
		return (value && *value) ? value : nullptr;
	}
}

int main() {
	const char* host = getEnv("CRAZY_MYSQL_HOST");
	const char* user = getEnv("CRAZY_MYSQL_USER");
	const char* password = getEnv("CRAZY_MYSQL_PASSWORD");
	const char* database = getEnv("CRAZY_MYSQL_DB");
	const char* portText = getEnv("CRAZY_MYSQL_PORT");

	if (!host || !user || !password) {
		std::cout << "mysql test skipped: CRAZY_MYSQL_HOST/USER/PASSWORD not set" << std::endl;
		return 0;
	}

	uint32_t port = 3306;
	if (portText) {
		port = static_cast<uint32_t>(std::strtoul(portText, nullptr, 10));
	}

	auto connection = std::make_shared<crazy::MySQLConnection>();
	if (!connection->connect(host, user, password, database ? database : "", port)) {
		CRAZY_ROOT_ERROR() << "connect mysql fail, error = " << connection->get_errno()
			<< ", error message = " << connection->get_error_message();
		return 1;
	}

	auto result = connection->exec("SELECT 1");
	assert(result != nullptr);
	assert(result->isValid());

	std::cout << "mysql test passed" << std::endl;
	return 0;
}
