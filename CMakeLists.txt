cmake_minimum_required(VERSION 3.15)

include(utility.cmake)

project(crazy LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -g3 -D_DEBUG /utf-8")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ggdb -g -DNDEBUG /utf-8")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2 -ggdb -g -DNDEBUG /utf-8")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -g3 -D_DEBUG")
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
)

set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/src/third_party")
set(MYSQL_ROOT_DIR "${THIRD_PARTY_DIR}/MySQL")

set(GIT_COMMIT "")
set(GIT_BRANCH "")
git_info(GIT_COMMIT GIT_BRANCH)

message(STATUS "GIT_COMMIT: ${GIT_COMMIT}")
message(STATUS "GIT_BRANCH: ${GIT_BRANCH}")

configure_file(
    ${CMAKE_SOURCE_DIR}/src/crazy/version.h.in
    ${CMAKE_SOURCE_DIR}/src/crazy/version.h
    @ONLY
)

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH "x64")
    else()
        set(ARCH "x86")
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(MYSQL_LIB_PATH "${MYSQL_ROOT_DIR}/lib/windows/${ARCH}/debug")
    else()
        set(MYSQL_LIB_PATH "${MYSQL_ROOT_DIR}/lib/windows/${ARCH}/release")
    endif()
    set(MYSQL_LIB_FILE "${MYSQL_LIB_PATH}/mysqlclient.lib")
    
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(MYSQL_LIB_PATH "${MYSQL_ROOT_DIR}/lib/linux/x64")
    else()
        set(MYSQL_LIB_PATH "${MYSQL_ROOT_DIR}/lib/linux/x86")
    endif()
    set(MYSQL_LIB_FILE "${MYSQL_LIB_PATH}/libmysqlclient.a")
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/crazy>
)

# 基础依赖
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# MySQL 直接链接到主库
if(EXISTS ${MYSQL_LIB_FILE})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${MYSQL_LIB_FILE})
    message(STATUS "链接 MySQL 客户端库: ${MYSQL_LIB_FILE}")
else()
    message(WARNING "未找到 MySQL 客户端库: ${MYSQL_LIB_FILE}")
endif()

# 系统库依赖 - 在 MySQL 之后链接
if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif()

# 测试程序
file(COPY ${CMAKE_SOURCE_DIR}/tests/app.ini DESTINATION ${CMAKE_BINARY_DIR}/config/)
function(add_crazy_test test_name)
    add_executable(${test_name} tests/${test_name}.cc)
    target_link_libraries(${test_name} PRIVATE ${PROJECT_NAME})
    
    # 测试程序也需要 MySQL 和系统库（按正确顺序）
    if(EXISTS ${MYSQL_LIB_FILE})
        target_link_libraries(${test_name} PRIVATE ${MYSQL_LIB_FILE})
    endif()
    
    if(UNIX)
        target_link_libraries(${test_name} PRIVATE dl pthread)
    endif()
endfunction()

add_crazy_test(test_actor)
add_crazy_test(test_actor_client)
add_crazy_test(test_config)
add_crazy_test(test_logger)
add_crazy_test(test_protocol)
add_crazy_test(test_mysql)
add_crazy_test(test_mvvc_lock_wapper)
add_crazy_test(test_localsocket)
add_crazy_test(test_json)
add_crazy_test(test_lock)
add_crazy_test(test_encryption)
add_crazy_test(test_date_time)
add_crazy_test(test_key_value_pair)
add_crazy_test(test_mmap)
