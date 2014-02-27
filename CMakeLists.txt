cmake_minimum_required (VERSION 2.6) 

project(libws)

set(PROJECT_MAJOR_VERSION 0)
set(PROJECT_MINOR_VERSION 1)
set(PROJECT_PATCH_VERSION 0)

option(LIBWS_WITH_TESTS "Build test suite" ON)
option(LIBWS_WITH_OPENSSL "Compile with OpenSSL support" ON)
option(LIBWS_WITH_LOG "Compile with logging support" ON)
option(LIBWS_WITH_MEMCHECK "Run valgrind on tests" ON)
option(LIBWS_WITH_EXAMPLES "Compile with example programs" ON)
option(LIBWS_WITH_AUTOBAHN "Compile the Autobahn test suite client. This requires extra dependencies." OFF)

set(PROJECT_VERSION ${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION})
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)

# Set some nicer output dirs.
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

# Aim for C89 with C++ style comments.
set(CMAKE_CXX_FLAGS "-Wimplicit-function-declaration")
set(CMAKE_C_FLAGS "-std=gnu89 -Wimplicit-function-declaration -Wpointer-sign")

# Turn on coverage support.
if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
	option(LIBWS_COVERAGE "Compile with coverage flags.")
	option(LIBWS_COVERAGE_REPORT "(GCC Only! Requires gcov/lcov to be installed). Include target for doing coverage analysis for the test suite. Note that -DCMAKE_BUILD_TYPE=Debug must be set" OFF)

	if (LIBWS_COVERAGE_REPORT)
		set(LIBWS_COVERAGE ON)
	endif()
endif()

if (LIBWS_COVERAGE)
	if(NOT CMAKE_COMPILER_IS_GNUCXX)
		if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
			message(FATAL_ERROR "Compiler is not GNU gcc! Aborting... You can set this on the command line using CC=/usr/bin/gcc CXX=/usr/bin/g++ cmake <options> ..")
		endif()
	endif()

	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	  MESSAGE(FATAL_ERROR "Code coverage results with an optimised (non-Debug) build may be misleading! Add -DCMAKE_BUILD_TYPE=Debug")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
endif()

include_directories(${PROJECT_SOURCE_DIR})

find_package(LibEvent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIR})

list(APPEND LIBWS_DEP_LIST LibEvent)
list(APPEND LIBWS_LIB_LIST ${LIBEVENT_LIBRARY})

if (LIBWS_WITH_OPENSSL)
	find_package(OpenSSL REQUIRED)

	if (NOT OPENSSL_FOUND)
		message(FATAL_ERROR "Cannot link OpenSSL not found!")
	endif()

	list(APPEND LIBWS_LIB_LIST 
		${OPENSSL_LIBRARIES}
		${LIBEVENT_OPENSSL_LIBRARY})

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
	libws_log.c
	libws_compat.c)

set(HDRS_PUBLIC 
	libws.h
	libws_types.h
	libws_header.h
	libws_log.h)

set(HDRS_PRIVATE
	libws_private.h
	libws_compat.h
	libws_handshake.h)

if (LIBWS_WITH_OPENSSL)
	list(APPEND SRCS libws_openssl.c)
	list(APPEND HDRS_PRIVATE libws_openssl.h)
else()
	list(APPEND SRCS libws_sha1.c)
	list(APPEND HDRS_PRIVATE libws_sha1.h)
endif()

source_group("Headers public"	FILES ${HDRS_PUBLIC})
source_group("Headers private"	FILES ${HDRS_PRIVATE})
source_group("Sources"			FILES ${SRCS})

# TODO: Support shared as well.
add_library(ws STATIC 
			${SRCS} 
			${HDRS_PUBLIC} 
			${HDRS_PRIVATE})

# Add libs and dependencies
add_dependencies(ws ${LIBWS_DEP_LIST})
target_link_libraries(ws ${LIBWS_LIB_LIST})

if (LIBWS_WITH_EXAMPLES)
	add_executable(echo_client examples/echo_client/echo_client.c)
	target_link_libraries(echo_client ws)
endif()

if (LIBWS_WITH_AUTOBAHN)
	if (NOT EXISTS "${PROJECT_SOURCE_DIR}/test/autobahn/cargo/cargo.c")
		message("####################################################")
		message(" Please do 'git submodule update --init --recursive'")
		message("####################################################")
		message(FATAL_ERROR "Missing submodules!")
	endif()

	set(JANSSON_WITHOUT_TESTS ON CACHE BOOL "")
	set(JANSSON_BUILD_DOCS OFF CACHE BOOL "")
	add_subdirectory(test/autobahn/jansson)
	include_directories(${JANSSON_INCLUDE_DIRS})

	# Uncomment this to get cargo debug info.
	#add_definitions(-DCARGO_DEBUG=5)
	add_executable(autobahntest
		test/autobahn/autobahntest.c
		test/autobahn/cargo/cargo.c
		test/libws_test_helpers.c)

	include_directories(test)

	target_link_libraries(autobahntest ws ${JANSSON_LIBRARIES})
endif()

if (LIBWS_WITH_TESTS)
	ENABLE_TESTING()
	add_subdirectory(test)
endif()

# TODO: Create CMake config.
# TODO: Export libraries and headers.
# TODO: Install stuff


