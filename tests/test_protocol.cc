#include "crazy.h"

namespace protocol {
	struct parent {
		std::string name;
		int32_t age;
		REFLECTION(name, age);
	};

	struct person {
		parent parents[10];
		REFLECTION(parents);
	};
}

int32_t main() {
	protocol::person _person;
	protocol::parent father;
	father.name = "123123";
	father.age = 12;
	protocol::parent mathor;
	mathor.name = "123123";
	mathor.age = 12;

	auto json_str = crazy::json::Converter::Serializable(_person);
	auto _person_dec = crazy::json::Converter::Deserializable<protocol::person>(json_str);
}
