#include "../src/crazy.h"
#include "crazy/config.h"
#include <string>
#include <vector>


struct general {
	std::string title;
	float version;
	std::vector<std::string> names;
};
REFLECTION(general, title, version, names);

struct servers {
	std::string name;
	std::string ip;
	bool active;
};
REFLECTION(servers, name, ip, active);

static crazy::ConfigValue<std::string>::Ptr g_name 
	= crazy::Config::Lookup<std::string>("name", std::string{"crazy"}, "test name");
static crazy::ConfigValue<int16_t>::Ptr g_age 
	= crazy::Config::Lookup<int16_t>("age", int16_t(16), "test age");
static crazy::ConfigValue<std::vector<std::string>>::Ptr g_names 
	= crazy::Config::Lookup<std::vector<std::string>>("general.names", std::vector<std::string>{"keisum1", "keisum2"}, "test names");
static crazy::ConfigValue<std::vector<uint16_t>>::Ptr g_ages 
	= crazy::Config::Lookup<std::vector<uint16_t>>("ages", std::vector<uint16_t>{1, 2}, "test ages");
static crazy::ConfigValue<std::map<std::string, std::string>>::Ptr g_logging 
	= crazy::Config::Lookup<std::map<std::string, std::string>>("logging", std::map<std::string, std::string>{}, "test loggings");
static crazy::ConfigValue<general>::Ptr g_general
	= crazy::Config::Lookup<general>("general", general{}, "test general");
// static crazy::ConfigValue<std::vector<servers>>::Ptr g_servers
//	= crazy::Config::Lookup<std::vector<servers>>("servers", std::vector<servers>{}, "test servers");

int main () {

	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value names";
	for (auto& itName : g_names->GetValue()) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " << itName;
	}
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value g_ages";
	for (auto& itAge : g_ages->GetValue()) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " << itAge;
	}
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value g_logging";
	for (auto& itLogging : g_logging->GetValue()) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config key: " << itLogging.first << " -- value : " << itLogging.second;
	}

//	for (auto& itServer : g_servers->GetValue()) {
//		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config server name " << itServer.name;
//		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config server ip " << itServer.ip;
//		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config server active " << itServer.active;
//	}
	
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config person title : " << g_general->GetValue().title;
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config person version : " << g_general->GetValue().version;
	for (auto& it : g_general->GetValue().names) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config names : " << it;
	}

	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " 
		<< g_name->GetName() << " " << g_name->GetValue() << " " << g_name->GetTypeName();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " 
		<< g_age->GetName() << " " << g_age->GetValue();
	
	crazy::Config::LoadPath("../");
	
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " 
		<< g_name->GetName() << " " << g_name->GetValue() << " " << g_name->GetTypeName();
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " 
		<< g_age->GetName() << " " << g_age->GetValue();
	
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value names";
	for (auto& itName : g_names->GetValue()) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " << itName;
	}
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value g_ages";
	for (auto& itAge : g_ages->GetValue()) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value " << itAge;
	}
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config value g_logging";
	for (auto& itLogging : g_logging->GetValue()) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config key: " << itLogging.first << " -- value : " << itLogging.second;
	}
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config person title : " << g_general->GetValue().title;
	CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config person version : " << g_general->GetValue().version;
	for (auto& it : g_general->GetValue().names) {
		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config names : " << it;
	}
//	for (auto& itServer : g_servers->GetValue()) {
//		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config server name " << itServer.name;
//		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config server ip " << itServer.ip;
//		CRAZY_INFO(CRAZY_ROOT_LOGGER()) << "test config server active " << itServer.active;
//	}
}
