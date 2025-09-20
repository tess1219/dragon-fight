cmake_minimum_required(VERSION 3.10)
project(dragon-fight)
list(APPEND CMAKE_PREFIX_PATH "/usr/local")

# Set default build type if not specified (suppresses raylib warning)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

# Check if local raylib/ exists and build it as subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/raylib/CMakeLists.txt")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)  # Disable raylib tests
    add_subdirectory(raylib)
    set(RAYLIB_FOUND TRUE)
    set(RAYLIB_LIBRARIES raylib)
else()
    set(raylib_DIR "/usr/local/lib/cmake/raylib") # Fallback for installed raylib
    find_package(raylib REQUIRED)
endif()

set(CMAKE_C_STANDARD 99)
set(SOURCES main.c assets.c entities.c level.c)  # Explicitly list project sources
add_executable(dragon-fight ${SOURCES})
target_link_libraries(dragon-fight ${RAYLIB_LIBRARIES} m)
target_compile_options(dragon-fight PRIVATE -Wall -Wextra)

# If using local raylib, include its headers
if(RAYLIB_FOUND)
    target_include_directories(dragon-fight PRIVATE raylib/src)
endif()
