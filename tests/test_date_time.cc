#include "crazy.h"

#include <cassert>
#include <iostream>

int main() {
	crazy::DateTime dt(2023, 12, 25, 14, 30, 0, crazy::TimeZone::UTC());
	assert(dt.year() == 2023);
	assert(dt.month() == 12);
	assert(dt.day() == 25);
	assert(dt.hour() == 14);
	assert(dt.minute() == 30);
	assert(dt.second() == 0);
	assert(dt.toString("%Y%m%d%H%M%S") == "20231225143000");

	crazy::DateTime leap_year(2024, 2, 29, 0, 0, 0, crazy::TimeZone::UTC());
	assert(leap_year.isLeapYear());
	assert(leap_year.daysInMonth() == 29);

	auto parsed = crazy::DateTime::fromString("2026-12-09 12:21:12");
	assert(parsed.year() == 2026);
	assert(parsed.month() == 12);
	assert(parsed.day() == 9);

	std::cout << "date_time tests passed" << std::endl;
	return 0;
}
