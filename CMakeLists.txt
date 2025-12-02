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
