cmake_minimum_required(VERSION 3.20)
project(PearToPear LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PEAR_BUILD_TESTS "Build tests" OFF)
option(PEAR_DEBUG "Enable debug logs" OFF)

include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)

FetchContent_MakeAvailable(CLI11)

if(PEAR_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )

    FetchContent_MakeAvailable(googletest)
endif()

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(nlohmann_json)

find_package(SQLite3 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc++)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/p2p.proto)
set(PROTO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})

set(PROTO_SRCS ${PROTO_BINARY_DIR}/p2p.pb.cc)
set(PROTO_HDRS ${PROTO_BINARY_DIR}/p2p.pb.h)
set(GRPC_SRCS ${PROTO_BINARY_DIR}/p2p.grpc.pb.cc)
set(GRPC_HDRS ${PROTO_BINARY_DIR}/p2p.grpc.pb.h)

add_custom_command(
    OUTPUT
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        ${GRPC_SRCS}
        ${GRPC_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
        --cpp_out=${PROTO_BINARY_DIR}
        --grpc_out=${PROTO_BINARY_DIR}
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
        ${PROTO_FILE}
    DEPENDS
        ${PROTO_FILE}
    VERBATIM
)

add_library(pear_proto
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(pear_proto PUBLIC
    ${PROTO_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
)

target_link_libraries(pear_proto PUBLIC
    ${Protobuf_LIBRARIES}
    PkgConfig::GRPC
)

add_library(pear_fs
    src/pear/fs/workspace.cpp
    src/pear/fs/hash.cpp
)

target_include_directories(pear_fs PUBLIC
    include
)

target_link_libraries(pear_fs PRIVATE
    OpenSSL::Crypto
)

add_library(pear_db
    src/pear/db/sqlite.cpp
    src/pear/db/schema.cpp
    src/pear/db/sqlite_database.cpp
)

target_include_directories(pear_db PUBLIC
    include
    src
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(pear_db PUBLIC
    ${SQLite3_LIBRARIES}
)

add_library(pear_net
    src/pear/net/node.cpp
    src/pear/net/master_service.cpp
    src/pear/net/remote_client.cpp
    src/pear/net/storage_service.cpp
)

target_include_directories(pear_net PUBLIC
    include
    src
    ${PROTO_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
)

target_link_libraries(pear_net PUBLIC
    pear_db
    pear_fs
    pear_proto
    ${Protobuf_LIBRARIES}
    PkgConfig::GRPC
)

add_library(pear_demon
    src/pear/demon/demon.cpp
)

target_include_directories(pear_demon PUBLIC
    include
    src
)

target_link_libraries(pear_demon PUBLIC
    pear_fs
    pear_db
    pear_net
)

add_executable(pear
    src/main.cpp
    src/pear/cli/commands.cpp
    src/pear/cli/command_utils.cpp
    src/pear/cli/sync.cpp
    src/pear/cli/status.cpp
    src/pear/cli/output.cpp
    src/pear/cli/json_output.cpp
)

target_include_directories(pear PRIVATE
    include
    src
    ${PROTO_BINARY_DIR}
)

target_link_libraries(pear PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    pear_fs
    pear_db
    pear_net
    pear_demon
)

if(PEAR_DEBUG)
    target_compile_definitions(pear PRIVATE PEAR_DEBUG)
endif()
