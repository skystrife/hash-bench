cmake_minimum_required(VERSION 3.2.0)
project(hash-bench)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_subdirectory(deps/meta meta-libs EXCLUDE_FROM_ALL)

add_executable(bench bench.cpp)
target_link_libraries(bench meta-util cpptoml)
