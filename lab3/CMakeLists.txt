cmake_minimum_required(VERSION 3.1.0)

project(set_tests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Werror)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
find_library (LIBRT rt)

add_executable(tests main.cpp)

target_link_libraries(tests PUBLIC pthread rt)
