#cmake_minimum_required (VERSION 2.6) 


function(generate_run_all_tests_driver _DRIVER_NAME _FUNCTION_NAMES _EXTRA_SOURCE_FILES)
	set(FORWARD_DECLARATIONS "")
	set(FUNCTION_CALLS "")

	message("${FORWARD_DECLARATIONS}")

	foreach (func ${_FUNCTION_NAMES})
		set(FORWARD_DECLARATIONS 
			"${FORWARD_DECLARATIONS}\nint ${func}(int argc, char *[]);\n")

		set(FUNCTION_CALLS 
			"${FUNCTION_CALLS}\nret |= ${func}(argc, argv);\n")
	endforeach()

	configure_file("${CMAKE_MODULE_PATH}/run_all_tests_driver.c.in"
		"${CMAKE_BINARY_DIR}/run_all_tests_driver.c")
	
	add_executable(${_DRIVER_NAME} "${CMAKE_BINARY_DIR}/run_all_tests_driver.c"
				 	${_EXTRA_SOURCE_FILES})

endfunction() # generate_run_all_tests_driver

