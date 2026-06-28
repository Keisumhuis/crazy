/**
 * @file endian.h
 * @author keisum (Keisumhuis@gmail.com)
 * @brief 大小端转换类
 * @version 0.1
 * @date 3
 *
 * @copyright Copyright (c) 2024
 */
#pragma once
#ifndef _CRAZY_ENDIAN_H_
#define _CRAZY_ENDIAN_H_

#include <stdint.h>
#include <stdlib.h>

#if __linux__
#include <byteswap.h>
#include <endian.h>
#elif __APPLE__
#include <__bit/byteswap.h>
#include <__bit/endian.h>
#endif

#include <type_traits>

#define CRAZY_LITTLE_ENDIAN 1
#define CRAZY_BIG_ENDIAN 2

extern "C++" {
#if _WIN32
#define CRAZY_BYTE_ORDER CRAZY_LITTLE_ENDIAN
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)_byteswap_ushort((uint16_t)value);
	}
	template <typename type, std::enable_if_t<sizeof(uint32_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)_byteswap_ulong((uint32_t)value);
	}
	template <typename type, std::enable_if_t<sizeof(uint64_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)_byteswap_uint64((uint64_t)value);
	}
#elif __linux__
#if _BYTE_ORDER == _BIG_ENDIAN
#define CRAZY_BYTE_ORDER CRAZY_BIG_ENDIAN
#elif
#define CRAZY_BYTE_ORDER CRAZY_LITTLE_ENDIAN
#endif
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)bswap_16((uint16_t)value);
	}
	template <typename type, std::enable_if_t<sizeof(uint32_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)bswap_32((uint32_t)value);
	}
	template <typename type, std::enable_if_t<sizeof(uint64_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)bswap_64((uint64_t)value);
	}
#elif __APPLE__
#if _BYTE_ORDER == _BIG_ENDIAN
#define CRAZY_BYTE_ORDER CRAZY_BIG_ENDIAN
#elif
#define CRAZY_BYTE_ORDER CRAZY_LITTLE_ENDIAN
#endif
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)__builtin_bswap16((uint16_t)value);
	}
	template <typename type, std::enable_if_t<sizeof(uint32_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)__builtin_bswap32((uint32_t)value);
	}
	template <typename type, std::enable_if_t<sizeof(uint64_t) == sizeof(type), type> = 0>
	type byteswap(type value) {
		return (type)__builtin_bswap64((uint64_t)value);
	}
#endif

#if CRAZY_BYTE_ORDER == CRAZY_LITTLE_ENDIAN
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToBigOrder(type value) {
		return byteswap(value);
	}
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToLittleOrder(type value) {
		return value;
	}
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToNetOrder(type value) {
		return byteswap(value);
	}
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToLocalOrder(type value) {
		return byteswap(value);
	}
#else
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToBigOrder(type value) {
		return value;
	}
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToLittleOrder(type value) {
		return byteswap(value);
	}
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToNetOrder(type value) {
		return byteswap(value);
	}
	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type SwapToLocalOrder(type value) {
		return byteswap(value);
	}
#endif

	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type hton(type value) {
		return byteswap(value);
	}

	template <typename type, std::enable_if_t<sizeof(uint16_t) == sizeof(type) || sizeof(uint32_t) == sizeof(type) ||
		sizeof(uint64_t) == sizeof(type),
		type> = 0>
	type ntoh(type value) {
		return byteswap(value);
	}
}
#endif  // ! _CRAZY_ENDIAN_H_
