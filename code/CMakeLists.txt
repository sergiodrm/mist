# CMAKELISTS for code

add_library(vkmmc)

file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false
		RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.c?? src/*.h)

file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false
		RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} include/vkmmc/*.h)

function(MakeProjectDirs src_files)
	string(REPLACE " " ";" src_array "${src_files}")
	foreach(FILE ${src_array})
		get_filename_component(PARENT_DIR "${FILE}" PATH)
		set(GROUP "${PARENT_DIR}")
		string(REPLACE "/" "\\" GROUP "${GROUP}")
		source_group("${GROUP}" FILES "${FILE}")
		message("-- Processing file: ${FILE} -> ${GROUP}")
	endforeach()
endfunction()

string(REPLACE ";" " " FILES_ARRAY "${SRC_FILES}")
MakeProjectDirs(${FILES_ARRAY})
string(REPLACE ";" " " FILES_ARRAY "${HEADER_FILES}")
MakeProjectDirs(${FILES_ARRAY})

target_sources(vkmmc
	PRIVATE
	${SRC_FILES}
	${HEADER_FILES}
)

set_property(TARGET vkmmc PROPERTY CXX_STANDARD 23)
target_compile_definitions(vkmmc PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(vkmmc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vkmmc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/vkmmc")
target_include_directories(vkmmc PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(vkmmc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty")

target_link_libraries(vkmmc PUBLIC
	vma
	glm
	Vulkan::Vulkan
	stb_image
	imgui
	SDL2::SDL2
	vkbootstrap
)

#add_custom_command(
#	TARGET vkmmc POST_BUILD
#	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:vkmmc> $<TARGET_FILE_DIR:vkmmc>
#	COMMAND_EXPAND_LISTS
#)

