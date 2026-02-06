cmake_minimum_required(VERSION 3.10)
project(GateKeeperService VERSION 2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure vcpkg integration if checking out a fresh repo
# (User needs to provide CMAKE_TOOLCHAIN_FILE usually)

find_package(CURL REQUIRED)
find_package(jsoncpp REQUIRED CONFIG)

add_executable(GateKeeperService GateKeeperService.cpp)

target_link_libraries(GateKeeperService PRIVATE CURL::libcurl jsoncpp_lib)

# Windows specific libs
if(WIN32)
    target_link_libraries(GateKeeperService PRIVATE Advapi32)
endif()

# Define macro matching the source code
target_compile_definitions(GateKeeperService PRIVATE JSON_DLL)
