#include "../src/crazy.h"
#include "crazy/format.h"

int main() {
	std::string surl = "/abc/123/{456}";
	std::string_view svurl = "/abc/123/{456}";
	
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << crazy::fmt::format("/abc/123/{456}", 789);
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << crazy::fmt::format(surl, "789");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << crazy::fmt::format(svurl, 789);
	
	CRAZY_ASSERT("/abc/123/789" == crazy::fmt::format("/abc/123/{456}", "789"));
	CRAZY_ASSERT("/abc/123/789" == crazy::fmt::format(surl, "789"));
	CRAZY_ASSERT("/abc/123/789" == crazy::fmt::format(svurl, "789"));
}
