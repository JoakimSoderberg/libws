###################################################
###                 Tests                       ###
###################################################

set(LIBWS_TESTS_NAME libws_tests)

# Find all test files with their relative path
# to the current directory.
file(GLOB TEST_SRCS
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/TEST_*.c")

create_test_sourcelist(
	LIBWS_TESTS_SRCS			# Name of the test suite.
	${LIBWS_TESTS_NAME}.c		# Name of the test DRIVER source generated.

	# List of test files. All TEST_*.c files.
	${TEST_SRCS}
	)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${LIBWS_TESTS_NAME}
				${LIBWS_TESTS_SRCS}
				"libws_test_helpers.c")

# Add test dependencies.	
add_dependencies(${LIBWS_TESTS_NAME} ${LIBWS_DEP_LIST})
target_link_libraries(${LIBWS_TESTS_NAME} libws ${LIBWS_LIB_LIST})

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