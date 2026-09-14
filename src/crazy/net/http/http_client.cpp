#include "crazy/net/http/http_client.h"

#include <cerrno>
#include <random>

#include "crazy/net/http/http_version.h"
#include "crazy/net/socket.h"
#include "crazy/uri.h"

// Windows 的 winnt.h 会定义 DELETE 宏，与 HttpMethod::DELETE 冲突，此处取消该宏
#undef DELETE

namespace crazy {
	void HttpClient::setHeader(const std::string& key, const std::string& value) {
		headers_.insert(key, value);
	}
	HttpHeader& HttpClient::headers() {
		return headers_;
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
		const std::string& body, ResponseCallback callback) {
		Uri u(uri);
		std::string host = u.getHost();
		if (host.empty()) {
			host = "127.0.0.1";
		}
		uint16_t port = u.getPort();
		if (port == 0) {
			const std::string scheme = u.getScheme();
			if (scheme == "https" || scheme == "wss") {
				port = 443;
			}
			else {
				port = 80;
			}
		}

		std::string target = u.getPath();
		if (target.empty()) {
			target = "/";
		}
		if (!u.getQuery().empty()) {
			target += "?" + u.getQuery();
		}

		auto req = std::make_shared<HttpRequest>(target);
		req->setMethod(method);
		req->setVersion(HttpVersion(1, 1));
		for (const auto& kv : headers_) {
			req->headers().insert(kv.first, kv.second);
		}
		req->headers().insert("Host", host);
		if (!req->headers().contains("Connection")) {
			req->headers().insert("Connection", "keep-alive");
		}
		req->setBody(body);

		if (!ensureConnection(host, port)) {
			return nullptr;
		}

		const std::string raw = req->toString();
		if (!sendAll(raw)) {
			closeConnection();
			return nullptr;
		}

		auto resp = receiveResponse();
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
	bool HttpClient::ensureConnection(const std::string& host, uint16_t port) {
		if (socket_ && socket_->active() &&
			connectedHost_ == host && connectedPort_ == port) {
			return true;
		}
		closeConnection();
		auto socket = std::make_shared<Socket>();
		if (!socket->connect(host, port)) {
			return false;
		}
		socket_ = std::move(socket);
		connectedHost_ = host;
		connectedPort_ = port;
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
	HttpResponse::ptr HttpClient::receiveResponse() {
		if (!socket_) {
			return nullptr;
		}
		HttpResponseParser parser;
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
		if (socket_) {
			socket_->close();
			socket_.reset();
		}
		connectedHost_.clear();
		connectedPort_ = 0;
		recvBuffer_.clear();
	}
}  // namespace crazy
