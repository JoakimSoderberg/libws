cmake_minimum_required (VERSION 2.6) 

project(libws)

set(PROJECT_MAJOR_VERSION 0)
set(PROJECT_MINOR_VERSION 1)
set(PROJECT_PATCH_VERSION 0)

option(LIBWS_WITH_OPENSSL "Compile with OpenSSL support" ON)
option(LIBWS_WITH_LOG "Compile with logging support" ON)

set(PROJECT_VERSION ${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION})
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)

find_package(LibEvent REQUIRED)

if (LIBWS_WITH_SSL)
	find_package(OpenSSL)

	if (NOT OPENSSL_FOUND)
		message("Disabling SSL")
		set(LIBWS_DISABLE_SSL ON)
		set(OPENSSL_CRYPTO_LIBRARY "")
		set(OPENSSL_INCLUDE_DIR "")
		set(OPENSSL_LIBRARIES "")
		set(LIBEVENT_OPENSSL_LIBRARY "")
	endif()
endif(LIBWS_WITH_SSL)

if (MSVC)
	set(ADD_HEADERS_TO_SOURCE_LIST 1)
endif()

set(LIBWS_SOURCES libws.c)
set(LIBWS_HEADERS libws.h)

# With Visual Studio, the headers can be included in the project.
if (ADD_HEADERS_TO_SOURCE_LIST)
	set(LIBWS_COMPILE_FILES 
		${LIBWS_SOURCES} 
		${LIBWS_HEADERS})
endif()

add_library(libws STATIC ${LIBWS_SOURCES})
