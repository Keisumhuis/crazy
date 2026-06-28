#include "crazy.h"

namespace protocol {
	struct Json {
		int32_t iiii;
		REFLECTION(iiii);
	};
}


int32_t main() {
	CRAZY_ROOT_DEBUG() << crazy::json::Converter::Serializable(protocol::Json{});
}

