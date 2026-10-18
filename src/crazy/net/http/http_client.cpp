#include "crazy/net/http/http_client.h"

#include <cerrno>
#include <random>

#include "crazy/config.h"
#include "crazy/logger.h"
#include "crazy/net/http/http_version.h"
#include "crazy/net/socket.h"
#include "crazy/net/tls.h"
#include "crazy/uri.h"

// Windows 的 winnt.h 会定义 DELETE 宏，与 HttpMethod::DELETE 冲突，此处取消该宏
#undef DELETE

namespace crazy {
	namespace {
		/**
		 * @brief 解析请求地址：主机、端口、是否 TLS、请求目标（path?query）.
		 */
		void resolveTarget(const std::string& uri, std::string& host, uint16_t& port, bool& secure, std::string& target) {
			Uri u(uri);
			host = u.getHost();
			if (host.empty()) {
				host = "127.0.0.1";
			}
			// `https://` / `wss://` 必须真的走 TLS：要么握手成功，要么显式失败，
			// 绝不把请求明文发到 443
			const std::string scheme = u.getScheme();
			secure = (scheme == "https" || scheme == "wss");
			port = u.getPort();
			if (port == 0) {
				port = secure ? 443 : 80;
			}
			target = u.getPath();
			if (target.empty()) {
				target = "/";
			}
			if (!u.getQuery().empty()) {
				target += "?" + u.getQuery();
			}
		}
		/**
		 * @brief 构造请求：默认请求头 + Host + 默认 keep-alive.
		 */
		HttpRequest::ptr buildRequest(HttpMethod method, const std::string& host, const std::string& target,
			const std::string& body, const HttpHeader& headers) {
			auto request = std::make_shared<HttpRequest>(target);
			request->setMethod(method);
			request->setVersion(HttpVersion(1, 1));
			for (const auto& kv : headers) {
				request->headers().insert(kv.first, kv.second);
			}
			request->headers().insert("Host", host);
			if (!request->headers().contains("Connection")) {
				request->headers().insert("Connection", "keep-alive");
			}
			request->setBody(body);
			return request;
		}
	}
	void HttpClient::setHeader(const std::string& key, const std::string& value) {
		headers_.insert(key, value);
	}
	HttpHeader& HttpClient::headers() {
		return headers_;
	}
	void HttpClient::setChunkedUpload(bool enabled, size_t chunkSize) {
		chunkedUpload_ = enabled;
		chunkSize_ = chunkSize == 0 ? 8192 : chunkSize;
	}
	bool HttpClient::isChunkedUpload() const {
		return chunkedUpload_;
	}
	size_t HttpClient::chunkSize() const {
		return chunkSize_;
	}
	void HttpClient::applyChunkedUpload(HttpRequest& request) const {
		request.setChunkedTransferEncoding(chunkedUpload_);
		request.setChunkSize(chunkSize_);
	}
	void HttpClient::get(const std::string& uri, ResponseCallback callback) {
		request(HttpMethod::GET, uri, std::string(), std::move(callback));
	}
	void HttpClient::post(const std::string& uri, const std::string& body, ResponseCallback callback) {
		request(HttpMethod::POST, uri, body, std::move(callback));
	}
	void HttpClient::put(const std::string& uri, const std::string& body, ResponseCallback callback) {
		request(HttpMethod::PUT, uri, body, std::move(callback));
	}
	void HttpClient::del(const std::string& uri, ResponseCallback callback) {
		request(HttpMethod::DELETE, uri, std::string(), std::move(callback));
	}
	void HttpClient::head(const std::string& uri, ResponseCallback callback) {
		request(HttpMethod::HEAD, uri, std::string(), std::move(callback));
	}
	void HttpClient::options(const std::string& uri, ResponseCallback callback) {
		request(HttpMethod::OPTIONS, uri, std::string(), std::move(callback));
	}
	void HttpClient::patch(const std::string& uri, const std::string& body, ResponseCallback callback) {
		request(HttpMethod::PATCH, uri, body, std::move(callback));
	}
	void HttpClient::postForm(const std::string& uri,
		const std::vector<FormDataField>& fields,
		const std::vector<FormDataFile>& files,
		ResponseCallback callback) {
		const std::string boundary = generateBoundary();
		const std::string body = buildMultipartBody(fields, files, boundary);
		setHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
		post(uri, body, std::move(callback));
		headers_.erase("Content-Type");
	}
	HttpResponse::ptr HttpClient::request(HttpMethod method, const std::string& uri,
		const std::string& body, ResponseCallback callback, DataCallback dataCallback) {
		if (busy_) {
			CRAZY_SYSTEM_ERROR() << "http client has a pending async request, uri = " << uri;
			return nullptr;
		}
		if (chunkedUpload_ && headers_.contains("Content-Length")) {
			CRAZY_SYSTEM_ERROR() << "chunked upload must not set Content-Length, uri = " << uri;
			return nullptr;
		}
		std::string host;
		uint16_t port = 0;
		bool secure = false;
		std::string target;
		resolveTarget(uri, host, port, secure, target);
		auto req = buildRequest(method, host, target, body, headers_);
		applyChunkedUpload(*req);

		if (!ensureConnection(host, port, secure) || !ensureBlocking()) {
			closeConnection();
			return nullptr;
		}

		const std::string raw = req->toString();
		if (!sendAll(raw)) {
			closeConnection();
			return nullptr;
		}

		auto resp = receiveResponse(dataCallback);
		if (!resp) {
			closeConnection();
			return nullptr;
		}
		if (!req->shouldKeepAlive() || !resp->shouldKeepAlive()) {
			closeConnection();
		}
		if (callback && resp) {
			callback(resp);
		}
		return resp;
	}
	std::string HttpClient::generateBoundary() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dist(0, 15);
		static const char hex[] = "0123456789abcdef";
		std::string suffix;
		suffix.reserve(16);
		for (int i = 0; i < 16; ++i) {
			suffix += hex[dist(gen)];
		}
		return "----crazyBoundary" + suffix;
	}
	std::string HttpClient::buildMultipartBody(const std::vector<FormDataField>& fields,
		const std::vector<FormDataFile>& files, const std::string& boundary) {
		std::string body;
		for (const auto& field : fields) {
			body += "--" + boundary + "\r\n";
			body += "Content-Disposition: form-data; name=\"" + field.name + "\"\r\n";
			body += "\r\n";
			body += field.value + "\r\n";
		}
		for (const auto& file : files) {
			body += "--" + boundary + "\r\n";
			body += "Content-Disposition: form-data; name=\"" + file.name
				+ "\"; filename=\"" + file.filename + "\"\r\n";
			if (!file.contentType.empty()) {
				body += "Content-Type: " + file.contentType + "\r\n";
			}
			body += "\r\n";
			body += file.content + "\r\n";
		}
		body += "--" + boundary + "--\r\n";
		return body;
	}
	bool HttpClient::ensureConnection(const std::string& host, uint16_t port, bool secure) {
		if (socket_ && socket_->active() &&
			connectedHost_ == host && connectedPort_ == port && connectedSecure_ == secure) {
			return true;
		}
		closeConnection();
		if (secure) {
			// 客户端 TLS 参数取 `[HTTP]`/`[http]` 节，与 HttpApplication 的节名回退保持一致
			const std::string section = Config::HasSection("HTTP") ? "HTTP" : "http";
			TlsOptions options;
			options.caFile = Config::GetString(section, "tls_ca_file");
			options.minVersion = Config::GetString(section, "tls_min_version", "1.2");
			options.verifyPeer = Config::GetBoolean(section, "tls_verify_peer", true);
			std::string error;
			TlsContext::ptr context = TlsContext::createClient(options, error);
			if (!context) {
				CRAZY_SYSTEM_ERROR() << "https is not available, host = " << host << ":" << port
					<< ", section [" << section << "], reason = " << error;
				return false;
			}
			auto socket = std::make_shared<TlsSocket>(context, TlsSocket::Side::client);
			socket->setVerifyHost(host);
			if (!socket->connect(host, port)) {
				return false;
			}
			if (!socket->handshake(error)) {
				CRAZY_SYSTEM_ERROR() << "tls handshake failed, host = " << host << ":" << port
					<< ", reason = " << error;
				socket->abort();
				return false;
			}
			socket_ = std::move(socket);
			connectedHost_ = host;
			connectedPort_ = port;
			connectedSecure_ = true;
			return true;
		}
		auto socket = std::make_shared<Socket>();
		if (!socket->connect(host, port)) {
			return false;
		}
		socket_ = std::move(socket);
		connectedHost_ = host;
		connectedPort_ = port;
		connectedSecure_ = false;
		return true;
	}
	bool HttpClient::sendAll(const std::string& data) {
		if (!socket_) {
			return false;
		}
		size_t offset = 0;
		while (offset < data.size()) {
			const int32_t sent = socket_->send(data.data() + offset, data.size() - offset);
			if (sent <= 0) {
				if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
					continue;
				}
				return false;
			}
			offset += static_cast<size_t>(sent);
		}
		return true;
	}
	HttpResponse::ptr HttpClient::receiveResponse(const DataCallback& dataCallback) {
		if (!socket_) {
			return nullptr;
		}
		HttpResponseParser parser;
		parser.setDataCallback(dataCallback);
		char buffer[8192];
		while (!parser.isFinished() && !parser.hasError()) {
			if (recvBuffer_.empty()) {
				const int32_t received = socket_->recv(buffer, sizeof(buffer));
				if (received <= 0) {
					return nullptr;
				}
				recvBuffer_.append(buffer, static_cast<size_t>(received));
			}
			const size_t consumed = parser.execute(recvBuffer_.data(), recvBuffer_.size());
			if (consumed == 0 && !parser.isFinished()) {
				return nullptr;
			}
			if (consumed > 0) {
				recvBuffer_.erase(0, consumed);
			}
		}
		if (parser.hasError() || !parser.isFinished()) {
			return nullptr;
		}
		return parser.getResponse();
	}
	void HttpClient::closeConnection() {
		detachSelector();
		if (socket_) {
			socket_->close();
			socket_.reset();
		}
		connectedHost_.clear();
		connectedPort_ = 0;
		connectedSecure_ = false;
		recvBuffer_.clear();
		sendBuffer_.clear();
		responseParser_.reset();
		pendingCallback_ = nullptr;
		pendingDataCallback_ = nullptr;
		async_ = false;
		busy_ = false;
	}
	bool HttpClient::ensureNonBlocking() {
		if (async_) {
			return true;
		}
		if (!socket_ || !socket_->setNonBlocking(true)) {
			return false;
		}
		async_ = true;
		watchedFd_ = static_cast<int32_t>(socket_->socket());
		return true;
	}
	bool HttpClient::ensureBlocking() {
		if (!async_) {
			return true;
		}
		detachSelector();
		if (socket_ && !socket_->setNonBlocking(false)) {
			return false;
		}
		async_ = false;
		return true;
	}
	bool HttpClient::requestAsync(HttpMethod method, const std::string& uri, const std::string& body,
		ResponseCallback callback, DataCallback dataCallback) {
		if (busy_) {
			CRAZY_SYSTEM_ERROR() << "http client is busy, uri = " << uri;
			return false;
		}
		if (chunkedUpload_ && headers_.contains("Content-Length")) {
			CRAZY_SYSTEM_ERROR() << "chunked upload must not set Content-Length, uri = " << uri;
			return false;
		}
		std::string host;
		uint16_t port = 0;
		bool secure = false;
		std::string target;
		resolveTarget(uri, host, port, secure, target);

		if (!ensureConnection(host, port, secure) || !ensureNonBlocking()) {
			closeConnection();
			return false;
		}

		auto req = buildRequest(method, host, target, body, headers_);
		applyChunkedUpload(*req);
		requestKeepAlive_ = req->shouldKeepAlive();
		sendBuffer_ += req->toString();

		responseParser_ = std::make_shared<HttpResponseParser>();
		if (dataCallback) {
			responseParser_->setDataCallback(std::move(dataCallback));
		}
		pendingCallback_ = std::move(callback);
		pendingDataCallback_ = nullptr;
		busy_ = true;

		flushSendBuffer();
		if (!socket_) {
			// 发送阶段就失败了：以 nullptr 通知调用方
			failPending();
			return false;
		}
		updateInterest();
		return true;
	}
	bool HttpClient::onReadable() {
		if (!socket_ || !busy_ || !responseParser_) {
			return true;
		}
		char buf[8192];
		while (socket_ && busy_) {
			const int32_t n = socket_->recv(buf, sizeof(buf));
			if (n > 0) {
				recvBuffer_.append(buf, static_cast<size_t>(n));
				const size_t consumed = responseParser_->execute(recvBuffer_);
				if (consumed > 0) {
					recvBuffer_.erase(0, consumed);
				}
				if (responseParser_->hasError()) {
					CRAZY_SYSTEM_ERROR() << "http response parse fail, host = " << connectedHost_;
					failPending();
					return false;
				}
				if (responseParser_->isFinished()) {
					finishResponse();
					return socket_ != nullptr;
				}
				if (!async_) {
					return socket_ != nullptr;
				}
				continue;
			}
			if (0 == n) {
				// 对端关闭：把剩余数据喂完，能收全就当作响应完成（连接不再复用），否则算失败
				if (!recvBuffer_.empty()) {
					responseParser_->execute(recvBuffer_);
					recvBuffer_.clear();
				}
				if (responseParser_->isFinished() && !responseParser_->hasError()) {
					finishResponse(true);
				}
				else {
					failPending();
				}
				return false;
			}
			const int32_t error = WSAGetLastError();
			if (socket_ && (socketWouldBlock(error) || socket_->wantRead() || socket_->wantWrite())) {
				return true;
			}
			CRAZY_SYSTEM_ERROR() << "http recv fail, errno = " << error
				<< " errstr = " << socketErrorText(error);
			failPending();
			return false;
		}
		return socket_ != nullptr;
	}
	bool HttpClient::onWritable() {
		if (!flushSendBuffer()) {
			return true;
		}
		updateInterest();
		return false;
	}
	bool HttpClient::hasPendingWrite() const {
		return !sendBuffer_.empty();
	}
	bool HttpClient::isBusy() const {
		return busy_;
	}
	socket_t HttpClient::socket() const {
		return socket_ ? socket_->socket() : INVALID_SOCKET;
	}
	void HttpClient::registerSelectorCallbacks(RegisterEventCallback onRegister, UnregisterEventCallback onUnregister) {
		registerEvent_ = std::move(onRegister);
		unregisterEvent_ = std::move(onUnregister);
		if (socket_) {
			watchedFd_ = static_cast<int32_t>(socket_->socket());
			updateInterest();
		}
	}
	void HttpClient::detachSelector() {
		if (unregisterEvent_ && -1 != watchedFd_) {
			if (readRegistered_) {
				unregisterEvent_(watchedFd_, SelectorEventType::read);
			}
			if (writeRegistered_) {
				unregisterEvent_(watchedFd_, SelectorEventType::write);
			}
		}
		readRegistered_ = false;
		writeRegistered_ = false;
		watchedFd_ = -1;
	}
	void HttpClient::updateInterest() {
		if (!registerEvent_ || !unregisterEvent_) {
			return;
		}
		if (!socket_) {
			detachSelector();
			return;
		}
		const int32_t fd = (-1 != watchedFd_) ? watchedFd_ : static_cast<int32_t>(socket_->socket());
		watchedFd_ = fd;
		const bool wantRead = busy_;
		const bool wantWrite = !sendBuffer_.empty();
		if (wantRead != readRegistered_) {
			if (wantRead) {
				// 回调里可能注销本 fd 的事件，Selector 的分发已经先取出事件句柄再调用，允许这么做
				registerEvent_(fd, SelectorEventType::read, [this] {
					if (!onReadable()) {
						detachSelector();
					}
				});
			}
			else {
				unregisterEvent_(fd, SelectorEventType::read);
			}
			readRegistered_ = wantRead;
		}
		if (wantWrite != writeRegistered_) {
			if (wantWrite) {
				registerEvent_(fd, SelectorEventType::write, [this] { onWritable(); });
			}
			else {
				unregisterEvent_(fd, SelectorEventType::write);
			}
			writeRegistered_ = wantWrite;
		}
	}
	bool HttpClient::flushSendBuffer() {
		if (!socket_) {
			sendBuffer_.clear();
			return true;
		}
		while (!sendBuffer_.empty()) {
			const int32_t sent = socket_->send(sendBuffer_.data(), sendBuffer_.size());
			if (sent > 0) {
				sendBuffer_.erase(0, static_cast<size_t>(sent));
				continue;
			}
			const int32_t error = WSAGetLastError();
			if (socketWouldBlock(error) || socket_->wantWrite()) {
				return false;
			}
			CRAZY_SYSTEM_ERROR() << "http send fail, errno = " << error
				<< " errstr = " << socketErrorText(error);
			closeConnection();
			return true;
		}
		return true;
	}
	void HttpClient::finishResponse(bool closeAfter) {
		HttpResponse::ptr response = responseParser_ ? responseParser_->getResponse() : nullptr;
		const bool keepAlive = !closeAfter && responseParser_ && responseParser_->shouldKeepAlive() && requestKeepAlive_;
		ResponseCallback callback = std::move(pendingCallback_);
		pendingCallback_ = nullptr;
		pendingDataCallback_ = nullptr;
		responseParser_.reset();
		busy_ = false;
		if (keepAlive) {
			updateInterest();
		}
		else {
			closeConnection();
		}
		if (callback) {
			callback(response);
		}
	}
	void HttpClient::failPending() {
		ResponseCallback callback = std::move(pendingCallback_);
		pendingCallback_ = nullptr;
		pendingDataCallback_ = nullptr;
		responseParser_.reset();
		busy_ = false;
		closeConnection();
		if (callback) {
			callback(nullptr);
		}
	}
}  // namespace crazy
