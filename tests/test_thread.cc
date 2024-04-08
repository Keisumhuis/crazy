#include "../src/crazy.h"
#include "crazy/thread.h"

int main () {

	crazy::Thread th1([](){
			while (true) {
				CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "thread 1";
			}
			});
	crazy::Thread th2([](){
			while (true) {
				CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "thread 2";
			}
			});

	th1.join();
	th2.join();

	return 0;
}
