cmake_minimum_required(VERSION 3.10)

project(rtxON CXX)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "GLFW_BUILD_EXAMPLES" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "GLFW_BUILD_TESTS" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "GLFW_BUILD_DOCS" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "GLFW_INSTALL" FORCE)
add_subdirectory("external/glfw")

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE HEADERS "src/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp")

include_directories(
    "external/glfw/include"
    "external/glm"
    "external/stb"
    "external/tinyobjloader"
    "external/volk"
    "external/vulkan/include"
    "src"
)

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})

add_custom_command(
        TARGET rtxON POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/_data
        $<TARGET_FILE_DIR:rtxON>/_data
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME} glfw)
