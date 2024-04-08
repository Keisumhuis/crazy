#include "crazy.h"

int main() {
    while (1) {
        sleep(10);
        CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << "hook weak";
    }
    
}