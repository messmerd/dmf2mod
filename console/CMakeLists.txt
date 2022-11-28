cmake_minimum_required(VERSION 3.14)

project(dmf2mod_console)

add_executable(${PROJECT_NAME} console.cpp)
target_link_libraries(${PROJECT_NAME} dmf2mod)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME dmf2mod)
