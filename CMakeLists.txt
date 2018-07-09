# copyright defined in abieos/LICENSE.txt

cmake_minimum_required (VERSION 3.11)
project(abieos VERSION 0.1 LANGUAGES CXX)

FIND_PACKAGE(Boost 1.58 REQUIRED COMPONENTS date_time)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(abieos MODULE src/abieos.cpp)
target_include_directories(abieos PUBLIC external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(abieos Boost::date_time)

add_executable(test src/test.cpp src/abieos.cpp)
target_include_directories(test PUBLIC external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(test Boost::date_time)

add_executable(test-sanitize src/test.cpp src/abieos.cpp)
target_include_directories(test-sanitize PUBLIC external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(test-sanitize Boost::date_time -fno-omit-frame-pointer -fsanitize=address,undefined)
target_compile_options(test-sanitize PUBLIC -fno-omit-frame-pointer -fsanitize=address,undefined)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(abieos PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_compile_options(test PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
