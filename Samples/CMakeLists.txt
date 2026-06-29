cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# set the project name
project(${PROJECT_NAME} VERSION 1.0.0.0)

if (BUILD_SAMPLES)
add_subdirectory(FolderWatcherSample)
endif()

if (BUILD_SAMPLES)
	add_subdirectory(FolderCompressionSample)
endif()

# cpp_libraries
if(BUILD_THREAD_LIB AND BUILD_SAMPLES)
	add_subdirectory(ThreadSample)
endif()
if(BUILD_NETWORK_LIB AND BUILD_SAMPLES)
	add_subdirectory(NetworkServerSample)
	add_subdirectory(NetworkClientSample)
endif()
if(BUILD_DATABASE_LIB AND BUILD_SAMPLES)
	add_subdirectory(DatabaseSample)
endif()

if(BUILD_KAFKA_LIB AND BUILD_SAMPLES)
	add_subdirectory(KafkaSample)
endif()