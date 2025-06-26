cmake_minimum_required(VERSION 3.18)

set(PROJECT_NAME GameServerSample)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Windows definitions
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)  # Windows 10
    add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

set(SOURCE_FILES
    main.cpp
    Character.cpp
    GameServer.cpp
)

set(HEADER_FILES
    Character.h
    GameServer.h
)

project(${PROJECT_NAME} VERSION 1.0.0.0)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        GameNetwork
        GameDatabase
        Utilities
        Thread
)

# Include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}  # For module includes
)
