#include "../src/crazy.h"
#include "crazy/byte_array.h"
#include <vector>

struct Person {
	std::string name;
	int64_t age;
	std::vector<int32_t> vec;
};
REFLECTION(Person, name, age, vec);

int main () {
	Person person1{};
	person1.age = 18;
	person1.name = "keisum";
	person1.vec = {1,2,3,4,5};
	auto person1_str = crazy::byte_array::Converter::Serialize(person1);
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << person1_str;
	auto person2 = crazy::byte_array::Converter::Deserialize<Person>(person1_str);
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << person2.name;
	CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << person2.age;
	for (auto& it : person2.vec) {
		CRAZY_DEBUG(CRAZY_ROOT_LOGGER()) << it;
	}
	return 0;
}
