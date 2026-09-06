#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "crazy/net/http/http_multipart.h"
#include "crazy/net/http/http_request.h"
#include "crazy/net/http/http_response.h"
#include "crazy/net/http/http_router.h"

namespace {
	int failures = 0;

	void check(bool condition, const char* expression, int line) {
		if (!condition) {
			std::cerr << "check failed at line " << line << ": " << expression << std::endl;
			std::exit(1);
		}
	}

	void routeGlobal(crazy::HttpRequest&, crazy::HttpResponse& response) {
		response.setBody("global");
	}

	class RouteController {
	public:
		void hello(crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("member");
		}
		void bound(crazy::HttpRequest&, crazy::HttpResponse& response) {
			response.setBody("bound");
		}
	};
}  // namespace

#define CHECK(expression) check((expression), #expression, __LINE__)

int main() {
	crazy::HttpVersion version;
	CHECK(version.toString() == "HTTP/1.1");

	crazy::HttpRequest request("/hello");
	request.setVersion(crazy::HttpVersion(1, 1));
	request.setMethod(crazy::HttpMethod::POST);
	request.headers().insert("Host", "example.com");
	request.setBody("abc");

	const std::string serializedRequest = request.toString();
	CHECK(serializedRequest.rfind("POST /hello HTTP/1.1\r\n", 0) == 0);
	CHECK(serializedRequest.find("host: example.com\r\n") != std::string::npos);
	CHECK(serializedRequest.find("Content-Length: 3\r\n") != std::string::npos);
	CHECK(serializedRequest.size() >= 3 &&
		serializedRequest.compare(serializedRequest.size() - 3, 3, "abc") == 0);

	const std::string encodedForm =
		"POST /submit?x=1 HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 20\r\n"
		"\r\n"
		"name=John+Doe&age=13";

	crazy::HttpRequestParser requestParser;
	CHECK(requestParser.execute(encodedForm) == encodedForm.size());
	CHECK(requestParser.isFinished());
	CHECK(!requestParser.hasError());
	crazy::HttpRequest::ptr parsedRequest = requestParser.getRequest();
	CHECK(parsedRequest->method() == crazy::HttpMethod::POST);
	CHECK(parsedRequest->uri().getPath() == "/submit");
	CHECK(parsedRequest->uri().getQuery() == "x=1");
	CHECK(parsedRequest->body() == "name=John+Doe&age=13");
	CHECK(parsedRequest->parseMultipart() == nullptr);

	const std::vector<std::pair<std::string, std::string>> fields =
		parsedRequest->parseFormUrlEncoded();
	CHECK(fields.size() == 2);
	CHECK(fields[0].first == "name");
	CHECK(fields[0].second == "John Doe");
	CHECK(fields[1].first == "age");
	CHECK(fields[1].second == "13");

	const std::string responseRaw =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 2\r\n"
		"\r\n"
		"ok";

	crazy::HttpResponseParser responseParser;
	CHECK(responseParser.execute(responseRaw) == responseRaw.size());
	CHECK(responseParser.isFinished());
	CHECK(!responseParser.hasError());
	crazy::HttpResponse::ptr parsedResponse = responseParser.getResponse();
	CHECK(parsedResponse->status() == crazy::HttpStatus::OK);
	CHECK(parsedResponse->reasonPhrase() == "OK");
	CHECK(parsedResponse->body() == "ok");
	CHECK(parsedResponse->parseMultipart() == nullptr);
	CHECK(parsedResponse->parseFormUrlEncoded().empty());

	const std::string boundary = "boundary123";
	const std::string contentType =
		"multipart/form-data; boundary=" + boundary;
	const std::string multipartBody =
		"--" + boundary + "\r\n"
		"Content-Disposition: form-data; name=\"username\"\r\n"
		"\r\n"
		"alice\r\n"
		"--" + boundary + "\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"a.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"hello\r\n"
		"--" + boundary + "--\r\n";

	crazy::HttpMultipartParser::ptr multipartParser =
		crazy::HttpMultipartParser::parse(contentType, multipartBody);
	CHECK(multipartParser->isFinished());
	CHECK(!multipartParser->hasError());
	const std::vector<crazy::HttpMultipartPart>& parts = multipartParser->parts();
	CHECK(parts.size() == 2);
	CHECK(parts[0].name() == "username");
	CHECK(parts[0].body == "alice");
	CHECK(parts[1].name() == "file");
	CHECK(parts[1].filename() == "a.txt");
	CHECK(parts[1].contentType() == "text/plain");
	CHECK(parts[1].body == "hello");

	const std::string multipartRequest =
		"POST /upload HTTP/1.1\r\n"
		"Host: example.com\r\n"
		"Content-Type: " + contentType + "\r\n" +
		"Content-Length: " + std::to_string(multipartBody.size()) + "\r\n"
		"\r\n" +
		multipartBody;

	crazy::HttpRequestParser multipartRequestParser;
	CHECK(multipartRequestParser.execute(multipartRequest) == multipartRequest.size());
	CHECK(multipartRequestParser.isFinished());
	CHECK(!multipartRequestParser.hasError());
	crazy::HttpRequest::ptr uploadedRequest = multipartRequestParser.getRequest();
	crazy::HttpMultipartParser::ptr uploadedMultipart = uploadedRequest->parseMultipart();
	CHECK(uploadedMultipart != nullptr);
	CHECK(uploadedMultipart->isFinished());
	CHECK(!uploadedMultipart->hasError());
	CHECK(uploadedMultipart->parts().size() == 2);

	const std::string websocketKey = "dGhlIHNhbXBsZSBub25jZQ==";
	crazy::HttpRequest::ptr websocketRequest =
		crazy::HttpRequest::createWebSocketRequest(
			crazy::Uri("ws://example.com/chat"), websocketKey);
	CHECK(websocketRequest->method() == crazy::HttpMethod::GET);
	CHECK(websocketRequest->isWebSocket());
	CHECK(websocketRequest->isWebSocketHandshake());
	const auto websocketKeyHeader = websocketRequest->headers().get("Sec-WebSocket-Key");
	CHECK(websocketKeyHeader && *websocketKeyHeader == websocketKey);

	CHECK(crazy::HttpResponse::computeWebSocketAccept(websocketKey) ==
		"s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

	crazy::HttpResponse websocketResponse;
	websocketResponse.setVersion(crazy::HttpVersion(1, 1));
	websocketResponse.setStatus(crazy::HttpStatus::SWITCHING_PROTOCOLS);
	websocketResponse.setReasonPhrase("Switching Protocols");
	websocketResponse.headers().insert("Connection", "Upgrade");
	websocketResponse.headers().insert("Upgrade", "websocket");
	websocketResponse.headers().insert(
		"Sec-WebSocket-Accept", crazy::HttpResponse::computeWebSocketAccept(websocketKey));
	CHECK(websocketResponse.isWebSocket());
	CHECK(websocketResponse.isWebSocketHandshake());
	CHECK(websocketResponse.verifyWebSocketAccept(websocketKey));

	crazy::HttpRouter router;
	CHECK(router.add(crazy::HttpMethod::GET, "/global", routeGlobal));
	CHECK(!router.add(crazy::HttpMethod::GET, "/global", routeGlobal));

	RouteController controller;
	CHECK(router.add(crazy::HttpMethod::GET, "/member",
		&RouteController::hello, controller));
	RouteController* controllerPointer = &controller;
	CHECK(router.add(crazy::HttpMethod::GET, "/pointer",
		&RouteController::hello, controllerPointer));
	auto sharedController = std::make_shared<RouteController>();
	CHECK(router.add(crazy::HttpMethod::GET, "/shared",
		&RouteController::hello, sharedController));
	CHECK(router.add(crazy::HttpMethod::POST, "/bound",
		std::bind(&RouteController::bound, &controller,
			std::placeholders::_1, std::placeholders::_2)));
	CHECK(router.setHttpHandler<crazy::HttpMethod::GET>("/template", routeGlobal));

	crazy::HttpRequest globalRequest("/global");
	globalRequest.setMethod(crazy::HttpMethod::GET);
	crazy::HttpResponse globalResponse;
	CHECK(router.handle(globalRequest, globalResponse));
	CHECK(globalResponse.body() == "global");

	crazy::HttpRequest memberRequest("/member");
	memberRequest.setMethod(crazy::HttpMethod::GET);
	crazy::HttpResponse memberResponse;
	CHECK(router.handle(memberRequest, memberResponse));
	CHECK(memberResponse.body() == "member");

	crazy::HttpRequest pointerRequest("/pointer");
	pointerRequest.setMethod(crazy::HttpMethod::GET);
	crazy::HttpResponse pointerResponse;
	CHECK(router.handle(pointerRequest, pointerResponse));
	CHECK(pointerResponse.body() == "member");

	crazy::HttpRequest sharedRequest("/shared");
	sharedRequest.setMethod(crazy::HttpMethod::GET);
	crazy::HttpResponse sharedResponse;
	CHECK(router.handle(sharedRequest, sharedResponse));
	CHECK(sharedResponse.body() == "member");

	crazy::HttpRequest boundRequest("/bound");
	boundRequest.setMethod(crazy::HttpMethod::POST);
	crazy::HttpResponse boundResponse;
	CHECK(router.handle(boundRequest, boundResponse));
	CHECK(boundResponse.body() == "bound");

	crazy::HttpRequest templateRequest("/template");
	templateRequest.setMethod(crazy::HttpMethod::GET);
	crazy::HttpResponse templateResponse;
	CHECK(router.route(templateRequest, templateResponse));
	CHECK(templateResponse.body() == "global");

	crazy::HttpRequest wrongMethodRequest("/bound");
	wrongMethodRequest.setMethod(crazy::HttpMethod::GET);
	crazy::HttpResponse wrongMethodResponse;
	CHECK(!router.handle(wrongMethodRequest, wrongMethodResponse));

	if (failures == 0) {
		std::cout << "http tests passed" << std::endl;
	}
	return failures == 0 ? 0 : 1;
}

#undef CHECK
