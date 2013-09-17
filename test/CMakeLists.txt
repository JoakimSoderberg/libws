###################################################
###                 Tests                       ###
###################################################

set(LIBWS_TESTS_NAME libws_tests)
set(LIBWS_TESTS_ALL_NAME "${LIBWS_TESTS_NAME}_all")

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
	if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		include(GenerateAllTestsDriver)

		# The normal test driver doesn't have an option to run all tests at once
		# When checking test coverage we want that...
		generate_run_all_tests_driver(RUN_ALL_TESTS_SRCS 	# Name of driver (list of the sources).
			${LIBWS_TESTS_ALL_NAME}.c						# Name of the generated driver source.
			"${TEST_SRCS}"									# Test sources to include in the driver.
			)

		list(APPEND TEST_DRIVERS ${LIBWS_TESTS_ALL_NAME}.c)

		add_executable(${LIBWS_TESTS_ALL_NAME} 
					${RUN_ALL_TESTS_SRCS} 
					libws_test_helpers.c)

		add_dependencies(${LIBWS_TESTS_ALL_NAME}  ${LIBWS_DEP_LIST})
		target_link_libraries(${LIBWS_TESTS_ALL_NAME} ws ${LIBWS_LIB_LIST})

		include(CodeCoverage)
		setup_target_for_coverage(
			${LIBWS_TESTS_NAME}_coverage 	# Coverage make target.
			${LIBWS_TESTS_ALL_NAME} 		# Name of test runner executable.
			coverage 						# Name of output directory.
			)
	endif()
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${LIBWS_TESTS_NAME}
				${LIBWS_TESTS_SRCS}
				libws_test_helpers.c)

# Add test dependencies.	
add_dependencies(${LIBWS_TESTS_NAME} ${LIBWS_DEP_LIST})
target_link_libraries(${LIBWS_TESTS_NAME} ws ${LIBWS_LIB_LIST})

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
source_group("Test Drivers" FILES ${TEST_DRIVERS})
