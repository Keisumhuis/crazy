#include "config.h"
#include <algorithm>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace crazy {

void Config::LoadPath(const std::filesystem::path& path) {
	std::vector<std::pair<std::string, toml::value>> nodes;
	loadConfig(path, nodes);

	for (auto& itNode : nodes) {
		auto spConfigValue = LookupBase(itNode.first);
		if (spConfigValue) {
			spConfigValue->FromToml(itNode.second);
		}
	}
}

void Config::loadConfig(const std::filesystem::path& path
		, std::vector<std::pair<std::string, toml::value>>& nodes) {
	if (!std::filesystem::exists(path)) {
		throw std::logic_error("config path is no exit, " + path.string());
	}
	for (auto& itPath : std::filesystem::directory_iterator(path) ) {
		if (itPath.is_directory()) {
			loadConfig(itPath.path(), nodes);
		} else if (itPath.is_regular_file() 
				&& itPath.path().extension() == ".toml") {
			auto node = toml::parse(itPath.path());

			parser(node, nodes);
		}
	}
}

void Config::parser(const toml::value node
		, std::vector<std::pair<std::string, toml::value>>& nodes
		, const std::string& name) {
	if (node.is_table()) {
		auto table = node.as_table();
		for (auto& itTable : table) {
			std::string nodeName = name.empty() ? itTable.first : name + "." + itTable.first;
			parser(itTable.second, nodes, nodeName);
		}
		nodes.push_back(std::pair<std::string, toml::value>(name, node));
	} else {
		nodes.push_back(std::pair<std::string, toml::value>(name, node));
	}
}

std::unordered_map<std::string, ConfigValueBase::Ptr>& Config::GetDatas() {
	static std::unordered_map<std::string, ConfigValueBase::Ptr> datas;
	return datas;
}

}
