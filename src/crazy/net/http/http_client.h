/**
 * @file http_client.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 同步客户端.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "crazy/net/http/http_header.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"

namespace crazy {
	/**
	 * @brief form-data 文本字段.
	 */
	struct FormDataField {
		//! 字段名
		std::string name;
		//! 字段值
		std::string value;
	};

	/**
	 * @brief form-data 文件字段.
	 */
	struct FormDataFile {
		//! 字段名
		std::string name;
		//! 文件名
		std::string filename;
		//! 内容类型
		std::string contentType;
		//! 文件内容
		std::string content;
	};

	/**
	 * @brief HTTP 同步客户端.
	 */
	class HttpClient {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpClient>;
		//! 响应回调类型
		using ResponseCallback = std::function<void(HttpResponse::ptr)>;
		/**
		 * @brief 设置默认请求头（对所有请求生效）.
		 */
		void setHeader(const std::string& key, const std::string& value);
		/**
		 * @brief 获取默认请求头容器（可链式设置）.
		 */
		HttpHeader& headers();
		/**
		 * @brief 发起 GET 请求.
		 */
		void get(const std::string& uri, ResponseCallback callback);
		/**
		 * @brief 发起 POST 请求.
		 */
		void post(const std::string& uri, const std::string& body, ResponseCallback callback);
		/**
		 * @brief 发起 PUT 请求.
		 */
		void put(const std::string& uri, const std::string& body, ResponseCallback callback);
		/**
		 * @brief 发起 DELETE 请求.
		 */
		void del(const std::string& uri, ResponseCallback callback);
		/**
		 * @brief 发起 HEAD 请求.
		 */
		void head(const std::string& uri, ResponseCallback callback);
		/**
		 * @brief 发起 OPTIONS 请求.
		 */
		void options(const std::string& uri, ResponseCallback callback);
		/**
		 * @brief 发起 PATCH 请求.
		 */
		void patch(const std::string& uri, const std::string& body, ResponseCallback callback);
		/**
		 * @brief 发起 multipart/form-data POST 请求.
		 */
		void postForm(const std::string& uri, const std::vector<FormDataField>& fields, const std::vector<FormDataFile>& files, ResponseCallback callback);
		/**
		 * @brief 发起通用 HTTP 请求（同步阻塞）.
		 */
		HttpResponse::ptr request(HttpMethod method, const std::string& uri, const std::string& body, ResponseCallback callback);
		/**
		 * @brief 生成随机的 multipart boundary.
		 */
		static std::string generateBoundary();
		/**
		 * @brief 构造 multipart/form-data 请求体.
		 */
		static std::string buildMultipartBody(const std::vector<FormDataField>& fields, const std::vector<FormDataFile>& files, const std::string& boundary);

	private:
		//! 默认请求头
		HttpHeader headers_;
	};
}  // namespace crazy
