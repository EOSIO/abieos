cmake_minimum_required (VERSION 3.11)

FIND_PACKAGE(Boost 1.67 REQUIRED COMPONENTS date_time)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(abieos VERSION 0.1 LANGUAGES CXX)

add_executable(test src/test.cpp)
target_include_directories(test PUBLIC external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(test Boost::date_time)

add_library(abieos MODULE src/abieos.cpp)
target_include_directories(abieos PUBLIC external/rapidjson/include PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(abieos Boost::date_time)
