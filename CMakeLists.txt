cmake_minimum_required(VERSION 3.15)

include(utility.cmake)

project(crazy LANGUAGES C CXX)

option(CRAZY_BUILD_TESTS "Build crazy test executables" ON)
option(CRAZY_BUILD_E2E_TESTS "Build network e2e tests" ON)
option(CRAZY_BUILD_DB_TESTS "Build database tests" ON)

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

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/crazy>
        $<BUILD_INTERFACE:${CLICKHOUSE_ROOT_DIR}/include>
)

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

if(MYSQL_LINK_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LINK_LIBS})
endif()

if(CLICKHOUSE_LINK_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CLICKHOUSE_LINK_LIBS})
endif()

if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

file(COPY ${CMAKE_SOURCE_DIR}/tests/app.ini DESTINATION ${CMAKE_BINARY_DIR}/config/)

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
    add_crazy_test(test_http_application)
    add_crazy_test(test_websocket)
    add_crazy_test(test_websocket_server)
    add_crazy_test(test_http_client)
    add_crazy_test(test_websocket_client)
    add_crazy_test(test_smtp_client)
    add_crazy_test(test_mvcc_lock_wrapper)
    add_crazy_test(test_localsocket)
    add_crazy_test(test_json)
    add_crazy_test(test_lock)
    add_crazy_test(test_encryption)
    add_crazy_test(test_date_time)
    add_crazy_test(test_key_value_pair)
    add_crazy_test(test_mmap)

    if(CRAZY_BUILD_E2E_TESTS)
        add_crazy_test(test_http_client_e2e)
        add_crazy_test(test_websocket_client_e2e)
    endif()

    if(CRAZY_BUILD_DB_TESTS)
        add_crazy_test(test_mysql)
        add_crazy_test(test_clickhouse)
    endif()
endif()
