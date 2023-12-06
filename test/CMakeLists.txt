# CMAKELISTS for test project

add_executable(vkmmc_test
	main.cpp
)

target_link_libraries(vkmmc_test PUBLIC vkmmc)
target_include_directories(vkmmc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/code/include")
