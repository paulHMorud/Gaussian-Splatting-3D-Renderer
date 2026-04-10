cmake_minimum_required(VERSION 3.6)
project(glowbox)

find_package(PCL REQUIRED COMPONENTS io common)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

#
# Set python cmd, preferring python 3
#
find_program(PYTHON_3_LOCATION py)
if(NOT "${PYTHON_3_LOCATION}" STREQUAL "PYTHON_3_LOCATION-NOTFOUND")
    execute_process(COMMAND py -3 --version RESULT_VARIABLE has_python_3)
    if("${has_python_3}" STREQUAL "0")
        set(PYTHON_CMD py -3)
    else()
        set(PYTHON_CMD py)
    endif()
else()
    find_program(PYTHON_3_LOCATION python3)
    if(NOT "${PYTHON_3_LOCATION}" STREQUAL "PYTHON_3_LOCATION-NOTFOUND")
        set(PYTHON_CMD python3)
    else()
        set(PYTHON_CMD python)
    endif()
endif()
message("python used: ${PYTHON_CMD}")

#
# CMake setup
#
set(CMAKE_CXX_STANDARD 14)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_VERBOSE_MAKEFILE 0)
set(CMAKE_SUPPRESS_REGENERATION TRUE)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O3")
    if(NOT WIN32)
        set(GLAD_LIBRARIES dl)
    endif()
endif()

#
# GLFW
#
option(GLFW_INSTALL OFF)
option(GLFW_BUILD_DOCS OFF)
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
add_subdirectory(lib/glfw)

#
# SFML (audio only)
#
option(SFML_BUILD_GRAPHICS OFF)
option(SFML_BUILD_WINDOW OFF)
option(SFML_BUILD_NETWORK OFF)
add_subdirectory(lib/SFML)

#
# FMT
#
add_subdirectory(lib/fmt)

#
# GLAD (generate if missing)
#
if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lib/glad/include/glad/glad.h)
    message("Generating glad library files, using ${PYTHON_CMD}")
    execute_process(
        COMMAND ${PYTHON_CMD} -m glad --profile core --out-path . --generator c --spec gl
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/glad
    )
    message("Finished generating glad library files")
endif()

#
# ImGui
#
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui)

add_library(imgui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw)

#
# Sources
#
file(GLOB VENDORS_SOURCES
    lib/glad/src/glad.c
    lib/lodepng/lodepng.cpp
)

file(GLOB_RECURSE PROJECT_HEADERS
    src/*.hpp
    src/*.h
)

file(GLOB_RECURSE PROJECT_SOURCES
    src/*.cpp
    src/*.cxx
    src/*.cc
    src/*.c
)

file(GLOB_RECURSE PROJECT_SHADERS
    res/shaders/*.comp
    res/shaders/*.frag
    res/shaders/*.geom
    res/shaders/*.vert
)

file(GLOB PROJECT_CONFIGS
    CMakeLists.txt
    README.rst
    .gitignore
    .gitmodules
)

#
# Executable
#
add_definitions(
    -DGLFW_INCLUDE_NONE
    -DPROJECT_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"
)

add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${PROJECT_SHADERS}
    ${PROJECT_CONFIGS}
    ${VENDORS_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/lodepng
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/arrrgh
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/SFML/include
    ${PCL_INCLUDE_DIRS}
)

#
# Link libraries
#
target_link_libraries(${PROJECT_NAME}
    imgui
    glfw
    sfml-audio
    fmt::fmt
    ${GLFW_LIBRARIES}
    ${GLAD_LIBRARIES}
    ${PCL_LIBRARIES}
)

add_definitions(${PCL_DEFINITIONS})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT glowbox)
