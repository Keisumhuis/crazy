#include "../src/crazy.h"

int func (int i) {
	CRAZY_ASSERT(i > 0);
	std::cout << i << std::endl;
	func(--i);
}

int main () {
	func(30);	
}
