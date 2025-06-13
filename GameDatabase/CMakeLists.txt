cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME GameDatabase)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(SOURCE_FILES 
	DBConnection.cpp
	DBConnectionPool.cpp
	DBBind.cpp
)

set (HEADER_FILES
	DBConnection.h
	DBConnectionPool.h
	DBBind.h
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${SOURCE_FILES} ${HEADER_FILES})

if (WIN32)
	message(STATUS "Building for Windows: SQL ODBC Windows SDK")
else()
	find_package(unixodbc REQUIRED)
	find_package(nanodbc CONFIG REQUIRED)

    target_link_libraries(myapp PRIVATE UNIX::odbc)
	target_link_libraries(myapp PRIVATE nanodbc)
endif()

target_link_libraries(${LIBRARY_NAME} PUBLIC Network Thread)
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
