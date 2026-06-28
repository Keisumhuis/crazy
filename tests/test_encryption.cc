#include "crazy.h"

int32_t main() {
	CRAZY_ROOT_DEBUG() << crazy::MD5::encryption("test_123");
	CRAZY_ROOT_DEBUG() << crazy::Base64::encryption("test_123");
}
