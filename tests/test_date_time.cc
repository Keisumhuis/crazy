#include "crazy.h"

#include <cstdlib>
#include <iostream>

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	crazy::DateTime dt(2023, 12, 25, 14, 30, 0, crazy::TimeZone::UTC());
	CHECK(dt.year() == 2023);
	CHECK(dt.month() == 12);
	CHECK(dt.day() == 25);
	CHECK(dt.hour() == 14);
	CHECK(dt.minute() == 30);
	CHECK(dt.second() == 0);
	CHECK(dt.toString("%Y%m%d%H%M%S") == "20231225143000");

	crazy::DateTime leap_year(2024, 2, 29, 0, 0, 0, crazy::TimeZone::UTC());
	CHECK(leap_year.isLeapYear());
	CHECK(leap_year.daysInMonth() == 29);

	auto parsed = crazy::DateTime::fromString("2026-12-09 12:21:12");
	CHECK(parsed.year() == 2026);
	CHECK(parsed.month() == 12);
	CHECK(parsed.day() == 9);

	std::cout << "date_time tests passed" << std::endl;
	return 0;
}
