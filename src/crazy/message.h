/**
 * @file message.h
 * @version 0.0.1
 * @date 2023-12-07
 * @author Keisum (keisumhuis@gmail.com)
 * @copyright Copyright (c) 2023  Keisum_huis
 * @brief
 */
#ifndef ____CRAZY_MESSAGE_H____
#define ____CRAZY_MESSAGE_H____

#include <algorithm>
#include <memory>

#include "byte_array.h"

namespace crazy {
	
	enum class Type : int8_t {
		Request,
		Response,
		Notify
	};
	
	class Message {
	public:
		virtual	~Message() {}
		uint64_t GetLen() const { return m_len; }
		std::string GetSn() const { return m_sn; }
		uint32_t GetCmd() const { return m_cmd; }
		void SetLen(const uint64_t len) { m_len = len; }
		void SetSn(const std::string& sn) { m_sn = sn; }
		void SetCmd(const uint32_t cmd) { m_cmd = cmd; }
	private:
		uint64_t m_len;
		uint32_t m_cmd;
		std::string m_sn;
	};
	
	class Request : public Message {
	public:
		using Ptr = std::shared_ptr<Request>;
	private:
		std::string m_reqstr;
	};

	class Response : public Message {
	public:
		using Ptr = std::shared_ptr<Response>;
	private:
		uint32_t m_result;
		std::string m_resultStr;
	};
	
	class Notify : public Message {
	public:
		using Ptr = std::shared_ptr<Notify>;
	private:
		std::string m_notifyStr;
	};
}

#endif // ! ____CRAZY_MESSAGE_H____
