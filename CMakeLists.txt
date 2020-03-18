# copyright defined in abieos/LICENSE.txt

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)

cmake_minimum_required (VERSION 3.11)
project(abieos VERSION 0.1 LANGUAGES CXX C)

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED SKIP_SUBMODULE_CHECK)
  execute_process(COMMAND git submodule status --recursive
                  COMMAND grep -c "^[+\-]"
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE submodule_status
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(submodule_status GREATER 0)
     message(FATAL_ERROR "git submodules are not up to date.
Please run the command 'git submodule update --init --recursive'.")
  endif()
endif()

add_library(abieos MODULE src/abieos.cpp src/abi.cpp src/crypto.cpp include/eosio/fpconv.c)
target_include_directories(abieos PRIVATE include external/outcome/single-header external/rapidjson/include external/date/include)

add_library(abieos_headers INTERFACE)
target_include_directories(abieos_headers INTERFACE include external/outcome/single-header external/date/include external/rapidjson/include)

add_executable(test src/test.cpp src/abieos.cpp src/abi.cpp src/crypto.cpp include/eosio/fpconv.c)
target_include_directories(test PRIVATE include external/outcome/single-header external/rapidjson/include external/date/include)

add_executable(template_test src/template_test.cpp src/abieos.cpp src/abi.cpp src/crypto.cpp include/eosio/fpconv.c)
target_include_directories(template_test PRIVATE include external/outcome/single-header external/rapidjson/include external/date/include)

add_executable(key_test src/key_test.cpp src/abieos.cpp src/abi.cpp src/crypto.cpp include/eosio/fpconv.c)
target_include_directories(key_test PRIVATE include external/outcome/single-header external/rapidjson/include external/date/include)

add_executable(reflect_test src/reflect_test.cpp)
target_include_directories(reflect_test PRIVATE include)

add_executable(test-sanitize src/test.cpp src/abieos.cpp src/abi.cpp src/crypto.cpp include/eosio/fpconv.c)
target_include_directories(test-sanitize PRIVATE include external/outcome/single-header external/rapidjson/include external/date/include)
target_link_libraries(test-sanitize -fno-omit-frame-pointer -fsanitize=address,undefined)
target_compile_options(test-sanitize PUBLIC -fno-omit-frame-pointer -fsanitize=address,undefined)

# add_executable(fuzzer src/fuzzer.cpp src/abieos.cpp)
# target_include_directories(fuzzer PRIVATE external/outcome/single-header external/rapidjson/include external/date/include)
# target_link_libraries(fuzzer -fsanitize=fuzzer,address,undefined,signed-integer-overflow -fstandalone-debug)
# target_compile_options(fuzzer PRIVATE -fsanitize=fuzzer,address,undefined,signed-integer-overflow -fstandalone-debug)

if (CMAKE_CXX_COMPILER_ID MATCHES Clang|AppleClang)
    target_compile_options(abieos PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
    target_compile_options(test PRIVATE -Wall -Wextra -Wno-unused-parameter -fcolor-diagnostics)
endif()

add_subdirectory(tools)
