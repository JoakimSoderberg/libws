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

set(LIBWS_DEP_LIST)
set(LIBWS_LIB_LIST)

find_package(LibEvent REQUIRED)

if (LIBWS_WITH_OPENSSL)
	find_package(OpenSSL REQUIRED)

	if (NOT OPENSSL_FOUND)
		error("Cannot link OpenSSL not found!")
	endif()

	list(APPEND LIBWS_LIB_LIST ${OPENSSL_LIBRARIES})
endif(LIBWS_WITH_OPENSSL)

if (MSVC)
	set(ADD_HEADERS_TO_SOURCE_LIST 1)
endif()

# Generate the config header file.
configure_file(
	"libws_config.h.in" 
	"${PROJECT_BINARY_DIR}/libws_config.h"
	)

set(LIBWS_SOURCES 
	libws.c
	libws_private.c
	libws_log.c)

set(LIBWS_HEADERS 
	libws.h
	libws_private.h
	libws_log.h
	libws_compat.h)

# With Visual Studio, the headers can be included in the project.
if (ADD_HEADERS_TO_SOURCE_LIST)
	set(LIBWS_COMPILE_FILES 
		${LIBWS_SOURCES} 
		${LIBWS_HEADERS})
endif()

add_library(libws STATIC ${LIBWS_SOURCES})

# Add libs and dependencies
add_dependencies(libws ${LIBWS_DEP_LIST})
target_link_libraries(libws ${LIBWS_LIB_LIST})

###################################################
###                 Tests                       ###
###################################################

set(LIBWS_TESTS_NAME libws_tests)

# Find all test files with their relative path
# to the current directory.
file(GLOB TEST_SRCS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/test/TEST_*.c")

create_test_sourcelist(
	LIBWS_TESTS_SRCS 			# Name of the test suite.
	${LIBWS_TESTS_NAME}.c		# Name of the test DRIVER source generated.

	# List of test files. All TEST_*.c files.
	${TEST_SRCS}
	)

# Add test dependencies.	
add_dependencies(${LIBWS_TESTS_NAME} ${AGENT_DEP_LIST})
target_link_libraries(${LIBWS_TESTS_NAME} ${AGENT_LIB_LIST})

##
## Add all the tests to CTest.
##
	# Add tests for each test in the test suite.
	foreach (test ${TEST_SRCS})
		# Get the filename without .c extension.
		get_filename_component(TName ${test} NAME_WE)

		# Add a test with the filename we just got
		# And call the driver with the test as an argument.
		add_test(${TName} ${EXECUTABLE_OUTPUT_PATH}/${LIBWS_TESTS_NAME} ${TName})
	endforeach()

# Just make some nice source groups for IDEs.
source_group("Test Files" FILES ${TEST_SRCS})
source_group("Test Driver" FILES "${LIBWS_TESTS_NAME}.c")
