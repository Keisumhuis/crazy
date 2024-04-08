/**
 * @file endian.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_ENDIAN_H____
#define ____CRAZY_ENDIAN_H____

#include <endian.h>
#include <byteswap.h>

#include <algorithm>
#include <type_traits>

#define CRAZY_LITTLE_ENDIAN 	1
#define CRAZY_BIG_ENDIAN	2

namespace crazy {

#ifdef __BIG_ENDIAN__
#define CRAZY_BYTE_ORDER CRAZY_BIG_ENDIAN
#else
#define CRAZY_BYTE_ORDER CRAZY_LITTLE_ENDIAN
#endif

template <typename type, std::enable_if_t<sizeof(type) == sizeof(int16_t), int32_t> = 0>
type Byteswap(type& val) {
	return static_cast<type>(bswap_16(val));
}

template <typename type, std::enable_if_t<sizeof(type) == sizeof(int32_t), int32_t> = 0>
type Byteswap(type& val) {
	return static_cast<type>(bswap_32(val));
}

template <typename type, std::enable_if_t<sizeof(type) == sizeof(int64_t), int32_t> = 0>
type Byteswap(type& val) {
	return static_cast<type>(bswap_64(val));
}

#if CRAZY_BYTE_ORDER == CRAZY_BIG_ENDIAN
template <typename type>
type ByteSwapOnLittleEndian(type val) {
	return val;
}
template <typename type>
type ByteSwapOnBigEndian(type val) {
	return Byteswap(val);
}
#else
template <typename type>
type ByteSwapOnLittleEndian(type val) {
	return Byteswap(val);
}
template <typename type>
type ByteSwapOnBigEndian(type val) {
	return val;
}
#endif

}

#endif // ! ____CRAZY_ENDIAN_H____

