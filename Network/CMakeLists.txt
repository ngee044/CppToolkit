cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME Network)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
	ConnectConditions.h
	ConnectionJob.h
	DataHandler.h
	DataModes.h
	NetworkClient.h
	NetworkConstexpr.h
	NetworkServer.h
	NetworkSession.h
	FileManager.h
	FileSendingJob.h
	ReceivingJob.h
	SendingJob.h
)

set(SOURCE_FILES
	ConnectionJob.cpp
	DataHandler.cpp
	NetworkClient.cpp
	NetworkServer.cpp
	NetworkSession.cpp
	FileManager.cpp
	FileSendingJob.cpp
	ReceivingJob.cpp
	SendingJob.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(${LIBRARY_NAME} PUBLIC Boost::system)

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread)