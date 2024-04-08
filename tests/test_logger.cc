#include "../src/crazy.h"

int main () {
	for (size_t i = 0; i < 1000; ++i) {
		CRAZY_TRACE(CRAZY_ROOT_LOGGER()) << "test CRAZY_TRACE";	
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "test CRAZY_DEBUG";	
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test CRAZY_INFO";	
		CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "test CRAZY_ERROR";	
		CRAZY_WARN(CRAZY_ROOT_LOGGER()) << "test CRAZY_WARN";	
		CRAZY_FATAL(CRAZY_ROOT_LOGGER()) << "test CRAZY_FATAL";	
	
		
		CRAZY_TRACE(CRAZY_LOGGER(system)) << "test CRAZY_TRACE";	
		CRAZY_DEBUG(CRAZY_LOGGER(system)) << "test CRAZY_DEBUG";	
		CRAZY_INFO(CRAZY_LOGGER(system)) << "test CRAZY_INFO";	
		CRAZY_ERROR(CRAZY_LOGGER(system)) << "test CRAZY_ERROR";	
		CRAZY_WARN(CRAZY_LOGGER(system)) << "test CRAZY_WARN";	
		CRAZY_FATAL(CRAZY_LOGGER(system)) << "test CRAZY_FATAL";	
	}
	return 0;
}
