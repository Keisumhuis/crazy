#include "crazy/config.h"

#include <filesystem>
#include <fstream>

#include "crazy/logger.h"
#include "crazy/utils.h"

namespace crazy {

	void Config::LoadConfigPath(const std::string& path, const std::string& suffix) {
		if (!std::filesystem::exists(path)) {
			return;
		}
		for (auto it : std::filesystem::directory_iterator(path)) {
            try {
                if (it.is_directory()) {
                    LoadConfigPath(it.path().string(), suffix);
                }
                else if (it.is_regular_file() && it.path().extension() == suffix) {
                    LoadConfigFile(it.path().string());
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                continue;
            }
		}
	}
    void Config::LoadConfigPath(const std::vector<std::string>& path, const std::string& suffix) {
        for (auto& it : path) {
            LoadConfigPath(it, suffix);
        }
    }
	void Config::LoadConfigFile(const std::string& filePath) {
		if (!std::filesystem::exists(filePath)) {
			CRAZY_SYSTEM_ERROR() << "load config file fail, file is no exist, file path = " << filePath;
			return;
		}
		if (!std::filesystem::is_regular_file(filePath)) {
			CRAZY_SYSTEM_ERROR() << "load config file fail, is not regular file, file path = " << filePath;
			return;
		}
		ParseConfigFile(filePath);
	}
    bool Config::GetBoolean(const std::string& section, const std::string& key, const bool& defaultValue) {
        if (!GetConfigValueMap().count(section) || !GetConfigValueMap()[section].count(key)) {
            return defaultValue;
        }
        return GetConfigValueMap()[section][key] == "true" ? true : false;
    }
    int64_t Config::GetIntager(const std::string& section, const std::string& key, const int64_t& defaultValue) {
        if (!GetConfigValueMap().count(section) || !GetConfigValueMap()[section].count(key)) {
            return defaultValue;
        }
        return std::stoll(GetConfigValueMap()[section][key]);
    }
    double Config::GetDouble(const std::string& section, const std::string& key, const double& defaultValue) {
        if (!GetConfigValueMap().count(section) || !GetConfigValueMap()[section].count(key)) {
            return defaultValue;
        }
        return std::stod(GetConfigValueMap()[section][key]);
    }
    std::string Config::GetString(const std::string& section, const std::string& key, const std::string& defaultValue) {
        if (!GetConfigValueMap().count(section) || !GetConfigValueMap()[section].count(key)) {
            return defaultValue;
        }
        return GetConfigValueMap()[section][key];
    }
    void Config::EreaseValue(const std::string& section, const std::string& key) {
        if (GetConfigValueMap().count(section)) {
            GetConfigValueMap()[section].erase(key);
            if (GetConfigValueMap()[section].empty()) {
                GetConfigValueMap().erase(section);
            }
        }
    }
    bool Config::HasSession(const std::string& section) {
        return GetConfigValueMap().count(section);
    }
    void Config::ParseConfigFile(const std::string& filePath) {
        std::ifstream ifs(filePath, std::ios::in);
        if (!ifs.is_open()) {
            CRAZY_SYSTEM_ERROR() << "open config file fail, file path = " << filePath;
            return;
        }

        std::string line;
        std::string currentSection = CONFIG_GLOBAL_SECTION;

        while (std::getline(ifs, line)) {

            size_t commentPos = std::string::npos;
            commentPos = line.find('#');
            if (commentPos == std::string::npos) {
                commentPos = line.find(';');
            }

            std::string contentLine = line;
            if (commentPos != std::string::npos) {
                contentLine = line.substr(0, commentPos);
            }

            auto trimmedLine = StringUtil::Trim(contentLine);
            if (trimmedLine.empty()) {
                continue;
            }

            if (trimmedLine[0] == '[' && trimmedLine[trimmedLine.size() - 1] == ']') {
                currentSection = StringUtil::Trim(trimmedLine.substr(1, trimmedLine.size() - 2));
                if (currentSection.empty()) {
                    currentSection = CONFIG_GLOBAL_SECTION;
                }
                continue;
            }

            auto delimiterPos = trimmedLine.find('=');
            if (delimiterPos == std::string::npos) {
                continue;
            }

            auto key = StringUtil::Trim(trimmedLine.substr(0, delimiterPos));
            auto value = StringUtil::Trim(trimmedLine.substr(delimiterPos + 1));

            if (!value.empty()) {
                if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"') {
                    value = value.substr(1, value.size() - 2);
                }
                else if (value.size() >= 2 && value[0] == '\'' && value[value.size() - 1] == '\'') {
                    value = value.substr(1, value.size() - 2);
                }
            }

            if (key.empty()) {
                continue;
            }
            GetConfigValueMap()[currentSection][key] = value;
        }
        ifs.close();
    }
    Config::ConfigValueMap& Config::GetConfigValueMap() {
        static ConfigValueMap g_config;
        return g_config;
    }
}
