cmake_minimum_required(VERSION 3.10)
project(grpc_async)

add_compile_definitions(USING_TEMPLATE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_SOURCE_DIR}/protobuf/grpc)

add_subdirectory(proto)

include_directories(${CMAKE_SOURCE_DIR})

add_executable(
        sample
        sample.cc
        framework/server.cc
        services/samplesvc_impl.cc
        services/anothersamplesvc_impl.cc
        services/examplesvc_impl.cc
)

target_link_libraries(
        sample
        proto_lib
)

add_executable(
        grpc_async
        async.cc
)

target_link_libraries(
        grpc_async
        proto_lib
)

add_executable(
        grpc_sync
        sync.cc
)

target_link_libraries(
        grpc_sync
        proto_lib
)

add_executable(
        grpc_client
        client.cc
)

target_link_libraries(
        grpc_client
        proto_lib
)