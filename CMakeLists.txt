cmake_minimum_required(VERSION 3.15)

include(utility.cmake)

project(crazy LANGUAGES C CXX)

option(CRAZY_BUILD_TESTS "Build crazy test executables" ON)
option(CRAZY_BUILD_E2E_TESTS "Build network e2e tests" ON)
option(CRAZY_BUILD_DB_TESTS "Build database tests" ON)
option(CRAZY_ENABLE_TLS "Enable TLS/HTTPS transport (requires OpenSSL)" ON)
option(CRAZY_BUILD_NETWORK_TESTS "Build tests that require real public network access" ON)

set(CRAZY_OPENSSL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/openssl" CACHE PATH "OpenSSL root (include/ + lib/)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG /utf-8")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG /utf-8")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /O2 /Zi /DNDEBUG /utf-8")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ggdb -g -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -ggdb -g -DNDEBUG")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB_RECURSE SOURCES
    "src/crazy/*.cpp"
    "src/crazy/encryption/*.cpp"
    "src/crazy/net/*.cpp"
    "src/crazy/net/smtp/*.cpp"
    "src/crazy/net/http/*.c"
    "src/crazy/net/http/*.cpp"
)

set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/src/third_party")
set(MYSQL_ROOT_DIR "${THIRD_PARTY_DIR}/MySQL")
set(CLICKHOUSE_ROOT_DIR "${THIRD_PARTY_DIR}/clickhouse")
set(GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}/crazy")

set(GIT_COMMIT "")
set(GIT_BRANCH "")
git_info(GIT_COMMIT GIT_BRANCH)

message(STATUS "GIT_COMMIT: ${GIT_COMMIT}")
message(STATUS "GIT_BRANCH: ${GIT_BRANCH}")

configure_file(
    ${CMAKE_SOURCE_DIR}/src/crazy/version.h.in
    ${GENERATED_INCLUDE_DIR}/crazy/version.h
    @ONLY
)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH "x64")
    else()
        set(ARCH "x86")
    endif()

    set(MYSQL_LIB_DEBUG_FILE "${MYSQL_ROOT_DIR}/lib/windows/${ARCH}/debug/mysqlclient.lib")
    set(MYSQL_LIB_RELEASE_FILE "${MYSQL_ROOT_DIR}/lib/windows/${ARCH}/release/mysqlclient.lib")

    set(CLICKHOUSE_LIB_DEBUG_DIR "${CLICKHOUSE_ROOT_DIR}/lib/windows/${ARCH}/debug")
    set(CLICKHOUSE_LIB_RELEASE_DIR "${CLICKHOUSE_ROOT_DIR}/lib/windows/${ARCH}/release")
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MYSQL_LIB_PATH "${MYSQL_ROOT_DIR}/lib/linux/x64")
        set(CLICKHOUSE_LIB_DIR "${CLICKHOUSE_ROOT_DIR}/lib/linux/x64/release")
    else()
        set(MYSQL_LIB_PATH "${MYSQL_ROOT_DIR}/lib/linux/x86")
        set(CLICKHOUSE_LIB_DIR "${CLICKHOUSE_ROOT_DIR}/lib/linux/x86/release")
    endif()

    set(MYSQL_LIB_FILE "${MYSQL_LIB_PATH}/libmysqlclient.a")
endif()

set(MYSQL_LINK_LIBS "")
if(WIN32)
    if(EXISTS ${MYSQL_LIB_DEBUG_FILE} AND EXISTS ${MYSQL_LIB_RELEASE_FILE})
        list(APPEND MYSQL_LINK_LIBS
            debug ${MYSQL_LIB_DEBUG_FILE}
            optimized ${MYSQL_LIB_RELEASE_FILE}
        )
        message(STATUS "MySQL Debug: ${MYSQL_LIB_DEBUG_FILE}")
        message(STATUS "MySQL Release: ${MYSQL_LIB_RELEASE_FILE}")
    else()
        message(WARNING "MySQL client library not found")
    endif()
elseif(EXISTS ${MYSQL_LIB_FILE})
    list(APPEND MYSQL_LINK_LIBS ${MYSQL_LIB_FILE})
    message(STATUS "MySQL: ${MYSQL_LIB_FILE}")
else()
    message(WARNING "MySQL client library not found: ${MYSQL_LIB_FILE}")
endif()

set(CLICKHOUSE_LINK_LIBS "")
if(WIN32)
    set(CLICKHOUSE_LIB_NAMES
        clickhouse.lib
        absl_int128.lib
        cityhash.lib
        lz4.lib
        zstdstatic.lib
    )
    foreach(CH_LIB_NAME ${CLICKHOUSE_LIB_NAMES})
        set(CH_DEBUG_LIB "${CLICKHOUSE_LIB_DEBUG_DIR}/${CH_LIB_NAME}")
        set(CH_RELEASE_LIB "${CLICKHOUSE_LIB_RELEASE_DIR}/${CH_LIB_NAME}")
        if(EXISTS ${CH_DEBUG_LIB} AND EXISTS ${CH_RELEASE_LIB})
            list(APPEND CLICKHOUSE_LINK_LIBS
                debug ${CH_DEBUG_LIB}
                optimized ${CH_RELEASE_LIB}
            )
            message(STATUS "ClickHouse Debug: ${CH_DEBUG_LIB}")
            message(STATUS "ClickHouse Release: ${CH_RELEASE_LIB}")
        else()
            message(WARNING "ClickHouse library not found: ${CH_LIB_NAME}")
        endif()
    endforeach()
else()
    set(CLICKHOUSE_LIBS
        "${CLICKHOUSE_LIB_DIR}/libclickhouse.a"
        "${CLICKHOUSE_LIB_DIR}/libabsl_int128.a"
        "${CLICKHOUSE_LIB_DIR}/libcityhash.a"
        "${CLICKHOUSE_LIB_DIR}/liblz4.a"
        "${CLICKHOUSE_LIB_DIR}/libzstdstatic.a"
    )
    foreach(CH_LIB ${CLICKHOUSE_LIBS})
        if(EXISTS ${CH_LIB})
            list(APPEND CLICKHOUSE_LINK_LIBS ${CH_LIB})
            message(STATUS "ClickHouse: ${CH_LIB}")
        else()
            message(WARNING "ClickHouse: ${CH_LIB}")
        endif()
    endforeach()
endif()

set(CRAZY_TLS_LINK_LIBS "")
if(CRAZY_ENABLE_TLS)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_ARCH "x64")
    else()
        set(OPENSSL_ARCH "x86")
    endif()

    set(OPENSSL_INCLUDE_DIR "${CRAZY_OPENSSL_ROOT}/include")
    set(OPENSSL_SSL_HEADER "${OPENSSL_INCLUDE_DIR}/openssl/ssl.h")
    set(OPENSSL_CRYPTO_HEADER "${OPENSSL_INCLUDE_DIR}/openssl/crypto.h")

    if(WIN32)
        set(OPENSSL_LIB_ROOT "${CRAZY_OPENSSL_ROOT}/lib/windows/${OPENSSL_ARCH}")
        set(OPENSSL_SSL_LIB_NAME "ssl.lib")
        set(OPENSSL_CRYPTO_LIB_NAME "crypto.lib")
    else()
        set(OPENSSL_LIB_ROOT "${CRAZY_OPENSSL_ROOT}/lib/linux/${OPENSSL_ARCH}")
        set(OPENSSL_SSL_LIB_NAME "libssl.a")
        set(OPENSSL_CRYPTO_LIB_NAME "libcrypto.a")
    endif()

    if(NOT EXISTS "${OPENSSL_SSL_HEADER}")
        message(FATAL_ERROR
            "CRAZY_ENABLE_TLS=ON but the OpenSSL header is missing: ${OPENSSL_SSL_HEADER}\n"
            "  CRAZY_OPENSSL_ROOT = ${CRAZY_OPENSSL_ROOT}\n"
            "  Point it at an OpenSSL package containing include/ and lib/: -DCRAZY_OPENSSL_ROOT=<path>")
    endif()

    if(NOT EXISTS "${OPENSSL_CRYPTO_HEADER}")
        message(FATAL_ERROR
            "CRAZY_ENABLE_TLS=ON but the OpenSSL header is missing: ${OPENSSL_CRYPTO_HEADER}\n"
            "  CRAZY_OPENSSL_ROOT = ${CRAZY_OPENSSL_ROOT}\n"
            "  Point it at an OpenSSL package containing include/ and lib/: -DCRAZY_OPENSSL_ROOT=<path>")
    endif()

    if(NOT IS_DIRECTORY "${OPENSSL_LIB_ROOT}")
        message(FATAL_ERROR
            "CRAZY_ENABLE_TLS=ON but the OpenSSL library directory is missing: ${OPENSSL_LIB_ROOT}\n"
            "  CRAZY_OPENSSL_ROOT = ${CRAZY_OPENSSL_ROOT}\n"
            "  Point it at an OpenSSL package containing include/ and lib/: -DCRAZY_OPENSSL_ROOT=<path>")
    endif()

    if(CMAKE_CONFIGURATION_TYPES)
        set(OPENSSL_CHECK_CONFIGS "debug" "release")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(OPENSSL_CHECK_CONFIGS "debug")
    else()
        set(OPENSSL_CHECK_CONFIGS "release")
    endif()

    foreach(OPENSSL_CHECK_CONFIG ${OPENSSL_CHECK_CONFIGS})
        foreach(OPENSSL_LIB_NAME ${OPENSSL_SSL_LIB_NAME} ${OPENSSL_CRYPTO_LIB_NAME})
            set(OPENSSL_LIB_FILE "${OPENSSL_LIB_ROOT}/${OPENSSL_CHECK_CONFIG}/${OPENSSL_LIB_NAME}")
            if(NOT EXISTS "${OPENSSL_LIB_FILE}")
                message(FATAL_ERROR
                    "CRAZY_ENABLE_TLS=ON but the OpenSSL library is missing: ${OPENSSL_LIB_FILE}\n"
                    "  CRAZY_OPENSSL_ROOT = ${CRAZY_OPENSSL_ROOT}\n"
                    "  Point it at an OpenSSL package containing include/ and lib/: -DCRAZY_OPENSSL_ROOT=<path>")
            endif()
        endforeach()
    endforeach()

    set(OPENSSL_SSL_DEBUG_FILE "${OPENSSL_LIB_ROOT}/debug/${OPENSSL_SSL_LIB_NAME}")
    set(OPENSSL_SSL_RELEASE_FILE "${OPENSSL_LIB_ROOT}/release/${OPENSSL_SSL_LIB_NAME}")
    set(OPENSSL_CRYPTO_DEBUG_FILE "${OPENSSL_LIB_ROOT}/debug/${OPENSSL_CRYPTO_LIB_NAME}")
    set(OPENSSL_CRYPTO_RELEASE_FILE "${OPENSSL_LIB_ROOT}/release/${OPENSSL_CRYPTO_LIB_NAME}")

    list(APPEND CRAZY_TLS_LINK_LIBS
        "$<IF:$<CONFIG:Debug>,${OPENSSL_SSL_DEBUG_FILE},${OPENSSL_SSL_RELEASE_FILE}>"
        "$<IF:$<CONFIG:Debug>,${OPENSSL_CRYPTO_DEBUG_FILE},${OPENSSL_CRYPTO_RELEASE_FILE}>"
    )

    if(WIN32)
        list(APPEND CRAZY_TLS_LINK_LIBS
            ws2_32
            crypt32
            advapi32
            user32
            bcrypt
            gdi32
        )
    endif()

    message(STATUS "CRAZY_ENABLE_TLS=ON: TLS/HTTPS transport is compiled in (default ON; disable with -DCRAZY_ENABLE_TLS=OFF)")
    message(STATUS "OpenSSL root: ${CRAZY_OPENSSL_ROOT}")
    message(STATUS "OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL Debug: ${OPENSSL_SSL_DEBUG_FILE} ${OPENSSL_CRYPTO_DEBUG_FILE}")
    message(STATUS "OpenSSL Release: ${OPENSSL_SSL_RELEASE_FILE} ${OPENSSL_CRYPTO_RELEASE_FILE}")
endif()

# ---------------------------------------------------------------------------
# 随框架提供的 CA bundle：configure 期由 generate_ca_bundle.cmake 把 PEM 生成到
# src/crazy/net/ca_bundle.cpp（源码树内、随仓库提交），再作为普通源文件编译进库。
# 来源：src/third_party/ca/cacert.pem（https://curl.se/ca/cacert.pem，见 THIRD_PARTY_NOTICES.md）。
# ---------------------------------------------------------------------------
set(CRAZY_CA_BUNDLE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/ca/cacert.pem"
    CACHE FILEPATH "Bundled CA certificate bundle (PEM) compiled into the library")
set(CRAZY_CA_BUNDLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/crazy/net/ca_bundle.cpp")
# 日志里的诊断标签：显式指定 > 仓库相对路径 > 只留文件名（不把构建机绝对路径编进二进制）
set(CRAZY_CA_BUNDLE_LABEL "" CACHE STRING
    "Diagnostic label for the bundled CA bundle (log only; defaults to the repo-relative PEM path)")
if(NOT CRAZY_CA_BUNDLE_LABEL)
    file(RELATIVE_PATH CRAZY_CA_BUNDLE_RELATIVE "${CMAKE_SOURCE_DIR}" "${CRAZY_CA_BUNDLE_FILE}")
    if(CRAZY_CA_BUNDLE_RELATIVE MATCHES "^\\.\\.")
        get_filename_component(CRAZY_CA_BUNDLE_LABEL "${CRAZY_CA_BUNDLE_FILE}" NAME)
    else()
        set(CRAZY_CA_BUNDLE_LABEL "${CRAZY_CA_BUNDLE_RELATIVE}")
    endif()
endif()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CRAZY_CA_BUNDLE_FILE}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DCA_BUNDLE_PEM=${CRAZY_CA_BUNDLE_FILE}"
        "-DCA_BUNDLE_SOURCE=${CRAZY_CA_BUNDLE_SOURCE}"
        "-DCA_BUNDLE_LABEL=${CRAZY_CA_BUNDLE_LABEL}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_ca_bundle.cmake"
    RESULT_VARIABLE CRAZY_CA_BUNDLE_RESULT)
if(NOT CRAZY_CA_BUNDLE_RESULT EQUAL 0)
    message(FATAL_ERROR "CA bundle 生成失败（退出码 ${CRAZY_CA_BUNDLE_RESULT}）：${CRAZY_CA_BUNDLE_SOURCE}")
endif()
if(NOT EXISTS "${CRAZY_CA_BUNDLE_SOURCE}")
    message(FATAL_ERROR "CA bundle 源文件缺失，无法编译进库：${CRAZY_CA_BUNDLE_SOURCE}")
endif()

# 显式列入源文件（不依赖前面 file(GLOB) 的时机），先去重再追加
list(REMOVE_ITEM SOURCES "${CRAZY_CA_BUNDLE_SOURCE}")
list(APPEND SOURCES "${CRAZY_CA_BUNDLE_SOURCE}")

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/crazy>
        $<BUILD_INTERFACE:${CLICKHOUSE_ROOT_DIR}/include>
)

if(CRAZY_TLS_LINK_LIBS)
    target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${OPENSSL_INCLUDE_DIR}>)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CRAZY_TLS_ENABLED)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(MYSQL_LINK_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LINK_LIBS})
endif()

if(CLICKHOUSE_LINK_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CLICKHOUSE_LINK_LIBS})
endif()

if(CRAZY_TLS_LINK_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CRAZY_TLS_LINK_LIBS})
    message(STATUS "TLS enabled: ${PROJECT_NAME} links ${CRAZY_TLS_LINK_LIBS}")
endif()

if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/tests/app.ini DESTINATION ${CMAKE_BINARY_DIR}/config/)
file(COPY ${CMAKE_SOURCE_DIR}/tests/http_static DESTINATION ${CMAKE_BINARY_DIR}/)
file(COPY ${CMAKE_SOURCE_DIR}/tests/tls DESTINATION ${CMAKE_BINARY_DIR}/)

function(add_crazy_test test_name)
    add_executable(${test_name} tests/${test_name}.cc)
    target_link_libraries(${test_name} PRIVATE ${PROJECT_NAME})
    add_test(NAME ${test_name} COMMAND ${test_name})

    if(MYSQL_LINK_LIBS)
        target_link_libraries(${test_name} PRIVATE ${MYSQL_LINK_LIBS})
    endif()

    if(CLICKHOUSE_LINK_LIBS)
        target_link_libraries(${test_name} PRIVATE ${CLICKHOUSE_LINK_LIBS})
    endif()

    if(CRAZY_TLS_LINK_LIBS)
        target_link_libraries(${test_name} PRIVATE ${CRAZY_TLS_LINK_LIBS})
    endif()

    if(UNIX)
        target_link_libraries(${test_name} PRIVATE dl pthread)
    endif()
endfunction()

if(CRAZY_BUILD_TESTS)
    enable_testing()

    add_crazy_test(test_actor)
    add_crazy_test(test_actor_client)
    add_crazy_test(test_config)
    add_crazy_test(test_logger)
    add_crazy_test(test_protocol)
    add_crazy_test(test_http)
    add_crazy_test(test_http_session)
    add_crazy_test(test_http_application)
    add_crazy_test(test_socket)
    add_crazy_test(test_socket_ipv6)
    add_crazy_test(test_websocket)
    add_crazy_test(test_websocket_server)
    add_crazy_test(test_http_client)
    add_crazy_test(test_websocket_client)
    add_crazy_test(test_http_client_async)
    add_crazy_test(test_websocket_client_async)
    add_crazy_test(test_smtp_client)
    add_crazy_test(test_mvcc_lock_wrapper)
    add_crazy_test(test_localsocket)
    add_crazy_test(test_json)
    add_crazy_test(test_lock)
    add_crazy_test(test_encryption)
    add_crazy_test(test_date_time)
    add_crazy_test(test_key_value_pair)
    add_crazy_test(test_mmap)
    add_crazy_test(test_buffer)
    add_crazy_test(test_decoder)
    add_crazy_test(test_encoder)


    # -----------------------------------------------------------------------
    # 端到端 / TLS / 公网 / 数据库用例：与上面完全一致，全部平铺注册。
    # 不再用 CRAZY_BUILD_E2E_TESTS / CRAZY_ENABLE_TLS / CRAZY_BUILD_NETWORK_TESTS /
    # CRAZY_BUILD_DB_TESTS 包裹注册 —— 所有用例一律生成。
    # -----------------------------------------------------------------------
    add_crazy_test(test_http_client_e2e)
    add_crazy_test(test_http_keep_alive_e2e)
    add_crazy_test(test_http_chunked)
    add_crazy_test(test_http_chunked_evi)
    add_crazy_test(test_http_chunked_upload)
    add_crazy_test(test_http_range)
    add_crazy_test(test_websocket_client_e2e)

    add_crazy_test(test_https_client)
    add_crazy_test(test_https_tls_server)
    add_crazy_test(test_https_tls_client)
    add_crazy_test(test_https_tls_config)

    add_crazy_test(test_https_baidu)

    add_crazy_test(test_mysql)
    add_crazy_test(test_clickhouse)

    # 需要 TLS 客户端/服务端实现的用例：-DCRAZY_ENABLE_TLS=OFF 时以 77 跳过（不是失败）。
    set_tests_properties(
        test_https_client
        test_https_tls_server
        test_https_tls_client
        test_https_tls_config
        PROPERTIES SKIP_RETURN_CODE 77)

    # 真实公网用例：离线 / DNS 失败时同样以 77 跳过。
    set_tests_properties(test_https_baidu PROPERTIES SKIP_RETURN_CODE 77)
    target_compile_definitions(test_https_baidu PRIVATE
        CRAZY_BAIDU_OUTPUT_FILE="${CMAKE_BINARY_DIR}/baidu-response.html")

    set(CRAZY_TLS_TEST_TARGETS
        test_https_client
        test_https_tls_server
        test_https_tls_client
        test_https_tls_config
    )
    foreach(TLS_TEST_TARGET ${CRAZY_TLS_TEST_TARGETS})
        target_compile_definitions(${TLS_TEST_TARGET} PRIVATE
            CRAZY_TLS_TEST_DIR="${CMAKE_SOURCE_DIR}/tests/tls")
    endforeach()
endif()
