cmake_minimum_required (VERSION 2.6) 

add_executable(autobahntest autobahntest.c)

target_link_libraries(autobahntest ws)
