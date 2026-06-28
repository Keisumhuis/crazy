#include "crazy.h"

int main () {
	for (;;) {
		CRAZY_TRACE(root) << "test CRAZY_TRACE";	
		CRAZY_DEBUG(root) << "test CRAZY_DEBUG";
		CRAZY_INFO(root) << "test CRAZY_INFO";
		CRAZY_ERROR(root) << "test CRAZY_ERROR";	
		CRAZY_WARN(root) << "test CRAZY_WARN";	
		CRAZY_FATAL(root) << "test CRAZY_FATAL";	
	
		
		CRAZY_TRACE(system) << "test CRAZY_TRACE";	
		CRAZY_DEBUG(system) << "test CRAZY_DEBUG";	
		CRAZY_INFO(system) << "test CRAZY_INFO";	
		CRAZY_ERROR(system) << "test CRAZY_ERROR";	
		CRAZY_WARN(system) << "test CRAZY_WARN";	
		CRAZY_FATAL(system) << "test CRAZY_FATAL";	
	}
	return 0;
}
