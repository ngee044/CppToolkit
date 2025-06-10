cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME GameDatabase)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(SOURCE_FILES 
	Database.cpp
)

set (HEADER_FILES
	Database.h
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${LIBRARY_NAME} PUBLIC Network Thread)
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})