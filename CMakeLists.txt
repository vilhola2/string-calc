cmake_minimum_required(VERSION 3.25)
project(calc)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS OFF)

# Check compiler vendor and version
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if (CMAKE_C_COMPILER_VERSION VERSION_LESS "18.0")
        message(FATAL_ERROR "Clang 18 or newer is required for full C23 support. Found: ${CMAKE_C_COMPILER_VERSION}")
    endif()
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_C_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR "GCC 14 or newer is required for full C23 support. Found: ${CMAKE_C_COMPILER_VERSION}")
    endif()
else()
    message(FATAL_ERROR "Unsupported or unknown compiler: ${CMAKE_C_COMPILER_ID}. Full C23 support required.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
find_package(GMP REQUIRED)
find_package(MPFR REQUIRED)

option(BUILD_EXECUTABLE "Build the executable instead of a library" ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -gdwarf-4")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")
include_directories(${CMAKE_SOURCE_DIR}/src)
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.c")
if(BUILD_EXECUTABLE)
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE GMP::GMP MPFR::MPFR)
else()
    add_library(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE GMP::GMP MPFR::MPFR)
    # Symlink the header into lib/
    add_custom_command(TARGET calc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/src/calc.h
            ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/calc.h
        COMMENT "Symlinking calc.h into lib/"
    )
endif()
