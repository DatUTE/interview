cmake_minimum_required(VERSION 3.20)

project(grpc_hello_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION_RELEASE)
if(NOT GRPC_CPP_PLUGIN)
    get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin IMPORTED_LOCATION)
endif()

if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "Could not resolve gRPC::grpc_cpp_plugin executable path")
endif()

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/greeter.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(GENERATED_SOURCES
    "${GENERATED_DIR}/greeter.pb.cc"
    "${GENERATED_DIR}/greeter.pb.h"
    "${GENERATED_DIR}/greeter.grpc.pb.cc"
    "${GENERATED_DIR}/greeter.grpc.pb.h"
)

add_custom_command(
    OUTPUT ${GENERATED_SOURCES}
    COMMAND protobuf::protoc
    ARGS
        --proto_path "${CMAKE_CURRENT_SOURCE_DIR}"
        --cpp_out "${GENERATED_DIR}"
        --grpc_out "${GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
        "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    VERBATIM
)

add_library(greeter_proto
    "${GENERATED_DIR}/greeter.pb.cc"
    "${GENERATED_DIR}/greeter.grpc.pb.cc"
)

target_include_directories(greeter_proto
    PUBLIC
        "${GENERATED_DIR}"
)

target_link_libraries(greeter_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

add_executable(greeter_server greeter_server.cpp)
target_link_libraries(greeter_server PRIVATE greeter_proto)

add_executable(greeter_client greeter_client.cpp)
target_link_libraries(greeter_client PRIVATE greeter_proto)
