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

include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)

# Set some nicer output dirs.
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
	# Aim for C89 with C++ style comments.
	set(CMAKE_CXX_FLAGS "-Wimplicit-function-declaration")
	set(CMAKE_C_FLAGS "-std=gnu89 -Wimplicit-function-declaration -Wpointer-sign")

	# Turn on coverage support.
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

include_directories(${PROJECT_SOURCE_DIR}/src)

if (WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
	list(APPEND LIBWS_LIB_LIST Ws2_32.lib iphlpapi.lib User32.lib)
endif()

find_package(Libevent REQUIRED)

include_directories(${LIBEVENT_INCLUDE_DIRS})

list(APPEND LIBWS_DEP_LIST Libevent)
list(APPEND LIBWS_LIB_LIST ${LIBEVENT_LIBRARIES})

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

################################################################################
###                        System introspection                              ###
################################################################################

check_include_files(unistd.h LIBWS_HAVE_UNISTD_H)
check_include_files(sys/time.h LIBWS_HAVE_SYS_TIME_H)
check_include_files(stdint.h LIBWS_HAVE_STDINT_H)
check_include_files(inttypes.h LIBWS_HAVE_INTTYPES_H)
check_include_files(sys/types.h LIBWS_HAVE_SYS_TYPES_H)

# Look for 64-bit integer sizes.
check_type_size(__uint64 LIBWS_HAVE___UINT64)
check_type_size(uint64_t LIBWS_HAVE_UINT64_T)
check_type_size("unsigned long long" LIBWS_HAVE_UNSIGNED_LONG_LONG_INT)
check_type_size("unsigned long" LIBWS_HAVE_UNSIGNED_LONG_INT)
check_type_size("unsigned int" LIBWS_HAVE_UNSIGNED_INT)

if (LIBWS_HAVE_UINT64_T)
	set(LIBWS_UINT64 uint64_t)
elseif (LIBWS_HAVE___UINT64)
	set(LIBWS_UINT64 __uint64)
elseif (LIBWS_HAVE_UNSIGNED_LONG_INT
		AND (${LIBWS_HAVE_UNSIGNED_LONG_LONG_INT} EQUAL 8))
	set(LIBWS_UINT64 "unsigned long long")
elseif (LIBWS_HAVE_UNSIGNED_LONG_INT
		AND (${LIBWS_HAVE_UNSIGNED_LONG_INT} EQUAL 8))
	set(LIBWS_UINT64 "unsigned long int")
else()
	message(FATAL_ERROR "Could not detect a valid unsigned 64-bit integer type")
endif()

# Check our unsigned 32 bit integer sizes.
check_type_size(uint32_t LIBWS_HAVE_UINT32_T)
check_type_size(__uint32 LIBWS_HAVE___UINT32)

if (LIBWS_HAVE_UINT32_T)
	set(LIBWS_UINT32 uint32_t)
elseif (LIBWS_HAVE___UINT32)
	set(LIBWS_UINT32 __uint32)
elseif (LIBWS_HAVE_UNSIGNED_LONG_INT 
		AND (${LIBWS_HAVE_UNSIGNED_LONG_INT} EQUAL 4))
	set(LIBWS_UINT32 "unsigned long")
elseif (LIBWS_HAVE_UNSIGNED_INT AND (${LIBWS_HAVE_UNSIGNED_INT} EQUAL 4))
	set(LIBWS_UINT32 "unsigned int")
else()
	message(FATAL_ERROR "Could not detect a valid unsigned 32-bit integer type")
endif()

if (WIN32) # matches both msvc and cygwin
	set(LIBWS_64FMT "\"I64d\"")
else()
	set(LIBWS_64FMT "\"lld\"")
endif()

# Check for different inline keyword versions.
foreach(KEYWORD "inline" "__inline__" "__inline")
    set(CMAKE_REQUIRED_DEFINITIONS "-DKEYWORD=${KEYWORD}")
    CHECK_C_SOURCE_COMPILES(
        "
        #include <stdio.h>
        KEYWORD void a() {}
        int main(int argc, char **argv) { a(); return 0; }
        " LIBWS_HAVE_${KEYWORD})
endforeach()

if (NOT LIBWS_HAVE_inline)
    if (LIBWS_HAVE___inline__)
        set(LIBWS_INLINE __inline__)
    elseif(LIBWS_HAVE___inline)
        set(LIBWS_INLINE __inline)
    endif()
endif()
set(CMAKE_REQUIRED_DEFINITIONS "")

# Generate the config header file.
configure_file(
	"src/libws_config.h.in"
	"${PROJECT_BINARY_DIR}/include/libws_config.h"
	)

configure_file(
	"src/libws_private_config.h.in"
	"${PROJECT_BINARY_DIR}/libws_private_config.h"
	)

include_directories(${PROJECT_BINARY_DIR})

# Group the sources and headers.
set(SRCS 
	src/libws.c
	src/libws_private.c
	src/libws_header.c
	src/libws_handshake.c
	src/libws_log.c
	src/libws_compat.c
	src/libws_utf8.c)

set(HDRS_PUBLIC 
	src/libws.h
	src/libws_types.h
	src/libws_header.h
	src/libws_log.h)

set(HDRS_PRIVATE
	src/libws_private.h
	src/libws_compat.h
	src/libws_handshake.h
	src/libws_utf8.h
	${PROJECT_BINARY_DIR}/libws_private_config.h)

if (LIBWS_WITH_OPENSSL)
	list(APPEND SRCS src/libws_openssl.c)
	list(APPEND HDRS_PRIVATE src/libws_openssl.h)
else()
	list(APPEND SRCS src/libws_sha1.c)
	list(APPEND HDRS_PRIVATE src/libws_sha1.h)
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

message("=====================================================================")
message("Libevent library: ${LIBEVENT_LIBRARIES}")
message("Libevent include: ${LIBEVENT_INCLUDE_DIRS}")
message("=====================================================================")
