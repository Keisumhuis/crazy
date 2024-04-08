#include "../src/crazy.h"
#include "crazy/mutex.h"
#include <thread>

int main () {
	
	crazy::Mutex mutex;
	int index = 0;
	std::thread th([&mutex, &index](){
			
		for (size_t i = 0; i < 100; ++i){
			crazy::MutexGuard guard(mutex);
			CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "index : " << index++;
		}
			
	});

	for (size_t i = 0; i < 100; ++i){
		crazy::MutexGuard guard(mutex);
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "index : " << index++;
	}

	th.join();
	return 0;
}
