cmake_minimum_required(VERSION 3.15)
project(crazy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src)
include (cmake/utils.cmake)


set(CRAZY_LIB_FILE
	src/crazy/address.cc
	src/crazy/config.cc
	src/crazy/coroutine.cc
	src/crazy/hook.cc
	src/crazy/logger.cc
	src/crazy/message.cc
	src/crazy/processer.cc
	src/crazy/util.cc
	src/crazy/selector_epoll.cc
	src/crazy/scheduler.cc
	src/crazy/socket.cc
	src/crazy/tcp_server.cc
	src/crazy/thread.cc
	src/crazy/timer.cc
	src/crazy/util.cc
	src/crazy/http/request.cc
	src/crazy/http/response.cc
	src/crazy/util/hash_util.cc
	src/crazy/util/crypto_util.cc
	)

ragelmaker(src/crazy/http/http11_parser.rl CRAZY_LIB_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/crazy/http)
ragelmaker(src/crazy/http/httpclient_parser.rl CRAZY_LIB_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/crazy/http)
ragelmaker(src/crazy/uri.rl CRAZY_LIB_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/crazy)

add_compile_options(-std=c++17 -Wall -w -g)

add_library(crazy STATIC ${CRAZY_LIB_FILE})

link_libraries(crazy pthread dl ssl)

option(EXCLUDE_RAGEL "Exclude ragel files from build" OFF)


add_executable(test_ascii_logo tests/test_ascii_logo.cc)
add_executable(test_config tests/test_config.cc)
add_executable(test_coroutine tests/test_coroutine.cc)
add_executable(test_lexical_cast tests/test_lexical_cast.cc)
add_executable(test_assert tests/test_assert.cc)
add_executable(test_logger tests/test_logger.cc)
add_executable(test_thread tests/test_thread.cc)
add_executable(test_mutex tests/test_mutex.cc)
add_executable(test_byte_array tests/test_byte_array.cc)
add_executable(test_toml11 tests/test_toml11.cc)
add_executable(test_format tests/test_format.cc)
add_executable(test_processer tests/test_processer.cc)
add_executable(test_scheduler tests/test_scheduler.cc)
add_executable(test_timer tests/test_timer.cc)
add_executable(test_uri tests/test_uri.cc)
add_executable(test_socket tests/test_socket.cc)
add_executable(test_tcp_server tests/test_tcp_server.cc)
add_executable(test_hook tests/test_hook.cc)
add_executable(test_json tests/test_json.cc)

