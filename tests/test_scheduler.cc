#include "../src/crazy.h"
#include "crazy/coroutine.h"
#include "crazy/util.h"

int main () {
	// for (size_t i = 0; i < 5; ++i){
		// co [](){
		// 	while(1) {
		// 		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "first coroutine "
		// 			<< "coroutine id = " << crazy::GetCoroutineId();
		// 		sleep(1);
		// 	}
		// };
	// }
	co [](){
		while(1) {
			CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "second coroutine " 
					<< "coroutine id = " << crazy::GetCoroutineId();
			sleep(1);
		}
	};
	
	while(1) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "main func";
		sleep(1);
	}
}
