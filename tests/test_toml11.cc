#include "../src/crazy/toml/toml.hpp"
#include <iostream>

void parse (toml::value value) {
	if (value.is_array()) {
		auto array = value.as_array();

		for (auto itArray : array) {
			parse(itArray);
		}

	} else if (value.is_table()) {
		auto table = value.as_table();

		for (auto itTable : table) {
			parse(itTable.second);
		}
	} else {
		std::cout << value << std::endl;
	}
}

int main () {
	toml::value node("[12,13]");
	// parse(node);
	// std::cout << node.is_array() << std::endl;
	auto array = node.as_array();
	for (auto& itArray : array) {
		std::cout << itArray << std::endl;
	}
}
