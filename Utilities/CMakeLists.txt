cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(LIBRARY_NAME Utilities)
project(${LIBRARY_NAME} VERSION 1.0.0.0)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(HEADER_FILES
	ArgumentParser.h
	Combiner.h
	Compressor.h
	Converter.h
	FolderWatcher.h
	Folder.h
	File.h
	JsonTool.h
	Log.h
	Logger.h
	LogTypes.h
	SystemInformation.h
	Generator.h)

set(SOURCE_FILES
	ArgumentParser.cpp
	Combiner.cpp
	Compressor.cpp
	Converter.cpp
	FolderWatcher.cpp
	Folder.cpp
	File.cpp
	JsonTool.cpp
	Log.cpp
	Logger.cpp
	SystemInformation.cpp
	Generator.cpp
)

if(USE_ENCRYPT_LIBS)
	list(APPEND HEADER_FILES Encryptor.h)
	list(APPEND SOURCE_FILES Encryptor.cpp)
endif()

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem json)
find_package(efsw CONFIG REQUIRED)

target_link_libraries(${LIBRARY_NAME} PUBLIC lz4::lz4 efsw::efsw fmt::fmt-header-only ${CMAKE_DL_LIBS} Boost::filesystem Boost::system Boost::json)
if(USE_ENCRYPT_LIBS)
	find_package(cryptopp CONFIG REQUIRED)
	target_link_libraries(${LIBRARY_NAME} PUBLIC cryptopp::cryptopp)
	target_compile_definitions(${LIBRARY_NAME} PUBLIC -DUSE_ENCRYPT_MODULE)
endif()
