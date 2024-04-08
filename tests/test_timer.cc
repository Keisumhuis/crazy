#include "../src/crazy.h"
#include "crazy/timer.h"

int main() {
		
	TIMER_MANAGER().AddTimer(1000, []() {
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "on timer 1";	
	}, true);

	TIMER_MANAGER().AddTimer(500, []() {
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "on timer 0.5";	
	}, false);
	TIMER_MANAGER().AddTimer(2000, []() {
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "on timer 2";	
	}, true);
	while(1) {sleep(1);}
}
