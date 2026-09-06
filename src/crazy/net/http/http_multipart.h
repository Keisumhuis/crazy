/**
 * @file http_multipart.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief multipart/form-data 解析器.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "crazy/net/http/http_header.h"
#include "crazy/net/http/multipart_parser.h"

namespace crazy {
	/**
	 * @brief multipart part.
	 */
	struct HttpMultipartPart {
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpMultipartPart>;
		//! part 头
		HttpHeader headers;
		//! part 数据
		std::string body;
		/**
		 * @brief 获取 form 字段名.
		 */
		std::string name() const;
		/**
		 * @brief 获取上传文件名.
		 */
		std::string filename() const;
		/**
		 * @brief 获取 part 的 Content-Type.
		 */
		std::string contentType() const;
	};
	/**
	 * @brief multipart/form-data 解析器.
	 */
	class HttpMultipartParser {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpMultipartParser>;
		/**
		 * @brief 从 Content-Type 中提取 boundary.
		 */
		static std::string getBoundary(const std::string& contentType);
		/**
		 * @brief 使用 Content-Type 构造解析器.
		 */
		explicit HttpMultipartParser(const std::string& contentType);
		/**
		 * @brief 析构函数.
		 */
		~HttpMultipartParser();
		/**
		 * @brief 禁止拷贝构造.
		 */
		HttpMultipartParser(const HttpMultipartParser&) = delete;
		/**
		 * @brief 禁止拷贝赋值.
		 */
		HttpMultipartParser& operator=(const HttpMultipartParser&) = delete;
		/**
		 * @brief 增量解析 multipart 数据.
		 */
		size_t execute(const char* data, size_t length);
		/**
		 * @brief 增量解析 multipart 数据.
		 */
		size_t execute(const std::string& data);
		/**
		 * @brief 一次性解析完整 multipart 数据.
		 */
		static HttpMultipartParser::ptr parse(const std::string& contentType, const std::string& data);
		/**
		 * @brief 是否解析完成.
		 */
		bool isFinished() const;
		/**
		 * @brief 是否解析出错.
		 */
		bool hasError() const;
		/**
		 * @brief 获取解析得到的 part 列表.
		 */
		const std::vector<HttpMultipartPart>& parts() const;

	private:
		/**
		 * @brief multipart part 开始回调.
		 */
		static int32_t onPartDataBegin(multipart_parser* parser);
		/**
		 * @brief multipart 头字段回调.
		 */
		static int32_t onHeaderField(multipart_parser* parser, const char* data, size_t length);
		/**
		 * @brief multipart 头值回调.
		 */
		static int32_t onHeaderValue(multipart_parser* parser, const char* data, size_t length);
		/**
		 * @brief multipart part 头解析完成回调.
		 */
		static int32_t onHeadersComplete(multipart_parser* parser);
		/**
		 * @brief multipart part 数据回调.
		 */
		static int32_t onPartData(multipart_parser* parser, const char* data, size_t length);
		/**
		 * @brief multipart part 结束回调.
		 */
		static int32_t onPartDataEnd(multipart_parser* parser);
		/**
		 * @brief multipart body 结束回调.
		 */
		static int32_t onBodyEnd(multipart_parser* parser);
		/**
		 * @brief 提交当前头字段.
		 */
		void commitHeader();

	private:
		//! multipart_parser 解析器
		multipart_parser* parser_ = nullptr;
		//! multipart_parser 回调设置
		multipart_parser_settings settings_{};
		//! 解析得到的 part 列表
		std::vector<HttpMultipartPart> parts_;
		//! 当前头字段
		std::string headerField_;
		//! 当前头值
		std::string headerValue_;
		//! 当前是否已进入头值状态
		bool headerValueSeen_ = false;
		//! 是否解析完成
		bool finished_ = false;
		//! 是否解析出错
		bool error_ = false;
	};
}
