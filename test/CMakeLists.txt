# CMAKELISTS for test project


add_executable(vkmmc_test)

file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false
		RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c??)

SETUP_GROUPS(${SRC_FILES})

target_sources(vkmmc_test
	PRIVATE
		${SRC_FILES}
)

target_link_libraries(vkmmc_test PUBLIC vkmmc)
target_include_directories(vkmmc_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/code/include")