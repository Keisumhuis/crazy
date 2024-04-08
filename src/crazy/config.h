/**
 * @file config.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_CONFIG_H____
#define ____CRAZY_CONFIG_H____

#include <exception>
#include <stdint.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crazy/toml/toml/comments.hpp"
#include "crazy/toml/toml/utility.hpp"
#include "crazy/toml/toml/value.hpp"
#include "lexicl_cast.h"
#include "logger.h"
#include "toml/toml.hpp"

namespace crazy {

	template <typename to, typename from>
	struct TomlValueCast {};
	
	template <typename to> 
	struct TomlValueCast <to, toml::basic_value<toml::preserve_comments>> {
		to operator()(const toml::basic_value<toml::preserve_comments>& value) {
			try {
				return toml::get<to>(value);
			} catch (std::exception& e) {
				CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : "
					<< e.what();
				return to {};
			}
		}
	};
	
	template <typename from>
	struct TomlValueCast <toml::basic_value<toml::preserve_comments>, from> {
		toml::basic_value<toml::preserve_comments> operator()(const from& value) {
			try {
				return toml::value{value};
			} catch (std::exception& e) {
				CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : "
					<< e.what();
				return {};
			}
		}
	};

	class ConfigValueBase {
	public:
		using Ptr = std::shared_ptr<ConfigValueBase>;
		ConfigValueBase(const std::string& name, const std::string& description) 
			: m_name(name), m_description(description) {}
		virtual ~ConfigValueBase() {}
		void SetName(const std::string& name) { m_name = name; }
		void SetDescription(const std::string& description) { m_description = description; }
		std::string GetName() const { return m_name; }
		std::string GetDescription() const { return m_description; }
		virtual void FromToml(const toml::basic_value<toml::preserve_comments>& str) = 0;
		virtual const toml::basic_value<toml::preserve_comments> ToToml() const = 0;
		virtual const std::string GetTypeName() const = 0;	
	private:
		std::string m_name;
		std::string m_description;
	};

	template <typename type
		, typename to_toml = TomlValueCast<toml::basic_value<toml::preserve_comments>, type>
		, typename from_toml = TomlValueCast<type, toml::basic_value<toml::preserve_comments>>>
	class ConfigValue final : public ConfigValueBase {
	public:
		using Ptr = std::shared_ptr<ConfigValue>;
		ConfigValue(const std::string& name, const type& value, const std::string& description)
			: m_value(value), ConfigValueBase(name, description) {}

		void FromToml(const toml::basic_value<toml::preserve_comments>& str) override {
			m_value = from_toml()(str);
		}
		const toml::basic_value<toml::preserve_comments> ToToml() const override {
			return to_toml()(m_value);
		}
		const std::string GetTypeName() const override {
			return typeid(m_value).name();
		}

		const type GetValue() const { return m_value; }
		void SetValue(const type& value) { m_value = value; }
	private:
		type m_value;
	};
	
	class Config final {
	public:
		using Ptr = std::shared_ptr<Config>;
		template <typename type>
		static typename ConfigValue<type>::Ptr Lookup(const std::string& name
				, const type& value,const std::string& description = "") {
			auto configValue = Lookup<type>(name);
			if (configValue) {
				return configValue;
			}
			auto tmpValue = std::make_shared<ConfigValue<type>>(name, value, description);
			GetDatas()[name] = tmpValue;
			return tmpValue;
		}
		template <typename type>
		static typename ConfigValue<type>::Ptr Lookup(const std::string& name) {
			auto itConfigValue = GetDatas().find(name);
			if (GetDatas().end() == itConfigValue) {
				return nullptr;
			}
			return std::dynamic_pointer_cast<ConfigValue<type>>(itConfigValue->second);
		}
		static ConfigValueBase::Ptr LookupBase(const std::string& name) {
			auto itConfigValueBase = GetDatas().find(name);
			if (GetDatas().end() == itConfigValueBase) {
				return nullptr;
			}
			return itConfigValueBase->second;
		}
		static void LoadPath(const std::filesystem::path& path);
	private: 	
		static void loadConfig(const std::filesystem::path& path
				, std::vector<std::pair<std::string, toml::value>>& nodes);
		static void parser(const toml::value node
				, std::vector<std::pair<std::string, toml::value>>& nodes
				, const std::string& name = "");
		static std::unordered_map<std::string, ConfigValueBase::Ptr>& GetDatas();
	};
}

#endif // ! ____CRAZY_CONFIG_H____
