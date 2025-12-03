cmake_minimum_required(VERSION 4.0)

project(homeinvasion
	LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(vendor)

add_executable(homeinvasion
	src/main.c
	src/app.c
)
target_include_directories(homeinvasion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(homeinvasion PRIVATE
	SDL3::SDL3
	SDL3_image::SDL3_image
	SDL3_mixer::SDL3_mixer
	SDL3_ttf::SDL3_ttf
	Vulkan::Vulkan
)

find_package(Vulkan)
target_compile_definitions(homeinvasion PRIVATE
	GAMEPATH="${CMAKE_CURRENT_SOURCE_DIR}")


# SHADER COMPILING

function(add_slang_shader_target TARGET)
	cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
	set(SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/shader)
	set(ENTRY_POINTS -entry vert_main -entry frag_main)
	add_custom_command(
		OUTPUT ${SHADER_DIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR})
	add_custom_command(
		OUTPUT ${SHADER_DIR}/slang_compiled.spv
		COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_5 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang_compiled.spv
		WORKING_DIRECTORY ${SHADER_DIR}
		DEPENDS ${SHADER_DIR} ${SHADER_SOURCES}
		COMMENT "Compiling Slang shader"
		VERBATIM
	)
	add_custom_target(${TARGET} DEPENDS ${SHADER_DIR}/slang_compiled.spv)
endfunction()

find_program(SLANGC_EXECUTABLE slangc)

if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc not found!")
endif()

set(SHADER_SLANG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/shader/test1.slang)
add_slang_shader_target(shader SOURCES ${SHADER_SLANG_SOURCES})
add_dependencies(homeinvasion shader)

