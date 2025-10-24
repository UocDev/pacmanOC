cmake_minimum_required(VERSION 3.10)
project(pacmanOC VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include src)

find_package(CURL REQUIRED)

add_executable(pacmanoc
    src/main.cpp
    src/core/manager.cpp
    src/core/utils.cpp
    src/core/db.cpp
    src/core/tree.cpp
)

target_link_libraries(pacmanoc PRIVATE CURL::libcurl)

install(TARGETS pacmanoc DESTINATION /usr/bin)
