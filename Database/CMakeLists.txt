cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME Database)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
    Database.h
    PostgresDB.h)

set(SOURCE_FILES
    PostgresDB.cpp)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PostgreSQL REQUIRED)
target_link_libraries(${LIBRARY_NAME} PUBLIC PostgreSQL::PostgreSQL)

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread)