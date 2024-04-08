#include "../src/crazy.h"
#include "crazy/config.h"
#include "crazy/lexicl_cast.h"
#include <ios>
#include <iostream>
#include <ostream>

int main () {

	char sztrue[5] = "true";
	char *strue = "true";

	auto szdata = crazy::lexical_cast<bool>(sztrue);	
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << szdata;
	auto sdata = crazy::lexical_cast<bool>(strue);	
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << sdata;
	auto data = crazy::lexical_cast<bool>(std::string{"true"});	
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << data;
	auto i16 = crazy::lexical_cast<int16_t>("65535");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << i16;
	auto u16 = crazy::lexical_cast<uint16_t>("65535");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << u16;
	auto i32 = crazy::lexical_cast<int32_t>("65535");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << i32;
	auto u32 = crazy::lexical_cast<uint32_t>("65535");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << u32;
	auto i64 = crazy::lexical_cast<int64_t>("65535");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << i64;
	auto u64 = crazy::lexical_cast<uint64_t>("65535");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << u64;
	auto f = crazy::lexical_cast<float>("12.3406");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << std::fixed << f;
	auto d = crazy::lexical_cast<double>("12.3406540125");
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << std::fixed << d;
	auto dstr = crazy::lexical_cast<std::string>(d);
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << dstr;
}
