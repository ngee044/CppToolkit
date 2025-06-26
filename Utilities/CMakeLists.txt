cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 20)
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

# Build library list
set(UTILITY_LIBS ${CMAKE_DL_LIBS})

# Optional packages - none required for basic build
find_package(lz4 CONFIG QUIET)
if(lz4_FOUND)
    list(APPEND UTILITY_LIBS lz4::lz4)
    target_compile_definitions(${LIBRARY_NAME} PUBLIC -DHAVE_LZ4)
else()
    message(STATUS "lz4 not found, building without compression support")
endif()

find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    list(APPEND UTILITY_LIBS fmt::fmt-header-only)
    target_compile_definitions(${LIBRARY_NAME} PUBLIC -DHAVE_FMT)
else()
    message(STATUS "fmt not found, building without fmt support")
endif()

find_package(Boost QUIET COMPONENTS system filesystem json)
if(Boost_FOUND)
    list(APPEND UTILITY_LIBS Boost::filesystem Boost::system Boost::json)
    target_compile_definitions(${LIBRARY_NAME} PUBLIC -DHAVE_BOOST)
else()
    message(STATUS "Boost not found, building without Boost support")
endif()

find_package(efsw CONFIG QUIET)
if(efsw_FOUND)
    list(APPEND UTILITY_LIBS efsw::efsw)
    target_compile_definitions(${LIBRARY_NAME} PUBLIC -DHAVE_EFSW)
else()
    message(STATUS "efsw not found, building without file watcher support")
endif()

target_link_libraries(${LIBRARY_NAME} PUBLIC ${UTILITY_LIBS})
if(USE_ENCRYPT_LIBS)
	find_package(cryptopp CONFIG QUIET)
	if(cryptopp_FOUND)
		list(APPEND UTILITY_LIBS cryptopp::cryptopp)
		target_compile_definitions(${LIBRARY_NAME} PUBLIC -DUSE_ENCRYPT_MODULE)
	else()
		message(STATUS "cryptopp not found, building without encryption support")
	endif()
endif()
