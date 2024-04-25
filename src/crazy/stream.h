/**
 * @file stream.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_STREAM_H____
#define ____CRAZY_STREAM_H____

#include <memory>

namespace crazy {

	class Stream {
	public:
		using Ptr = std::shared_ptr<Stream>;
		virtual ~Stream() {}
		virtual int32_t Read(void* buffer, size_t length) = 0;
		virtual int32_t Write(const void* buffer, size_t length) = 0;
		virtual int32_t ReadFixSize(void* buffer, size_t length);
		virtual int32_t WriteFixSize(const void* buffer, size_t length);
		virtual void Close() = 0;
	};

}

#endif // ! ____CRAZY_STREAM_H____

