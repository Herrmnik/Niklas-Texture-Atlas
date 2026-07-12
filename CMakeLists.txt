cmake_minimum_required(VERSION 3.20)
project(TextureAtlasProject LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Put executables in <source>/bin/<Config> (e.g. bin/Debug, bin/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/Release)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

find_package(OpenGL REQUIRED)

add_subdirectory(external/glfw-3.4)

add_library(glad STATIC
    external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    external/glad/include
)

add_executable(TextureAtlasProject
    src/main.cpp
    src/TextureAtlas.cpp
)

target_include_directories(TextureAtlasProject PRIVATE
    include
    src
    external/glad/include
)

target_link_libraries(TextureAtlasProject PRIVATE
    glfw
    glad
    OpenGL::GL
)

if (MSVC)
    target_compile_definitions(TextureAtlasProject PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_custom_command(TARGET TextureAtlasProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:TextureAtlasProject>/shaders
)

add_custom_command(TARGET TextureAtlasProject POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:TextureAtlasProject>/assets
)