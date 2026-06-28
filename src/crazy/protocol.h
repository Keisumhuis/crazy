/**
 * @file protocol.h
 * @author kesium (keisumhuis@gmail.com)
 * @brief 
 * @version 0.1
 * @date 24
 *
 * @copyright Copyright (c) 2026
 */
#pragma once

#include <string.h>
#include <type_traits>
#include <string>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crazy/buffer.h"
#include "crazy/reflection.h"

namespace crazy::protocol {
	enum class ProtocolValueType : int8_t {
		BOOLEAN,
		INT8,
		UINT8,
		INT16,
		UINT16,
		INT32,
		UINT32,
		INT64,
		UINT64,
		FLOAT,
		DOUBLE,
		STRING,
		ARRAY,
		DEQUE,
		LIST,
		MAP,
		SET,
		STACK,
		UNORDERED_MAP,
		UNORDERED_SET,
		VECTOR,
		CUSTOM
	};
	class Converter;
	class Serialize {
		friend class Converter;
	public:
		template <typename type>
		void add_from(const type& value) {
			writeValue(value);
		}
		template <typename type>
		Serialize& operator<<(const type& value) {
			writeValue(value);
			return *this;
		}
		Buffer getBuffer() {
			return buffer_;
		}

		template <typename T, size_t N>
		[[maybe_unused]] auto writeValue(T(&value)[N]) -> void {
			writeType(ProtocolValueType::ARRAY);
			writeValue((uint64_t)N);
			for (size_t i = 0; i < N; ++i) {
				writeValue(value[i]);
			}
		}

		template <typename T, size_t N>
		[[maybe_unused]] auto writeValue(const std::array<T, N>& value) -> void {
			writeType(ProtocolValueType::ARRAY);
			writeValue((uint64_t)N);
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, bool>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::BOOLEAN);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int8_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::INT8);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, char>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::INT8);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint8_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::UINT8);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int16_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::INT16);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint16_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::UINT16);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int32_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::INT32);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint32_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::UINT32);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int64_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::INT64);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint64_t>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::UINT64);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_enum_v<T>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::UINT64);
			uint64_t tmp = (uint64_t)value;
			write((char*)(&tmp), sizeof(tmp));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, float>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::FLOAT);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, double>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::DOUBLE);
			write((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, std::string>, int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::STRING);
			writeValue((uint64_t)value.size());
			write((char*)value.data(), value.size());
		}

		template <typename T>
		[[maybe_unused]] auto writeValue(const std::deque<T>& value) -> void {
			writeType(ProtocolValueType::DEQUE);
			writeValue((uint64_t)value.size());
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename T>
		[[maybe_unused]] auto writeValue(const std::list<T>& value) -> void {
			writeType(ProtocolValueType::LIST);
			writeValue((uint64_t)value.size());
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename key, typename val>
		[[maybe_unused]] auto writeValue(const std::map<key, val>& value) -> void {
			writeType(ProtocolValueType::MAP);
			writeValue((uint64_t)value.size());
			for (const auto& [_key, _value] : value) {
				writeValue(_key);
				writeValue(_value);
			}
		}

		template <typename T>
		[[maybe_unused]] auto writeValue(const std::set<T>& value) -> void {
			writeType(ProtocolValueType::SET);
			writeValue((uint64_t)value.size());
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename T>
		[[maybe_unused]] auto writeValue(const std::stack<T>& value) -> void {
			writeType(ProtocolValueType::STACK);
			writeValue((uint64_t)value.size());
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename key, typename val>
		[[maybe_unused]] auto writeValue(const std::unordered_map<key, val>& value) -> void {
			writeType(ProtocolValueType::UNORDERED_MAP);
			writeValue((uint64_t)value.size());
			for (const auto& [_key, _value] : value) {
				writeValue(_key);
				writeValue(_value);
			}
		}

		template <typename T>
		[[maybe_unused]] auto writeValue(const std::unordered_set<T>& value) -> void {
			writeType(ProtocolValueType::UNORDERED_SET);
			writeValue((uint64_t)value.size());
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename T>
		[[maybe_unused]] auto writeValue(const std::vector<T>& value) -> void {
			writeType(ProtocolValueType::VECTOR);
			writeValue((uint64_t)value.size());
			for (const auto& it : value) {
				writeValue(it);
			}
		}

		template <typename T, std::enable_if_t<
			!std::is_same_v<T, bool> &&
			!std::is_same_v<T, int8_t> &&
			!std::is_same_v<T, uint8_t> &&
			!std::is_same_v<T, int16_t> &&
			!std::is_same_v<T, uint16_t> &&
			!std::is_same_v<T, int32_t> &&
			!std::is_same_v<T, uint32_t> &&
			!std::is_same_v<T, int64_t> &&
			!std::is_same_v<T, uint64_t> &&
			!std::is_same_v<T, float> &&
			!std::is_same_v<T, double> &&
			!std::is_same_v<T, std::string> &&
			!std::is_enum_v<T> &&
			!std::is_fundamental_v<T> &&
			!std::is_array_v<T>,
			int32_t> = 0>
		[[maybe_unused]] auto writeValue(const T& value) -> void {
			writeType(ProtocolValueType::CUSTOM);
			value.to_protocol(*this);
		}

	private:
		void writeType(ProtocolValueType type) {
			buffer_.append((char*)(&type), sizeof(type));
		}
		void write(char* data, uint32_t count) {
			buffer_.append(data, count);
		}
		Buffer buffer_;
	};

	class Deserialize {
		friend class Converter;
	public:
		template <typename type>
		auto get_from(type& value) -> void {
			readValue(value);
		}
		template <typename type>
		Deserialize& operator>>(const type& value) {
			return *this;
		}
		void setBuffer(const Buffer& buffer) {
			buffer_ = buffer;
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, bool>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::BOOLEAN != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int8_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::INT8 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, char>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::INT8 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint8_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::UINT8 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int16_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::INT16 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint16_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::UINT16 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int32_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::INT32 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint32_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::UINT32 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, int64_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::INT64 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, uint64_t>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::UINT64 != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(uint64_t));
		}

		template <typename T, std::enable_if_t<std::is_enum_v<T>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::UINT64 != readType()) {
				throw std::logic_error("");
			}
			uint64_t tmp;
			read((char*)(&tmp), sizeof(uint64_t));
			value = (T)tmp;
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, float>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::FLOAT != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, double>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::DOUBLE != readType()) {
				throw std::logic_error("");
			}
			read((char*)(&value), sizeof(value));
		}

		template <typename T, std::enable_if_t<std::is_same_v<T, std::string>, int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::STRING != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			value.resize(count);
			read(value.data(), count);
		}

		template <typename T, size_t N>
		[[maybe_unused]] auto readValue(T(&value)[N]) -> void {
			if (ProtocolValueType::ARRAY != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			if (N != count) {
				throw std::logic_error("");
			}
			for (uint64_t i = 0; i < count; ++i) {
				readValue(value[i]);
			}
		}

		template <typename T, size_t N>
		[[maybe_unused]] auto readValue(std::array<T, N>& value) -> void {
			if (ProtocolValueType::ARRAY != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			if (N != count) {
				throw std::logic_error("");
			}
			for (uint64_t i = 0; i < count; ++i) {
				readValue(value[i]);
			}
		}

		template <typename T>
		[[maybe_unused]] auto readValue(std::deque<T>& value) -> void {
			if (ProtocolValueType::DEQUE != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				T _value;
				readValue(_value);
				value.push_back(_value);
			}
		}

		template <typename T>
		[[maybe_unused]] auto readValue(std::list<T>& value) -> void {
			if (ProtocolValueType::LIST != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				T _value;
				readValue(_value);
				value.push_back(_value);
			}
		}

		template <typename key, typename val>
		[[maybe_unused]] auto readValue(std::map<key, val>& value) -> void {
			if (ProtocolValueType::MAP != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				key _key;
				val _val;
				readValue(_key);
				readValue(_val);
				value[_key] = _val;
			}
		}

		template <typename T>
		[[maybe_unused]] auto readValue(std::set<T>& value) -> void {
			if (ProtocolValueType::SET != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				T _value;
				readValue(_value);
				value.insert(_value);
			}
		}

		template <typename T>
		[[maybe_unused]] auto readValue(std::stack<T>& value) -> void {
			if (ProtocolValueType::STACK != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				T _value;
				readValue(_value);
				value.push(_value);
			}
		}

		template <typename key, typename val>
		[[maybe_unused]] auto readValue(std::unordered_map<key, val>& value) -> void {
			if (ProtocolValueType::UNORDERED_MAP != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				key _key;
				val _val;
				readValue(_key);
				readValue(_val);
				value[_key] = _val;
			}
		}

		template <typename T>
		[[maybe_unused]] auto readValue(std::unordered_set<T>& value) -> void {
			if (ProtocolValueType::UNORDERED_SET != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				T _value;
				readValue(_value);
				value.insert(_value);
			}
		}

		template <typename T>
		[[maybe_unused]] auto readValue(std::vector<T>& value) -> void {
			if (ProtocolValueType::VECTOR != readType()) {
				throw std::logic_error("");
			}
			uint64_t count = 0;
			readValue(count);
			for (uint64_t i = 0; i < count; ++i) {
				T _value;
				readValue(_value);
				value.push_back(_value);
			}
		}

		template <typename T, std::enable_if_t<
			!std::is_same_v<T, bool> &&
			!std::is_same_v<T, int8_t> &&
			!std::is_same_v<T, uint8_t> &&
			!std::is_same_v<T, int16_t> &&
			!std::is_same_v<T, uint16_t> &&
			!std::is_same_v<T, int32_t> &&
			!std::is_same_v<T, uint32_t> &&
			!std::is_same_v<T, int64_t> &&
			!std::is_same_v<T, uint64_t> &&
			!std::is_same_v<T, float> &&
			!std::is_same_v<T, double> &&
			!std::is_same_v<T, std::string> &&
			!std::is_enum_v<T> &&
			!std::is_fundamental_v<T> &&
			!std::is_array_v<T>,
			int32_t> = 0>
		[[maybe_unused]] auto readValue(T& value) -> void {
			if (ProtocolValueType::CUSTOM != readType()) {
				throw std::logic_error("");
			}
			value.from_protocol(*this);
		}

	private:
		ProtocolValueType readType() {
			ProtocolValueType type;
			read((char*)&type, sizeof(type));
			return type;
		}
		void read(char* data, uint64_t count) {
			memcpy(data, buffer_.readBegin(), count);
			buffer_.readed(count);
		}
		Buffer buffer_;
	};

	class Converter {
	public:
		template <typename type>
		static std::string Serializable(const type& value) {
			Serialize serializable;
			serializable.writeValue(value);
			auto buffer = serializable.getBuffer();
			return std::string(buffer.readBegin(), buffer.readableCount());
		}
		template <typename type>
		static type Deserializable(const std::string& value) {
			type object;
			Buffer buffer;
			buffer.append(value.c_str(), (uint32_t)value.size());
			Deserialize deserializable;
			deserializable.setBuffer(buffer);
			deserializable.readValue(object);
			return object;
		}
	};
}
