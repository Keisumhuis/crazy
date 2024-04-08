/**
 * @file rapidjson_reflection.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_RAPIDJSON_REFLECTION_H____
#define ____CRAZY_RAPIDJSON_REFLECTION_H____

#include <array>
#include <deque>
#include <list>
#include <map>
#include <optional>
#include <stack>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "reflection.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace crazy::json {

    class Converter;
    class Serialize {
    	friend class Converter;
    public:

    	template<typename type>
    	constexpr void add_from(const char* key, const type& value) {
    		set_key(key);
    		write_value(value);
    	}

    	template<typename type>
    	constexpr void add_from(const char* key, const std::optional<type>& value) {
    		if (value.has_value()) {
    			set_key(key);
    			write_value<type>(value.value());
    		}
    	}

    	template<typename object>
    	auto serialize(const object& value) -> std::string {
    		_string_buffer_writer.Reset(_string_buffer);
    		write_value(value);
    		return _string_buffer.GetString();
    	}

    private:
    	auto set_key(const char* key) -> void {
    		if (key != nullptr) {
    			_string_buffer_writer.Key(key);
    		}
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, bool>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Bool(value);
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, int32_t> ||
    		std::is_same_v<type, int16_t> ||
    		std::is_same_v<type, int8_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Int(value);
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, uint32_t> ||
    		std::is_same_v<type, uint16_t> ||
    		std::is_same_v<type, uint8_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Uint(value);
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, int64_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Int64(value);
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, uint64_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Uint64(value);
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, float_t> ||
    		std::is_same_v<type, double_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Double(value);
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, std::string>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.String(value.c_str());
    	}

    	template <typename type, std::enable_if_t<std::is_same_v<type, char*> ||
    		std::is_same_v<type, const char*>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.String(value);
    	}

    	template <typename type, std::enable_if_t<std::is_enum_v<type>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.Int64((int64_t)value);
    	}

    	template <typename type, size_t N, template<typename, size_t> class wrapper = std::array,
    		std::enable_if_t<std::is_same_v<wrapper<type, N>, std::array<type, N>> == true, void> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const wrapper<type, N>& value) -> void {
    		_string_buffer_writer.StartArray();
    		for (auto& object : value) {
    			add_from(nullptr, value);
    		}
    		_string_buffer_writer.EndArray();
    	}

    	template <typename type, size_t N>
    	[[maybe_unused]]
    	constexpr auto write_value(const type(&value)[N]) -> void {
    		_string_buffer_writer.StartArray();
    		for (auto& object : value) {
    			add_from(nullptr, value);
    		}
    		_string_buffer_writer.EndArray();
    	}

    	template <typename type, typename _alloc, template <typename, typename> class wrapper = std::vector,
    		std::enable_if_t<std::is_same_v<wrapper<type, _alloc>, std::vector<type, _alloc>> ||
    		std::is_same_v<wrapper<type, _alloc>, std::list<type, _alloc>> ||
    		std::is_same_v<wrapper<type, _alloc>, std::deque<type, _alloc>>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const wrapper <type, _alloc>& value) -> void {
    		_string_buffer_writer.StartArray();
    		for (auto& object : value) {
    			add_from(nullptr, object);
    		}
    		_string_buffer_writer.EndArray();
    	}

    	template <typename type, template <typename> class wrapper = std::stack,
    		std::enable_if_t<std::is_same_v<wrapper<type>, std::stack<type>>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const wrapper<type>& value) -> void {
    		_string_buffer_writer.StartArray();
    		for (auto& object : value) {
    			add_from(nullptr, object);
    		}
    		_string_buffer_writer.EndArray();
    	}

    	template <typename type, template <typename> class wrapper = std::set,
    		std::enable_if_t<std::is_same_v<wrapper<type>, std::set<type>> ||
    		std::is_same_v<wrapper<type>, std::unordered_set<type>>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const wrapper<type>& value) -> void {
    		_string_buffer_writer.StartArray();
    		for (auto& object : value) {
    			add_from(nullptr, object);
    		}
    		_string_buffer_writer.EndArray();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, char*> ||
    		std::is_same_v<key, const char*>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(_key, _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, std::string>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(_key.c_str(), _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, int8_t> ||
    		std::is_same_v<key, uint8_t> ||
    		std::is_same_v<key, int16_t> ||
    		std::is_same_v<key, uint16_t> ||
    		std::is_same_v<key, int32_t> ||
    		std::is_same_v<key, uint32_t> ||
    		std::is_same_v<key, int64_t> ||
    		std::is_same_v<key, uint64_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(std::to_string(_key).c_str(), _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_enum_v<key>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(std::to_string((int64_t)_key).c_str(), _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, char*> ||
    		std::is_same_v<key, const char*>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::unordered_map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(_key, _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, std::string>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::unordered_map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(_key.c_str(), _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, int8_t> ||
    		std::is_same_v<key, uint8_t> ||
    		std::is_same_v<key, int16_t> ||
    		std::is_same_v<key, uint16_t> ||
    		std::is_same_v<key, int32_t> ||
    		std::is_same_v<key, uint32_t> ||
    		std::is_same_v<key, int64_t> ||
    		std::is_same_v<key, uint64_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::unordered_map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(std::to_string(_key).c_str(), _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_enum_v<key>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const std::unordered_map<key, val>& value) -> void {
    		_string_buffer_writer.StartObject();
    		for (auto& [_key, _val] : value) {
    			add_from(std::to_string((int64_t)_key).c_str(), _val);
    		}
    		_string_buffer_writer.EndObject();
    	}

    	template<typename type,
    		std::enable_if_t<(
    		!std::is_same_v<type, int8_t> &&
    		!std::is_same_v<type, int16_t> &&
    		!std::is_same_v<type, int32_t> &&
    		!std::is_same_v<type, int64_t> &&
    		!std::is_same_v<type, uint8_t> &&
    		!std::is_same_v<type, uint16_t> &&
    		!std::is_same_v<type, uint32_t> &&
    		!std::is_same_v<type, uint64_t> &&
    		!std::is_same_v<type, bool> &&
    		!std::is_same_v<type, float_t> &&
    		!std::is_same_v<type, double_t> &&
    		!std::is_same_v<type, char*> &&
    		!std::is_same_v<type, const char*> &&
    		!std::is_same_v<type, std::string> &&
    		!std::is_enum_v<type> ), int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto write_value(const type& value) -> void {
    		_string_buffer_writer.StartObject();
    		members_reflection<type>::to_json(*this, value);
    		_string_buffer_writer.EndObject();
    	}

    private:
    	rapidjson::StringBuffer _string_buffer;
    	rapidjson::Writer<rapidjson::StringBuffer> _string_buffer_writer;
    };

    class Deserialize;
    class Value {
    	friend class Deserialize;
    public:
    	template <typename type>
    	constexpr auto get_from(const char* key, type& value) -> void {
    		if (_innerValue->IsNull()) {
    			return;
    		} else if (auto it = _innerValue->FindMember(key); it != _innerValue->MemberEnd()) {
    			Value(key, &it->value).get_to<type>(value);
    		} else {
    			value = type {};
    		}
    	}

    	template <typename type>
    	constexpr auto get_from(const char* key, std::optional<type>& value) -> void {
    		if (auto it = _innerValue->FindMember(key); it != _innerValue->MemberEnd()) {
    			Value(key, &it->value).get_to<type>(value);
    		} else {
    			value = nullptr;
    		}
    	}

    	template<typename type>
    	auto get_to(std::optional<type>& value) -> void {
    		if (_innerExist) {
    			get_value<type>(value.value());
    		} else {
    			value = nullptr;
    		}
    	}

    	template<typename type>
    	auto get_to(type& value) -> void {
    		get_value(value);
    	}

    	explicit Value(const char* key) {
    		_innerKey = key;
    		_innerValue = nullptr;
    		_innerExist = true;
    	}

    	explicit Value(const char* key, rapidjson::GenericValue<rapidjson::UTF8<>>* value) {
    		_innerKey = key;
    		_innerValue = value;
    		_innerExist = true;
    	}
    private:
    	template<typename type>
    	[[nodiscard]]
    	constexpr auto get() -> type {
    		type value {};
    		get_value(value);
    		return value;
    	}

    	template<typename type, std::enable_if_t<
    		std::is_same_v<type, int8_t> ||
    		std::is_same_v<type, uint8_t> ||
    		std::is_same_v<type, int16_t> ||
    		std::is_same_v<type, uint16_t> ||
    		std::is_same_v<type, int32_t> ||
    		std::is_same_v<type, uint32_t> ||
    		std::is_same_v<type, int64_t> ||
    		std::is_same_v<type, uint64_t> ||
    		std::is_same_v<type, float_t> ||
    		std::is_same_v<type, double_t>, int> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(type& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = type {};
    			return;
    		}
    		if (_innerValue->IsInt()) {
    			value = (type)_innerValue->GetInt();
    		} else if (_innerValue->IsUint()) {
    			value = (type)_innerValue->GetUint();
    		} else if (_innerValue->IsInt64()) {
    			value = (type)_innerValue->GetInt64();
    		} else if (_innerValue->IsUint64()) {
    			value = (type)_innerValue->GetUint64();
    		} else if (_innerValue->IsDouble()) {
    			value = (type)_innerValue->GetDouble();
    		}
    	}

    	template<typename type, std::enable_if_t<
    		std::is_same_v<type, bool>, int> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(type& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = type {};
    		} else if (_innerValue->IsBool()) {
    			value = _innerValue->GetBool();
    		}
    	}

    	template<typename type, std::enable_if_t<
    		std::is_same_v<type, char*>, int> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(type& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = type {};
    		} else if (_innerValue->IsString()) {
    			value = const_cast<char*>( _innerValue->GetString() );
    		}
    	}

    	template<typename type, std::enable_if_t<
    		std::is_same_v<type, std::string>, int> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(type& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = type {};
    		} else if (_innerValue->IsString()) {
    			value = _innerValue->GetString();
    		}
    	}

    	template<typename type, std::enable_if_t<
    		std::is_enum_v<type>, int> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(type& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = type {};
    		}
    		value = (type)_innerValue->GetInt64();
    	}

    	template<typename type, size_t N, template<typename, size_t> class wrapper = std::array,
    		std::enable_if_t<std::is_same_v<wrapper<type, N>, std::array<type, N>>, int> = 0>
    	[[maybe_unused]]
    	auto get_value(wrapper<type, N>& value) -> void {
    		size_t size = 0;
    		if (_innerValue->IsNull()) {
    			wrapper<type, N>().swap(value);
    			return;
    		}
    		if (!_innerValue->IsArray()) {
    			value = wrapper<type, N> {};
    			return;
    		}
    		if (_innerValue->Size() != N) {
    			size = _innerValue->Size() < N ? _innerValue->Size() : N;
    		}
    		auto trueValue = _innerValue->GetArray();
    		for (size_t i = 0; i < size; ++i) {
    			Value _value(nullptr, &trueValue[i]);
    			_value.get_from(value[i]);
    		}
    	}

    	template<typename type, typename _alloc,
    		template<typename, typename> class wrapper = std::vector, std::enable_if_t<
    		std::is_same_v<wrapper<type, _alloc>, std::vector<type, _alloc>> ||
    		std::is_same_v<wrapper<type, _alloc>, std::list<type, _alloc>> ||
    		std::is_same_v<wrapper<type, _alloc>, std::deque<type, _alloc>>, int> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(wrapper<type, _alloc>& value) -> void {
    		if (_innerValue->IsNull() || !_innerValue->IsArray()) {
    			value = wrapper<type, _alloc> {};
    			return;
    		}
    		type tmpValue;
    		auto trueValue = _innerValue->GetArray();
    		for (size_t i = 0; i < trueValue.Size(); ++i) {
    			Value _value(nullptr, &trueValue[i]);
    			_value.get_to(tmpValue);
    			value.push_back(tmpValue);
    		}
    	}

    	template<typename type>
    	[[maybe_unused]]
    	constexpr auto get_value(std::set<type>& value) -> void {
    		if (_innerValue->IsNull() || !_innerValue->IsArray()) {
    			value = std::set<type> {};
    			return;
    		}
    		type tmpValue;
    		auto trueValue = _innerValue->GetArray();
    		for (size_t i = 0; i < trueValue.Size(); ++i) {
    			Value _value(nullptr, &trueValue[i]);
    			_value.get_to(tmpValue);
    			value.insert(tmpValue);
    		}
    	}

    	template<typename type>
    	[[maybe_unused]]
    	constexpr auto get_value(std::unordered_set<type>& value) -> void {
    		if (_innerValue->IsNull() || !_innerValue->IsArray()) {
    			value = std::unordered_set<type> {};
    			return;
    		}
    		type tmpValue;
    		auto trueValue = _innerValue->GetArray();
    		for (size_t i = 0; i < trueValue.Size(); ++i) {
    			Value _value(nullptr, &trueValue[i]);
    			_value.get_to(tmpValue);
    			value.insert(tmpValue);
    		}
    	}

    	template<typename type>
    	[[maybe_unused]]
    	constexpr auto get_value(std::stack<type>& value) -> void {
    		if (_innerValue->IsNull() || !_innerValue->IsArray()) {
    			value = std::vector<type> {};
    			return;
    		}
    		type tmpValue;
    		auto trueValue = _innerValue->GetArray();
    		for (size_t i = 0; i < trueValue.Size(); ++i) {
    			Value _value(nullptr, &trueValue[i]);
    			_value.get_to(tmpValue);
    			value.push(value);
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, char*> ||
    		std::is_same_v<key, const char*>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _value(nullptr, &itr->value);
    				value[itr->name.GetString()] = _value.get<val>();
    			}
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, std::string>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[itr->name.GetString()] = _val.get<val>();
    			}
    		}

    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, int8_t> ||
    		std::is_same_v<key, uint8_t> ||
    		std::is_same_v<key, int16_t> ||
    		std::is_same_v<key, uint16_t> ||
    		std::is_same_v<key, int32_t> ||
    		std::is_same_v<key, uint32_t> ||
    		std::is_same_v<key, int64_t> ||
    		std::is_same_v<key, uint64_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[(key)std::stoll(itr->name.GetString())] = _val.get<val>();
    			}
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_enum_v<key>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[(key)std::stoll(itr->name.GetString())] = _val.get<val>();
    			}
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, char*> ||
    		std::is_same_v<key, const char*>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::unordered_map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[itr->name.GetString()] = _val.get<val>();
    			}
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, std::string>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::unordered_map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[itr->name.GetString()] = _val.get<val>();
    			}
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_same_v<key, int8_t> ||
    		std::is_same_v<key, uint8_t> ||
    		std::is_same_v<key, int16_t> ||
    		std::is_same_v<key, uint16_t> ||
    		std::is_same_v<key, int32_t> ||
    		std::is_same_v<key, uint32_t> ||
    		std::is_same_v<key, int64_t> ||
    		std::is_same_v<key, uint64_t>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::unordered_map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[(key)std::stoll(itr->name.GetString())] = _val.get<val>();
    			}
    		}
    	}

    	template <typename key, typename val, std::enable_if_t<
    		std::is_enum_v<key>, int32_t> = 0>
    	[[maybe_unused]]
    	constexpr auto get_value(std::unordered_map<key, val>& value) -> void {
    		if (_innerValue->IsNull()) {
    			value = std::map<key, val> {};
    			return;
    		}

    		if (_innerValue->IsObject()) {
    			for (auto itr = _innerValue->GetObject().MemberBegin();
    				itr != _innerValue->GetObject().MemberEnd(); ++itr) {
    				Value _val(nullptr, &itr->value);
    				value[(key)std::stoll(itr->name.GetString())] = _val.get<val>();
    			}
    		}
    	}

    	template<typename type,
    		std::enable_if_t<( !std::is_same_v<type, int8_t> &&
    		!std::is_same_v<type, int16_t> &&
    		!std::is_same_v<type, int32_t> &&
    		!std::is_same_v<type, int64_t> &&
    		!std::is_same_v<type, uint8_t> &&
    		!std::is_same_v<type, uint16_t> &&
    		!std::is_same_v<type, uint32_t> &&
    		!std::is_same_v<type, uint64_t> &&
    		!std::is_same_v<type, bool> &&
    		!std::is_same_v<type, float_t> &&
    		!std::is_same_v<type, double_t> &&
    		!std::is_same_v<type, char*> &&
    		!std::is_same_v<type, const char*> &&
    		!std::is_same_v<type, std::string> &&
    		!std::is_enum_v<type> ), int> = 0>
    	[[maybe_unused]]
    	auto get_value(type& value) -> void {
    		if (!_innerValue->IsObject()) {
    			value = type {};
    		}
    		members_reflection<type>::from_json(Value(nullptr, _innerValue), value);
    	}

    private:
    	const char* _innerKey;
    	rapidjson::GenericValue<rapidjson::UTF8<>>* _innerValue;
    	bool _innerExist;
    };

    class Deserialize {
    	friend class Converter;
    public:
    	Deserialize(const char* json) {
    		_innerDom->Parse(json);
    	}
    	~Deserialize() {
    		delete _innerDom;
    	}

    	template <typename type>
    	constexpr auto deserialize(type& value) -> void {
    		Value val(nullptr, _innerDom);
    		val.get_to(value);
    	}
    private:
    	rapidjson::GenericDocument<rapidjson::UTF8<>>* _innerDom = new rapidjson::GenericDocument<rapidjson::UTF8<>>();
    };

    class [[maybe_unused]] Converter {
    public:
    	template <typename object>
    	static auto serialize(object&& obj) -> std::string {
    		Serialize _serialize;
    		return _serialize.serialize(obj);
    	}

    	template <typename object>
    	static auto deserialize(const std::string& json) -> object {
    		object obj {};
    		Deserialize _deserialize(json.c_str());
    		_deserialize.deserialize(obj);
    		return obj;
    	}
    };

}

#endif // ! ____CRAZY_RAPIDJSON_REFLECTION_H____