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

list(APPEND TEST_DRIVERS ${LIBWS_TESTS_NAME}.c)

# Get the TEST_*.c filenames without extension.
set(TEST_SRCS_NO_EXT)
foreach (test ${TEST_SRCS})
	get_filename_component(TName ${test} NAME_WE)
	list(APPEND TEST_SRCS_NO_EXT ${TName})
endforeach()

# Coverage using gcov (gcc only).
if (LIBWS_COVERAGE)
	if(CMAKE_COMPILER_IS_GNUCXX)
		include(GenerateAllTestsDriver)

		# The normal test driver doesn't have an
		# option to run all tests at once.
		generate_run_all_tests_driver(run_all_tests_driver
			"${TEST_SRCS_NO_EXT}"
			"${TEST_SRCS};libws_test_helpers.c")

		list(APPEND TEST_DRIVERS run_all_tests_driver.c)

		add_dependencies(run_all_tests_driver ${LIBWS_DEP_LIST})
		target_link_libraries(run_all_tests_driver ws ${LIBWS_LIB_LIST} gcov)

		include(CodeCoverage)
		setup_target_for_coverage(
			${LIBWS_TESTS_NAME}_coverage 	# Coverage make target.
			run_all_tests_driver 			# Name of test runner executable.
			coverage 						# Name of output directory.
			)
	endif()
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${LIBWS_TESTS_NAME}
				${LIBWS_TESTS_SRCS}
				"libws_test_helpers.c")

# Add test dependencies.	
add_dependencies(${LIBWS_TESTS_NAME} ${LIBWS_DEP_LIST})
target_link_libraries(${LIBWS_TESTS_NAME} ws ${LIBWS_LIB_LIST})

if(CMAKE_COMPILER_IS_GNUCXX)
    target_link_libraries(${LIBWS_TESTS_NAME} gcov)
endif()

##
## Add all the tests to CTest.
##
	# Add tests for each test in the test suite.
	foreach (test_name ${TEST_SRCS_NO_EXT})
		# Add a test with the filename we just got
		# And call the driver with the test as an argument.
		add_test(${test_name} ${EXECUTABLE_OUTPUT_PATH}/${LIBWS_TESTS_NAME} ${test_name})
	endforeach()

# Just make some nice source groups for IDEs.
source_group("Test Files" FILES ${TEST_SRCS})
source_group("Test Drivers" FILES "${TEST_DRIVERS}")
