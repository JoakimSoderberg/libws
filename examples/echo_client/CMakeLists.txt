cmake_minimum_required (VERSION 2.6) 

add_executable(echo_client echo_client.c)

target_link_libraries(echo_client ws)
