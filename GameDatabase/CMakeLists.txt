cmake_minimum_required(VERSION 3.18)

set(LIBRARY_NAME GameDatabase)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(SOURCE_FILES 
	DBConnection.cpp
	DBConnectionPool.cpp
	DBBind.cpp
	DBTransaction.cpp
	DBStoredProcedure.cpp
	DBCache.cpp
	DBMigration.cpp
)

set (HEADER_FILES
	DBConnection.h
	DBConnectionPool.h
	DBBind.h
	DBTransaction.h
	DBStoredProcedure.h
	DBCache.h
	DBMigration.h
)

project(${LIBRARY_NAME} VERSION 1.0.0.0)

add_library(${LIBRARY_NAME} ${SOURCE_FILES} ${HEADER_FILES})

if (WIN32)
	message(STATUS "Building for Windows: SQL ODBC Windows SDK")
	target_link_libraries(${LIBRARY_NAME} PUBLIC odbc32)
	target_compile_definitions(${LIBRARY_NAME} PRIVATE UNICODE _UNICODE)
elseif(APPLE)
	message(STATUS "Building for macOS: Using iODBC")
	# macOS에서는 시스템 iODBC 사용
	find_library(IODBC_LIB iodbc)
	find_library(IODBCINST_LIB iodbcinst)
	if (IODBC_LIB AND IODBCINST_LIB)
		target_link_libraries(${LIBRARY_NAME} PUBLIC ${IODBC_LIB} ${IODBCINST_LIB})
		target_include_directories(${LIBRARY_NAME} PUBLIC /usr/local/include)
	else()
		message(FATAL_ERROR "iODBC not found on macOS")
	endif()
else()
	message(STATUS "Building for Linux: Using unixODBC")
	find_package(unixodbc REQUIRED)
	target_link_libraries(${LIBRARY_NAME} PUBLIC unixODBC::unixODBC)
endif()

# 의존성 라이브러리
target_link_libraries(${LIBRARY_NAME} PUBLIC Utilities ThreadPool)

# 포함 디렉토리
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# C++20 기능 활성화
target_compile_features(${LIBRARY_NAME} PUBLIC cxx_std_20)
