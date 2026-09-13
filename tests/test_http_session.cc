#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_router.h"
#include "crazy/net/http/http_session.h"
#include "crazy/net/socket.h"
#include "crazy/utils.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {
	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	class CaptureSocket final : public crazy::Socket {
	public:
		int32_t send(const void* buffer, size_t length, int32_t) override {
			sentData_.append(static_cast<const char*>(buffer), length);
			return static_cast<int32_t>(length);
		}

		const std::string& sentData() const {
			return sentData_;
		}

	private:
		//! 已发送数据
		std::string sentData_;
	};

	class TestHttpSession final : public crazy::HttpSession {
	public:
		using crazy::HttpSession::HttpSession;
		using crazy::HttpSession::handleRequest;
	};
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
#ifdef _WIN32
	WSADATA wsaData;
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

	{
		auto router = std::make_shared<crazy::HttpRouter>();
		CHECK(router->add(crazy::HttpMethod::GET, "/sync",
			[](crazy::HttpRequest&, crazy::HttpResponse& response) {
				response.setBody("sync");
			}));

		auto socket = std::make_shared<CaptureSocket>();
		auto session = std::make_shared<TestHttpSession>(socket, router);
		auto request = std::make_shared<crazy::HttpRequest>("/sync");
		request->setMethod(crazy::HttpMethod::GET);

		session->handleRequest(request);

		CHECK(socket->sentData().find("HTTP/1.1 200 OK\r\n") == 0);
		CHECK(socket->sentData().find("Content-Length: 4\r\n") != std::string::npos);
		CHECK(socket->sentData().find("\r\n\r\nsync") != std::string::npos);
	}

	{
		auto router = std::make_shared<crazy::HttpRouter>();
		crazy::HttpResponse::ptr deferredResponse;
		CHECK(router->add(crazy::HttpMethod::GET, "/defer",
			[&deferredResponse](crazy::HttpRequest&, crazy::HttpResponse& response) {
				deferredResponse = response.defer();
			}));

		auto socket = std::make_shared<CaptureSocket>();
		auto session = std::make_shared<TestHttpSession>(socket, router);
		std::function<void()> postedTask;
		session->registerPostCallback([&postedTask](std::function<void()> callback) {
			postedTask = std::move(callback);
			});

		auto request = std::make_shared<crazy::HttpRequest>("/defer");
		request->setMethod(crazy::HttpMethod::GET);
		session->handleRequest(request);

		CHECK(deferredResponse != nullptr);
		CHECK(deferredResponse->isDeferred());
		CHECK(!deferredResponse->isSent());
		CHECK(socket->sentData().empty());
		CHECK(!postedTask);

		deferredResponse->setBody("deferred");
		deferredResponse->send();

		CHECK(deferredResponse->isSent());
		CHECK(static_cast<bool>(postedTask));
		CHECK(socket->sentData().empty());

		auto firstPostedTask = std::move(postedTask);
		deferredResponse->send();
		CHECK(!postedTask);
		firstPostedTask();

		CHECK(socket->sentData().find("HTTP/1.1 200 OK\r\n") == 0);
		CHECK(socket->sentData().find("Content-Length: 8\r\n") != std::string::npos);
		CHECK(socket->sentData().find("\r\n\r\ndeferred") != std::string::npos);
	}

	const std::string staticDirectory =
		crazy::PathUtil::CreateTempDirectory("crazy_http_static_test");
	CHECK(!staticDirectory.empty());
	const std::string staticFile =
		crazy::PathUtil::JoinPath(staticDirectory, "index.html");
	const std::string nestedDirectory =
		crazy::PathUtil::JoinPath(staticDirectory, "nested");
	CHECK(crazy::PathUtil::CreateDir(nestedDirectory, true));
	const std::string nestedFile =
		crazy::PathUtil::JoinPath(nestedDirectory, "data.json");
	{
		std::ofstream file(staticFile, std::ios::binary);
		CHECK(static_cast<bool>(file));
		file << "<h1>crazy</h1>";
	}
	{
		std::ofstream file(nestedFile, std::ios::binary);
		CHECK(static_cast<bool>(file));
		file << "{\"message\":\"crazy\"}";
	}

	{
		crazy::HttpRouter router;
		CHECK(router.addStaticDirectory("/assets", staticDirectory));
		CHECK(!router.addStaticDirectory("/assets", staticDirectory));

		crazy::HttpRequest request("/assets/index.html");
		request.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse response;
		CHECK(router.handle(request, response));
		CHECK(response.status() == crazy::HttpStatus::OK);
		CHECK(response.body() == "<h1>crazy</h1>");
		const auto contentType = response.headers().get("Content-Type");
		CHECK(contentType && *contentType == "text/html; charset=utf-8");

		crazy::HttpRequest disabledListingRequest("/assets/");
		disabledListingRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse disabledListingResponse;
		CHECK(router.handle(disabledListingRequest, disabledListingResponse));
		CHECK(disabledListingResponse.status() == crazy::HttpStatus::NOT_FOUND);

		crazy::HttpRequest missingRequest("/assets/missing.txt");
		missingRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse missingResponse;
		CHECK(router.handle(missingRequest, missingResponse));
		CHECK(missingResponse.status() == crazy::HttpStatus::NOT_FOUND);

		crazy::HttpRequest parentRequest("/assets/../index.html");
		parentRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse parentResponse;
		CHECK(router.handle(parentRequest, parentResponse));
		CHECK(parentResponse.status() == crazy::HttpStatus::FORBIDDEN);

		crazy::HttpRequest currentRequest("/assets/./index.html");
		currentRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse currentResponse;
		CHECK(router.handle(currentRequest, currentResponse));
		CHECK(currentResponse.status() == crazy::HttpStatus::FORBIDDEN);

		crazy::HttpRequest postRequest("/assets/index.html");
		postRequest.setMethod(crazy::HttpMethod::POST);
		crazy::HttpResponse postResponse;
		CHECK(!router.handle(postRequest, postResponse));
	}

	{
		crazy::HttpRouter router;
		CHECK(router.addStaticDirectory("/browse", staticDirectory, true));

		crazy::HttpRequest redirectRequest("/browse");
		redirectRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse redirectResponse;
		CHECK(router.handle(redirectRequest, redirectResponse));
		CHECK(redirectResponse.status() == crazy::HttpStatus::MOVED_PERMANENTLY);
		const auto location = redirectResponse.headers().get("Location");
		CHECK(location && *location == "/browse/");

		crazy::HttpRequest listingRequest("/browse/");
		listingRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse listingResponse;
		CHECK(router.handle(listingRequest, listingResponse));
		CHECK(listingResponse.status() == crazy::HttpStatus::OK);
		CHECK(listingResponse.body().find("index.html") != std::string::npos);
		CHECK(listingResponse.body().find("nested/") != std::string::npos);
		const auto listingContentType = listingResponse.headers().get("Content-Type");
		CHECK(listingContentType &&
			*listingContentType == "text/html; charset=utf-8");

		crazy::HttpRequest nestedListingRequest("/browse/nested/");
		nestedListingRequest.setMethod(crazy::HttpMethod::GET);
		crazy::HttpResponse nestedListingResponse;
		CHECK(router.handle(nestedListingRequest, nestedListingResponse));
		CHECK(nestedListingResponse.status() == crazy::HttpStatus::OK);
		CHECK(nestedListingResponse.body().find("data.json") != std::string::npos);
		CHECK(nestedListingResponse.body().find("../") != std::string::npos);
	}

	CHECK(crazy::PathUtil::RemoveDirectoryRecursive(staticDirectory));

	std::cout << "http session tests passed" << std::endl;
#ifdef _WIN32
	WSACleanup();
#endif
	return 0;
}

#undef CHECK
