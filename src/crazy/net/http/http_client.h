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
#include "crazy/net/selector.h"
#include "crazy/net/socket.h"

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
		//! 响应体数据回调类型（数据到达即回调，不等待响应结束）
		using DataCallback = std::function<void(const std::string& data)>;
		//! 事件注册回调类型（`ActorInterface::registerEventOnThread` / `Selector::registerEvent`）
		using RegisterEventCallback = std::function<void(int32_t, SelectorEventType, std::function<void()>)>;
		//! 事件注销回调类型
		using UnregisterEventCallback = std::function<void(int32_t, SelectorEventType)>;
		/**
		 * @brief 设置默认请求头（对所有请求生效）.
		 */
		void setHeader(const std::string& key, const std::string& value);
		/**
		 * @brief 获取默认请求头容器（可链式设置）.
		 */
		HttpHeader& headers();
		/**
		 * @brief 设置请求体是否以分块方式（Transfer-Encoding: chunked）发送.
		 * @param enabled 为 true 时不写 `Content-Length`，请求体按 `chunkSize` 切块并以 `0\r\n\r\n` 收尾.
		 * @param chunkSize 单个数据块的最大字节数（0 表示使用默认值 8192）.
		 * @note 默认关闭。开启后默认请求头里不得再有 `Content-Length`：此时 `request()` 返回
		 * nullptr、`requestAsync()` 返回 false，且不会向连接写出任何字节。
		 */
		void setChunkedUpload(bool enabled, size_t chunkSize = 8192);
		/**
		 * @brief 判断请求体是否以分块方式发送.
		 */
		bool isChunkedUpload() const;
		/**
		 * @brief 获取分块上传时单个数据块的最大字节数.
		 */
		size_t chunkSize() const;
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
		HttpResponse::ptr request(HttpMethod method, const std::string& uri, const std::string& body, ResponseCallback callback, DataCallback dataCallback = {});
		/**
		 * @brief 以事件驱动方式发起请求（不阻塞等待响应）.
		 * @param method 请求方法.
		 * @param uri 目标地址.
		 * @param body 请求体.
		 * @param callback 响应完成回调（在 selector 线程触发；连接失败时以 nullptr 调用）.
		 * @param dataCallback 响应体分段回调，可为空.
		 * @return 请求已发出返回 true.
		 * @note 建连（wss 含 TLS 握手）仍是阻塞的；请求发出后由 `onReadable()` 驱动解析响应.
		 * 同一客户端同一时刻只允许一个在途请求，见 `isBusy()`.
		 */
		bool requestAsync(HttpMethod method, const std::string& uri, const std::string& body,
			ResponseCallback callback, DataCallback dataCallback = {});
		/**
		 * @brief socket 可读时调用：解析响应，完成时触发回调.
		 * @return 连接仍可继续返回 true.
		 */
		bool onReadable();
		/**
		 * @brief socket 可写时调用：把请求数据写完.
		 * @return 仍有待发送数据返回 true.
		 */
		bool onWritable();
		/**
		 * @brief 是否有待发送数据.
		 */
		bool hasPendingWrite() const;
		/**
		 * @brief 是否有在途请求.
		 */
		bool isBusy() const;
		/**
		 * @brief 当前 socket，未连接时返回 INVALID_SOCKET.
		 */
		socket_t socket() const;
		/**
		 * @brief 注册事件注册/注销回调.
		 * @note 注册后只需要读事件的在途请求与有积压数据时的写事件由客户端自己维护；
		 * 通常直接绑定 `ActorInterface::registerEventOnThread` / `unregisterEventOnThread`.
		 */
		void registerSelectorCallbacks(RegisterEventCallback onRegister, UnregisterEventCallback onUnregister);
		/**
		 * @brief 注销本客户端占用的读写事件.
		 */
		void detachSelector();
		/**
		 * @brief 生成随机的 multipart boundary.
		 */
		static std::string generateBoundary();
		/**
		 * @brief 构造 multipart/form-data 请求体.
		 */
		static std::string buildMultipartBody(const std::vector<FormDataField>& fields, const std::vector<FormDataFile>& files, const std::string& boundary);

	private:
		/**
		 * @brief 确保当前请求目标的连接已经建立.
		 */
		bool ensureConnection(const std::string& host, uint16_t port, bool secure);
		/**
		 * @brief 发送全部请求数据.
		 */
		bool sendAll(const std::string& data);
		/**
		 * @brief 从当前连接读取完整响应.
		 * @param dataCallback 响应体数据片段回调，为空时不逐段交付.
		 */
		HttpResponse::ptr receiveResponse(const DataCallback& dataCallback);
		/**
		 * @brief 关闭当前 HTTP 连接.
		 */
		void closeConnection();
		/**
		 * @brief 切到非阻塞（首次异步请求时调用）.
		 */
		bool ensureNonBlocking();
		/**
		 * @brief 切回阻塞（阻塞 API 复用异步连接时调用）.
		 */
		bool ensureBlocking();
		/**
		 * @brief 把请求数据尽量写出去.
		 * @return 缓冲已清空返回 true；还有剩余（需要写事件）返回 false.
		 */
		bool flushSendBuffer();
		/**
		 * @brief 按当前状态注册/注销读写事件.
		 */
		void updateInterest();
		/**
		 * @brief 响应完成后的收尾：触发回调并按需关闭连接.
		 * @param closeAfter true 表示对端已关闭（响应以断链结束），不再复用连接.
		 */
		void finishResponse(bool closeAfter = false);
		/**
		 * @brief 在途请求失败：清理并以 nullptr 触发回调.
		 */
		void failPending();
		/**
		 * @brief 把分块上传选项应用到请求上.
		 */
		void applyChunkedUpload(HttpRequest& request) const;

	private:
		//! 默认请求头
		HttpHeader headers_;
		//! 当前复用的 HTTP 连接
		Socket::ptr socket_;
		//! 当前连接对应的主机
		std::string connectedHost_;
		//! 当前连接对应的端口
		uint16_t connectedPort_ = 0;
		//! 当前连接是否使用 TLS
		bool connectedSecure_ = false;
		//! 当前连接中尚未消费的响应数据
		std::string recvBuffer_;
		//! 待发送数据
		std::string sendBuffer_;
		//! 在途请求的响应解析器
		HttpResponseParser::ptr responseParser_;
		//! 在途请求的回调
		ResponseCallback pendingCallback_;
		//! 在途请求的响应体回调
		DataCallback pendingDataCallback_;
		//! 在途请求是否要求复用连接
		bool requestKeepAlive_ = true;
		//! 事件注册回调
		RegisterEventCallback registerEvent_;
		//! 事件注销回调
		UnregisterEventCallback unregisterEvent_;
		//! 已注册事件的 fd（-1 表示未注册）
		int32_t watchedFd_ = -1;
		//! 读事件是否已注册
		bool readRegistered_ = false;
		//! 写事件是否已注册
		bool writeRegistered_ = false;
		//! 是否事件驱动模式（socket 已切到非阻塞）
		bool async_ = false;
		//! 是否有在途请求
		bool busy_ = false;
		//! 请求体是否以分块方式发送
		bool chunkedUpload_ = false;
		//! 分块上传时单个数据块的最大字节数
		size_t chunkSize_ = 8192;
	};
}  // namespace crazy
