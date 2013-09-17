
function(generate_run_all_tests_driver _DRIVER_NAME _DRIVER_SRC_NAME _EXTRA_SOURCE_FILES)

	set(FORWARD_DECLARATIONS "")
	set(FUNCTION_CALLS "")

	# Get the function names by removing the extension of the files.
	# TEST_helloworld.c -> int TEST_helloworld(int argc, char *argv[])
	set(_FUNCTION_NAMES)
	foreach (test ${_EXTRA_SOURCE_FILES})
		get_filename_component(TName ${test} NAME_WE)
		list(APPEND _FUNCTION_NAMES ${TName})
	endforeach()

	foreach (func ${_FUNCTION_NAMES})
		set(FORWARD_DECLARATIONS 
			"${FORWARD_DECLARATIONS}int ${func}(int argc, char *argv[]);\n")

		set(FUNCTION_CALLS 
			"${FUNCTION_CALLS}\t${func}(argc, argv);\n")
	endforeach()

	configure_file("${CMAKE_MODULE_PATH}/run_all_tests_driver.c.in"
				"${_DRIVER_SRC_NAME}")

	set(${_DRIVER_NAME} "${_DRIVER_SRC_NAME};${_EXTRA_SOURCE_FILES}" PARENT_SCOPE)

endfunction() # generate_run_all_tests_driver

