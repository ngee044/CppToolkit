cmake_minimum_required(VERSION 3.18)

set(PROGRAM_NAME NetworkServerSample)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(SOURCE_FILES NetworkServerSample.cpp)

project(${PROGRAM_NAME} VERSION 1.0.0.0)

add_executable(${PROGRAM_NAME} ${SOURCE_FILES})

target_link_libraries(${PROGRAM_NAME} PUBLIC Network)