cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME AWSService)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(HEADER_FILES
    AWSS3Client.h
    AWSSQSPublisher.h
    AWSSQSConsumer.h
    AWSSQSBase.h
)

set(SOURCE_FILES
    AWSS3Client.cpp
    AWSSQSPublisher.cpp
    AWSSQSConsumer.cpp
    AWSSQSBase.cpp
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${HEADER_FILES} ${SOURCE_FILES})

target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${AWSSDK_INCLUDE_DIRS})

find_package(AWSSDK CONFIG REQUIRED COMPONENTS core s3 sqs)

target_link_libraries(${LIBRARY_NAME} PUBLIC Thread ${AWSSDK_LIBRARIES})
