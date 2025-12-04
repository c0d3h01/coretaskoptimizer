cmake_minimum_required(VERSION 3.18.1)
project(coretaskoptimizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Android-specific settings
if(ANDROID)
    set(ANDROID_STL c++_shared)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Flags
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,--strip-all")
endif()

# === Define executable FIRST ===
add_executable(task_optimizer src/main.cpp)

# === Threads (portable) ===
find_package(Threads REQUIRED)
target_link_libraries(task_optimizer PRIVATE Threads::Threads)

# === Android-specific strip step ===
if(ANDROID AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET task_optimizer POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:task_optimizer>
        COMMENT "Stripping binary for release"
    )
endif()

# Output directory
set_target_properties(task_optimizer PROPERTIES
    OUTPUT_NAME "task_optimizer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
