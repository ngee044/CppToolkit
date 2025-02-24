cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME Redis)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
	TLSOptions.h
	RedisClient.h
	RedisConnector.h
)

set(SOURCE_FILES
	TLSOptions.cpp
	RedisClient.cpp
	RedisConnector.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(redis++ CONFIG REQUIRED)
if(WIN32)
    target_link_libraries(${LIBRARY_NAME} PUBLIC redis++::redis++)
else()
    target_link_libraries(${LIBRARY_NAME} PUBLIC redis++::redis++_static)
endif()

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread)