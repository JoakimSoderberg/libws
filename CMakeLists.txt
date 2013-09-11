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
include_directories(${LIBEVENT_INCLUDE_DIR})

set(LIBWS_DEP_LIST LibEvent)
set(LIBWS_LIB_LIST ${LIBEVENT_CORE_LIBRARY})

if (LIBWS_WITH_OPENSSL)
	find_package(OpenSSL REQUIRED)

	if (NOT OPENSSL_FOUND)
		message(FATAL_ERROR "Cannot link OpenSSL not found!")
	endif()

	list(APPEND LIBWS_LIB_LIST ${OPENSSL_LIBRARIES})

	include_directories(${OPENSSL_INCLUDE_DIR})
endif(LIBWS_WITH_OPENSSL)

# Do system introspection.

# TODO: Check for inline keyword.
set(LIBWS_INLINE)

# Generate the config header file.
configure_file(
	"libws_config.h.in" 
	"${PROJECT_BINARY_DIR}/libws_config.h"
	)

include_directories(${PROJECT_BINARY_DIR})

# Group the sources and headers.
set(SRCS 
	libws.c
	libws_private.c
	libws_header.c
	libws_handshake.c
	libws_log.c)

set(HDRS_PUBLIC 
	libws.h
	libws_types.h
	libws_header.h
	libws_handshake.h
	libws_log.h)

set(HDRS_PRIVATE
	libws_private.h
	libws_compat.h)

if (LIBWS_WITH_OPENSSL)
	list(APPEND SRCS libws_openssl.c)
	list(APPEND HDRS_PRIVATE libws_openssl.h)
endif()

source_group("Headers public"	FILES ${HDRS_PUBLIC})
source_group("Headers private"	FILES ${HDRS_PRIVATE})
source_group("Sources"			FILES ${SRCS})

add_library(libws STATIC 
			${SRCS} 
			${HDRS_PUBLIC} 
			${HDRS_PRIVATE})

# Add libs and dependencies
add_dependencies(libws ${LIBWS_DEP_LIST})
target_link_libraries(libws ${LIBWS_LIB_LIST})

add_subdirectory(test)
