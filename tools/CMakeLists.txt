# copyright defined in abieos/LICENSE.txt

add_executable(name name.cpp)
target_link_libraries(name abieos_headers)

add_custom_command( TARGET name POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:name> ${CMAKE_CURRENT_BINARY_DIR}/name2num )
add_custom_command( TARGET name POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:name> ${CMAKE_CURRENT_BINARY_DIR}/num2name )
