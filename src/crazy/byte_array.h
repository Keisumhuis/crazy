/**
 * @file byte_array.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief/
 */
#ifndef ____CRAZY_BYTE_ARRAY_H____
#define ____CRAZY_BYTE_ARRAY_H____

#include <algorithm>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <stdint.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "logger.h"
#include "reflection.h"

namespace crazy::byte_array {

	enum class ByteType : uint8_t {
		BOOL, INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64
		, FLOAT, DOUBLE, STRING, VECTOCR, SET, MAP, UNORDERED_SET, UNORDERED_MAP
		, CUSTOM
	};

	class Converter;
	class Serialize final {
		friend class Converter;
	public:
		template <typename type>
		void add_from(const type& value) {
			write_value(value);
		}
		template <typename type>
		std::string serialize(type& value) {
			write_value(value);
			return _buffer_string;
		}
		template <typename type>
		Serialize& operator<< (const type& value) {
			write_value(value);
			return *this;
		} 
		std::string serialize() {
			return _buffer_string;
		}
	protected:
		template <typename type
			, std::enable_if_t<std::is_same_v<type, bool>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::BOOL);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int8_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::INT8);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int16_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::INT16);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int32_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::INT32);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int64_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::INT64);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint8_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::UINT8);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint16_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::UINT16);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint32_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::UINT32);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint64_t>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::UINT64);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, float>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::FLOAT);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, double>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::DOUBLE);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		template <typename type
			, std::enable_if_t<std::is_enum_v<type>, int32_t> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::INT64);
			write(reinterpret_cast<const char*>(&value), sizeof(type));
		}
		[[maybe_unused]]
		void write_value(const std::string& value) {
			write_type(ByteType::STRING);
			write_length(value.size());
			write(value.data(), value.size());
		}
		template <typename type>
		[[maybe_unused]]
		void write_value(const std::vector<type>& value) {
			write_type(ByteType::VECTOCR);
			write_length(value.size());
			for (auto& it : value) {
				write_value(it);
			}
		}
		template <typename type>
		[[maybe_unused]]
		void write_value(const std::set<type>& value) {
			write_type(ByteType::SET);
			write_length(value.size());
			for (auto& it : value) {
				write_value(it);
			}
		}
		template <typename key, typename val>
		[[maybe_unused]]
		void write_value(const std::map<key, val>& value) {
			write_type(ByteType::MAP);
			write_length(value.size());
			for (auto& [_key, _val] : value) {
				write_value(_key);
				write_value(_val);
			}	
		}
		template <typename type>
		[[maybe_unused]]
		void write_value(const std::unordered_set<type>& value) {
			write_type(ByteType::UNORDERED_SET);
			write_length(value.size());
			for (auto& it : value) {
				write_value(it);
			}	
		}
		template <typename key, typename val>
		[[maybe_unused]]
		void write_value(const std::unordered_map<key, val>& value) {
			write_type(ByteType::UNORDERED_MAP);
			write_length(value.size());
			for (auto& [_key, _val] : value) {
				write_value(_key);
				write_value(_val);
			}	
		}
		template <typename type, std::enable_if_t<
			!std::is_same_v<type, bool> &&
			!std::is_same_v<type, int8_t> &&
			!std::is_same_v<type, int16_t> &&
			!std::is_same_v<type, int32_t> &&
			!std::is_same_v<type, int64_t> &&
			!std::is_same_v<type, uint8_t> &&
			!std::is_same_v<type, uint16_t> &&
			!std::is_same_v<type, uint32_t> &&
			!std::is_same_v<type, uint64_t> &&
			!std::is_same_v<type, float> &&
			!std::is_same_v<type, double> &&
			!std::is_enum_v<type>, int32_t
			> = 0>
		[[maybe_unused]]
		void write_value(const type& value) {
			write_type(ByteType::CUSTOM);
			members_reflection<type>::to_byte_array(*this, value);
		}
	protected:
		void write_type(const ByteType& type) {
			_buffer_string.append(reinterpret_cast<const char *>(&type), sizeof(ByteType));
		}
		void write_length(const uint64_t& length) {
			_buffer_string.append(reinterpret_cast<const char *>(&length), sizeof(uint64_t));
		}
		void write(const char* data, const size_t& size = sizeof(uint8_t)) {
			_buffer_string.append(data, size);	
		}
	private:
		std::string _buffer_string;
	};

	class Deserialize final {
		friend class Converter;
	public:
		Deserialize() 
			: _pos (0) {}
		Deserialize(const std::string& data) 
			: _buffer_string (data) 
			, _pos (0) {}
		Deserialize(const char* data, size_t len) 
			: _buffer_string (std::string{data, len}) 
			, _pos (0) {}
		template <typename type>
		void get_from(type& value) {
			get_value(value);
		}
		template <typename type>
		type deserialize(const std::string& data) {
			_buffer_string = data;
			_pos = 0;
			type value;
			get_value(value);
			return value;
		}
		template <typename type>
		Deserialize& operator >> (type& value) {
			get_from(value);
			return *this;
		}
		template<typename Tuple, size_t Id>
		void getv(Deserialize& ds, Tuple& t) {
			ds >> std::get<Id>(t);
		}
		template<typename Tuple, size_t... I>
		Tuple get_tuple(std::index_sequence<I...>) {
			Tuple t;
			std::initializer_list<int>{((getv<Tuple, I>(*this, t)), 0)...};
			return t;
		}
	protected:
		template <typename type
			, std::enable_if_t<std::is_same_v<type, bool>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::BOOL == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int8_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::INT8 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int16_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::INT16 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int32_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::INT32 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, int64_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::INT64 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint8_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::UINT8 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint16_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::UINT16 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint32_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::UINT32 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, uint64_t>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::UINT64 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, float>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::FLOAT == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_same_v<type, double>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::DOUBLE == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type
			, std::enable_if_t<std::is_enum_v<type>, int32_t> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::UINT64 == get_type()) {
				get(reinterpret_cast<char*>(&value), sizeof(type));
				return;
			}
			throw std::logic_error("byte array type error");
		}
		[[maybe_unused]]
		void get_value(std::string& value) {
			if (ByteType::STRING == get_type()) {
				size_t size = get_length();
				value.resize(size);
				get(value.data(), size);
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type>
		[[maybe_unused]]
		void get_value(std::vector<type>& value) {
			if (ByteType::VECTOCR == get_type()) {
				size_t size = get_length();
				for (size_t i = 0; i < size; ++i) {
					type _tmp;
					get_value(_tmp);
					value.push_back(_tmp);
				}
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type>
		[[maybe_unused]]
		void get_value(std::set<type>& value) {
			if (ByteType::SET == get_type()) {
				size_t size = get_length();
				for (size_t i = 0; i < size; ++i) {
					type _tmp;
					get_value(_tmp);
					value.insert(_tmp);
				}
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename key, typename val>
		[[maybe_unused]]
		void get_value(std::map<key, val>& value) {
			if (ByteType::MAP == get_type()) {
				size_t size = get_length();
				for (size_t i = 0; i < size; ++i) {
					key _key;
					val _val;
					get_value(_key);
					get_value(_val);
					value[_key] = _val;
				}
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type>
		[[maybe_unused]]
		void get_value(std::unordered_set<type>& value) {
			if (ByteType::UNORDERED_SET == get_type()) {
				size_t size = get_length();
				for (size_t i = 0; i < size; ++i) {
					type _tmp;
					get_value(_tmp);
					value.insert(_tmp);
				}
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename key, typename val>
		[[maybe_unused]]
		void get_value(std::unordered_map<key, val>& value) {
			if (ByteType::UNORDERED_MAP == get_type()) {
				size_t size = get_length();
				for (size_t i = 0; i < size; ++i) {
					key _key;
					val _val;
					get_value(_key);
					get_value(_val);
					value[_key] = _val;
				}
				return;
			}
			throw std::logic_error("byte array type error");
		}
		template <typename type, std::enable_if_t<
			!std::is_same_v<type, bool> &&
			!std::is_same_v<type, int8_t> &&
			!std::is_same_v<type, int16_t> &&
			!std::is_same_v<type, int32_t> &&
			!std::is_same_v<type, int64_t> &&
			!std::is_same_v<type, uint8_t> &&
			!std::is_same_v<type, uint16_t> &&
			!std::is_same_v<type, uint32_t> &&
			!std::is_same_v<type, uint64_t> &&
			!std::is_same_v<type, float> &&
			!std::is_same_v<type, double> &&
			!std::is_enum_v<type>, int32_t
			> = 0>
		[[maybe_unused]]
		void get_value(type& value) {
			if (ByteType::CUSTOM == get_type()) {
				members_reflection<type>::from_byte_array(*this, value);	
				return;
			}
			throw std::logic_error("byte array type error");
		}
	protected:
		ByteType get_type() {
		 	ByteType type = static_cast<ByteType>(_buffer_string[_pos++]);
			return type;
		}
		uint64_t get_length() {	
			uint64_t size = static_cast<uint64_t>(_buffer_string[_pos]);
			_pos += sizeof(uint64_t);
			return size;
		}
		void get(char* data, const size_t& length) {
			std::memcpy(data, &_buffer_string[_pos], length);
			_pos += length;
		}
	private:
		size_t _pos;
		std::string _buffer_string;
	};
	
	class Converter final {
	public:
		template <typename type>
		static std::string Serialize(const type& value) {
			try {
				class Serialize serialize;
				return serialize.serialize(value);
			} catch (std::exception& e) {
				CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : " << e.what();
				return "";	
			}
		}
		template <typename type>
		static type Deserialize(const std::string& data) {
			try {
				class Deserialize deserialize;
				return deserialize.deserialize<type>(data);
			} catch (std::exception& e) {
				CRAZY_ERROR(CRAZY_ROOT_LOGGER()) << "exception : " << e.what();	
				return type{};
			}
		}
	};
}


#endif // !  ____CRAZY_BYTE_ARRAY_H____
