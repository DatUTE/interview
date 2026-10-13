cmake_minimum_required(VERSION 3.20)

project(browser_network_service_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(network_service_demo
    main.cpp
    network_manager.cpp
    thread_pool.cpp
    tls_http_client.cpp
)

target_include_directories(network_service_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(network_service_demo
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)
