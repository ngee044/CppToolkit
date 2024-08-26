cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME Thread)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
	Job.h
	JobPool.h
	JobPriorities.h
	ThreadPool.h
	ThreadWorker.h
)

set(SOURCE_FILES
	Job.cpp
	JobPool.cpp
	JobPriorities.cpp
	ThreadPool.cpp
	ThreadWorker.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost REQUIRED COMPONENTS json)
target_link_libraries(${LIBRARY_NAME} PUBLIC Boost::json)

target_link_libraries(${LIBRARY_NAME} PUBLIC Utilities)