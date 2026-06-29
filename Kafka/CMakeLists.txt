cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME Kafka)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
	KafkaConfig.h
	KafkaBase.h
	KafkaQueueConsume.h
	KafkaQueueEmitter.h
	KafkaMessage.h
	DeliveryResult.hpp
	KafkaCommon.h
)

set(SOURCE_FILES
	KafkaConfig.cpp
	KafkaQueueConsume.cpp
	KafkaQueueEmitter.cpp
	KafkaBase.cpp
	KafkaMessage.cpp
)

find_package(unofficial-modern-cpp-kafka CONFIG REQUIRED)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread unofficial::modern-cpp-kafka::modern-cpp-kafka)