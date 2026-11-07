cmake_minimum_required(VERSION 3.22.1)
project(hypocket_engine LANGUAGES CXX)

add_library(hypocket_engine SHARED
    hypocket_engine.cpp
    jni_bridge.cpp
)

target_compile_options(hypocket_engine PRIVATE -Wall -Wextra -Werror)
target_link_libraries(hypocket_engine PRIVATE log)
