cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME RabbitMQ)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
	SSLOptions.h
	RabbitMQBase.h
	ConsumeInformation.h
	ConsumeInformationContainer.h
	RabbitMQWorkQueueConsume.h
	RabbitMQWorkQueueEmitter.h
)

set(SOURCE_FILES
	SSLOptions.cpp
	RabbitMQBase.cpp
	ConsumeInformation.cpp
	ConsumeInformationContainer.cpp
	RabbitMQWorkQueueConsume.cpp
	RabbitMQWorkQueueEmitter.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(rabbitmq-c CONFIG REQUIRED)
if(WIN32)
    target_link_libraries(${LIBRARY_NAME} PUBLIC rabbitmq::rabbitmq)
else()
    target_link_libraries(${LIBRARY_NAME} PUBLIC rabbitmq::rabbitmq-static)
endif()

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread)