cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME GameNetworkSession)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(SOURCE_FILES
	NetworkSession.cpp
)

set (HEADER_FILES
	NetworkSession.h
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(${LIBRARY_NAME} PUBLIC Utilities)
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})