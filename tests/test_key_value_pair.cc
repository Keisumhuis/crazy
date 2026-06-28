#include "crazy.h"

struct Test {
	std::vector<crazy::KeyValuePair> toItem() {
		return {
			{ "11", std::string("dasdasdasd") },
			{ "22", std::string("dasdasdasd") }
		};
	}
};

int32_t main() {
	Test test;
	CRAZY_ROOT_DEBUG() << crazy::datacsv(test.toItem());
}

