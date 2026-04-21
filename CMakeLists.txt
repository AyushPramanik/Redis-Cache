cmake_minimum_required(VERSION 3.20)
project(cacheserver VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_definitions(PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_compile_definitions(PLATFORM_MACOS)
endif()

add_compile_options(
  -Wall -Wextra -Wpedantic -Wshadow
  $<$<CONFIG:Release>:-O3 -march=native>
  $<$<CONFIG:Debug>:-g3 -fsanitize=address,undefined>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)
add_link_options(
  $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

include(FetchContent)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.13.0
)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(spdlog Catch2)

file(GLOB_RECURSE LIB_SOURCES
  "src/server/*.cpp"
  "src/network/*.cpp"
  "src/protocol/*.cpp"
  "src/storage/*.cpp"
  "src/commands/*.cpp"
  "src/concurrency/*.cpp"
  "src/persistence/*.cpp"
  "src/metrics/*.cpp"
)

add_library(cachelib STATIC ${LIB_SOURCES})
target_include_directories(cachelib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cachelib PUBLIC spdlog::spdlog)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(cachelib PUBLIC pthread)
endif()

add_executable(cacheserver src/main.cpp)
target_link_libraries(cacheserver PRIVATE cachelib)

enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
