/**
 * @file http_router.h
 * @author keisum (keisumhuis@gmail.com)
 * @brief HTTP 路由注册和分发.
 * @version 0.1
 * @date 2026-09-06
 */
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <utility>

#include "crazy/net/http/http11_common.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/utils.h"

namespace crazy {
	/**
	 * @brief HTTP 路由.
	 */
	class HttpRouter {
	public:
		//! 智能指针声明
		using ptr = std::shared_ptr<HttpRouter>;
		//! 路由处理器类型
		using handler_type = std::function<void(HttpRequest&, HttpResponse&)>;
		/**
		 * @brief 构造函数.
		 */
		HttpRouter() = default;
		/**
		 * @brief 注册自由函数或绑定函数.
		 */
		template <typename Function>
		bool add(HttpMethod method, const std::string& path, Function&& handler) {
			return addHandler(method, path, handler_type(std::forward<Function>(handler)));
		}
		/**
		 * @brief 注册成员函数并绑定对象引用.
		 */
		template <typename Class, typename Member>
		bool add(HttpMethod method, const std::string& path,
			Member Class::* handler, Class& object) {
			return addHandler(method, path, [object = &object, handler](HttpRequest& request, HttpResponse& response) {
				std::invoke(handler, *object, request, response);
				});
		}
		/**
		 * @brief 注册成员函数并绑定对象指针.
		 */
		template <typename Class, typename Member>
		bool add(HttpMethod method, const std::string& path,
			Member Class::* handler, Class* object) {
			return addHandler(method, path, [object, handler](HttpRequest& request, HttpResponse& response) {
				if (object != nullptr) {
					std::invoke(handler, *object, request, response);
				}
				});
		}
		/**
		 * @brief 注册成员函数并绑定 shared_ptr 对象.
		 */
		template <typename Class, typename Member>
		bool add(HttpMethod method, const std::string& path,
			Member Class::* handler, const std::shared_ptr<Class>& object) {
			return addHandler(method, path, [object, handler](HttpRequest& request, HttpResponse& response) {
				if (object) {
					std::invoke(handler, *object, request, response);
				}
				});
		}
		/**
		 * @brief 按模板方法注册自由函数或绑定函数.
		 */
		template <HttpMethod method, typename Function>
		bool setHttpHandler(const std::string& path, Function&& handler) {
			return add(method, path, std::forward<Function>(handler));
		}
		/**
		 * @brief 按模板方法注册成员函数并绑定对象引用.
		 */
		template <HttpMethod method, typename Class, typename Member>
		bool setHttpHandler(const std::string& path, Member Class::* handler, Class& object) {
			return add(method, path, handler, object);
		}
		/**
		 * @brief 按模板方法注册成员函数并绑定对象指针.
		 */
		template <HttpMethod method, typename Class, typename Member>
		bool setHttpHandler(const std::string& path, Member Class::* handler, Class* object) {
			return add(method, path, handler, object);
		}
		/**
		 * @brief 按模板方法注册成员函数并绑定 shared_ptr 对象.
		 */
		template <HttpMethod method, typename Class, typename Member>
		bool setHttpHandler(const std::string& path, Member Class::* handler, const std::shared_ptr<Class>& object) {
			return add(method, path, handler, object);
		}
		/**
		 * @brief 注册静态文件目录.
		 */
		bool addStaticDirectory(const std::string& path, const std::string& directory,
			bool enableDirectoryListing = false) {
			std::string normalizedPath = path.empty() ? "/" : path;
			if (normalizedPath[0] != '/') {
				normalizedPath.insert(normalizedPath.begin(), '/');
			}
			while (normalizedPath.size() > 1 && normalizedPath.back() == '/') {
				normalizedPath.pop_back();
			}

			if (!PathUtil::IsDirectory(directory)) {
				return false;
			}
			std::error_code error;
			const std::filesystem::path root =
				std::filesystem::weakly_canonical(std::filesystem::path(directory), error);
			if (error) {
				return false;
			}
			for (const auto& item : staticDirectories_) {
				if (item.path == normalizedPath) {
					return false;
				}
			}
			staticDirectories_.push_back({ normalizedPath, root, enableDirectoryListing });
			return true;
		}
		/**
		 * @brief 分发 HTTP 请求.
		 * @return 是否找到处理器.
		 */
		bool handle(HttpRequest& request, HttpResponse& response) const;
		/**
		 * @brief 分发 HTTP 请求.
		 * @return 是否找到处理器.
		 */
		bool route(HttpRequest& request, HttpResponse& response) const {
			return handle(request, response);
		}
		/**
		 * @brief 判断是否已注册某个路由.
		 */
		bool contains(HttpMethod method, const std::string& path) const;
		/**
		 * @brief 清空所有路由.
		 */
		void clear() {
			handlers_.clear();
			staticDirectories_.clear();
		}
		/**
		 * @brief 获取已注册的路由数量.
		 */
		std::size_t size() const {
			return handlers_.size();
		}

	private:
		//! 静态文件目录
		struct StaticDirectory {
			std::string path;
			std::filesystem::path directory;
			bool enableDirectoryListing = false;
		};
		/**
		 * @brief 内部注册处理器.
		 */
		bool addHandler(HttpMethod method, const std::string& path, handler_type handler);
		/**
		 * @brief 分发静态文件请求.
		 */
		bool handleStaticDirectory(HttpRequest& request, HttpResponse& response) const;
		/**
		 * @brief 判断 URL 路径是否包含不安全的路径段.
		 */
		static bool hasUnsafePathSegment(const std::string& path);
		/**
		 * @brief 获取静态文件 MIME 类型.
		 */
		static std::string staticFileContentType(const std::filesystem::path& path);
		/**
		 * @brief 生成静态目录页面.
		 */
		static std::string staticDirectoryListing(
			const std::filesystem::path& directory,
			const std::string& requestPath,
			const std::string& basePath);
		/**
		 * @brief 转义 HTML 文本.
		 */
		static std::string htmlEscape(const std::string& value);
		/**
		 * @brief 转义 URL 路径段.
		 */
		static std::string urlEncodePathSegment(const std::string& value);
		/**
		 * @brief 生成 METHOD path 路由键.
		 */
		static std::string makeRouteKey(HttpMethod method, const std::string& path);
		//! 路由处理器表
		std::unordered_map<std::string, handler_type> handlers_;
		//! 静态文件目录列表
		std::vector<StaticDirectory> staticDirectories_;
	};
	inline bool HttpRouter::handle(HttpRequest& request, HttpResponse& response) const {
		const std::string key = makeRouteKey(request.method(), request.uri().getPath());
		const auto it = handlers_.find(key);
		if (it != handlers_.end()) {
			it->second(request, response);
			return true;
		}
		return handleStaticDirectory(request, response);
	}
	inline bool HttpRouter::contains(HttpMethod method, const std::string& path) const {
		return handlers_.find(makeRouteKey(method, path)) != handlers_.end();
	}
	inline bool HttpRouter::addHandler(HttpMethod method,
		const std::string& path, handler_type handler) {
		const auto [it, inserted] = handlers_.emplace(makeRouteKey(method, path), std::move(handler));
		return inserted;
	}
	inline bool HttpRouter::handleStaticDirectory(HttpRequest& request, HttpResponse& response) const {
		if (request.method() != HttpMethod::GET) {
			return false;
		}

		const std::string requestPath = request.uri().getPath().empty() ? "/" : request.uri().getPath();
		const StaticDirectory* matchedDirectory = nullptr;
		for (const auto& item : staticDirectories_) {
			const bool matched = item.path == "/" ? requestPath[0] == '/' : requestPath == item.path || (requestPath.size() > item.path.size() && requestPath.compare(0, item.path.size(), item.path) == 0 && requestPath[item.path.size()] == '/');
			if (matched && (matchedDirectory == nullptr || item.path.size() > matchedDirectory->path.size())) {
				matchedDirectory = &item;
			}
		}
		if (matchedDirectory == nullptr) {
			return false;
		}

		if (hasUnsafePathSegment(requestPath)) {
			response.setStatus(HttpStatus::FORBIDDEN);
			response.setBody("Forbidden");
			return true;
		}

		std::string relativePath;
		if (matchedDirectory->path == "/") {
			relativePath = requestPath.substr(1);
		}
		else if (requestPath.size() > matchedDirectory->path.size()) {
			relativePath = requestPath.substr(matchedDirectory->path.size() + 1);
		}
		while (!relativePath.empty() && relativePath.front() == '/') {
			relativePath.erase(relativePath.begin());
		}

		std::error_code error;
		std::error_code canonicalError;
		const std::string requestedFile = PathUtil::JoinPath(matchedDirectory->directory.string(), relativePath);
		const std::filesystem::path filePath = std::filesystem::weakly_canonical(std::filesystem::path(requestedFile), canonicalError);
		std::error_code relativeError;
		const std::filesystem::path relativeToRoot = std::filesystem::relative(filePath, matchedDirectory->directory, relativeError);
		const auto firstSegment = relativeToRoot.begin();
		const bool outsideRoot = canonicalError || relativeError || (relativeToRoot != std::filesystem::path() && firstSegment != relativeToRoot.end() && *firstSegment == std::filesystem::path(".."));
		if (outsideRoot) {
			response.setStatus(HttpStatus::FORBIDDEN);
			response.setBody("Forbidden");
			return true;
		}
		error.clear();
		if (std::filesystem::is_directory(filePath, error)) {
			if (!matchedDirectory->enableDirectoryListing) {
				response.setStatus(HttpStatus::NOT_FOUND);
				response.setBody("Not Found");
				return true;
			}
			if (!requestPath.empty() && requestPath.back() != '/') {
				response.setStatus(HttpStatus::MOVED_PERMANENTLY);
				response.headers().insert("Location", requestPath + "/");
				response.setBody("Moved Permanently");
				return true;
			}
			response.setStatus(HttpStatus::OK);
			response.headers().insert("Content-Type", "text/html; charset=utf-8");
			const std::string basePath = matchedDirectory->path == "/"
				? "/" : matchedDirectory->path + "/";
			response.setBody(staticDirectoryListing(filePath, requestPath, basePath));
			return true;
		}
		if (error || !PathUtil::IsFile(filePath.string())) {
			response.setStatus(HttpStatus::NOT_FOUND);
			response.setBody("Not Found");
			return true;
		}

		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file) {
			response.setStatus(HttpStatus::NOT_FOUND);
			response.setBody("Not Found");
			return true;
		}
		const std::streamsize size = file.tellg();
		if (size < 0) {
			response.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
			response.setBody("Internal Server Error");
			return true;
		}
		std::string body(static_cast<size_t>(size), '\0');
		file.seekg(0, std::ios::beg);
		if (size > 0 && !file.read(body.data(), size)) {
			response.setStatus(HttpStatus::INTERNAL_SERVER_ERROR);
			response.setBody("Internal Server Error");
			return true;
		}

		response.setStatus(HttpStatus::OK);
		response.headers().insert("Content-Type", staticFileContentType(filePath));
		response.setBody(body);
		return true;
	}
	inline bool HttpRouter::hasUnsafePathSegment(const std::string& path) {
		size_t begin = 0;
		while (begin <= path.size()) {
			const size_t end = path.find('/', begin);
			const std::string segment = path.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
			if (segment == "." || segment == "..") {
				return true;
			}
			if (end == std::string::npos) {
				break;
			}
			begin = end + 1;
		}
		return false;
	}
	inline std::string HttpRouter::staticFileContentType(const std::filesystem::path& path) {
		const std::string extension = PathUtil::GetFileExtension(path.string());
		if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
		if (extension == ".css") return "text/css; charset=utf-8";
		if (extension == ".js") return "application/javascript; charset=utf-8";
		if (extension == ".json") return "application/json; charset=utf-8";
		if (extension == ".txt") return "text/plain; charset=utf-8";
		if (extension == ".xml") return "application/xml; charset=utf-8";
		if (extension == ".svg") return "image/svg+xml";
		if (extension == ".png") return "image/png";
		if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
		if (extension == ".gif") return "image/gif";
		if (extension == ".webp") return "image/webp";
		if (extension == ".ico") return "image/x-icon";
		if (extension == ".wasm") return "application/wasm";
		return "application/octet-stream";
	}
	inline std::string HttpRouter::staticDirectoryListing(const std::filesystem::path& directory, const std::string& requestPath, const std::string& basePath) {
		std::string listingPath = requestPath.empty() ? "/" : requestPath;
		if (listingPath.back() != '/') {
			listingPath += '/';
		}

		std::string parentPath;
		if (listingPath != basePath) {
			const std::string relativePath = listingPath.substr(basePath.size());
			const size_t slash = relativePath.find_last_of('/', relativePath.size() - 2);
			parentPath = slash == std::string::npos ? basePath : basePath + relativePath.substr(0, slash + 1);
		}

		std::vector<std::pair<std::string, bool>> entries;
		std::error_code error;
		for (std::filesystem::directory_iterator it(directory, error), end;
			!error && it != end; it.increment(error)) {
			std::error_code entryError;
			const auto entryPath = std::filesystem::weakly_canonical(it->path(), entryError);
			if (entryError) {
				continue;
			}
			const auto relativeEntry = std::filesystem::relative(entryPath, directory, entryError);
			const auto firstSegment = relativeEntry.begin();
			if (entryError || relativeEntry.empty() || (firstSegment != relativeEntry.end() && *firstSegment == std::filesystem::path(".."))) {
				continue;
			}
			const bool isDirectory = it->is_directory(entryError);
			if (entryError) {
				continue;
			}
			entries.emplace_back(it->path().filename().string(), isDirectory);
		}
		std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
			return left.first < right.first;
			});

		std::ostringstream html;
		html << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Index of "
			<< htmlEscape(listingPath)
			<< "</title></head><body><h1>Index of "
			<< htmlEscape(listingPath) << "</h1><ul>";
		if (!parentPath.empty()) {
			html << "<li><a href=\"" << htmlEscape(parentPath) << "\">../</a></li>";
		}
		for (const auto& [name, isDirectory] : entries) {
			const std::string displayName = name + (isDirectory ? "/" : "");
			html << "<li><a href=\"" << htmlEscape(listingPath + urlEncodePathSegment(name) + (isDirectory ? "/" : "")) << "\">" << htmlEscape(displayName) << "</a></li>";
		}
		html << "</ul></body></html>";
		return html.str();
	}
	inline std::string HttpRouter::htmlEscape(const std::string& value) {
		std::string result;
		result.reserve(value.size());
		for (const char c : value) {
			switch (c) {
			case '&': result += "&amp;"; break;
			case '<': result += "&lt;"; break;
			case '>': result += "&gt;"; break;
			case '"': result += "&quot;"; break;
			case '\'': result += "&#39;"; break;
			default: result += c; break;
			}
		}
		return result;
	}
	inline std::string HttpRouter::urlEncodePathSegment(const std::string& value) {
		const char hex[] = "0123456789ABCDEF";
		std::string result;
		result.reserve(value.size());
		for (const unsigned char c : value) {
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
				result += static_cast<char>(c);
			}
			else {
				result += '%';
				result += hex[(c >> 4) & 0x0F];
				result += hex[c & 0x0F];
			}
		}
		return result;
	}
	inline std::string HttpRouter::makeRouteKey(HttpMethod method, const std::string& path) {
		std::string key = httpMethodToString(method);
		key += ' ';
		if (path.empty()) {
			key += '/';
		}
		else if (path[0] == '/') {
			key += crazy::StringUtil::ToLower(path);
		}
		else {
			key += '/';
			key += crazy::StringUtil::ToLower(path);
		}
		return key;
	}
}
