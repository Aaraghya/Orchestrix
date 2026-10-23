cmake_minimum_required(VERSION 3.20)

project(Orchestrix
    VERSION 1.0.0
    DESCRIPTION "A Discrete-Event Simulator for Adaptive Data-Center Resource Allocation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable useful compiler warnings.
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core library: domain models and simulation logic.
add_library(orchestrix_core
    src/Job.cpp
    src/Server.cpp
    src/DataCenter.cpp
)

target_include_directories(orchestrix_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Main CLI executable.
add_executable(orchestrix
    src/main.cpp
)

target_link_libraries(orchestrix
    PRIVATE orchestrix_core
)

# Automated tests will be added here as the project develops.
include(CTest)

if(BUILD_TESTING)
    # Add test targets here when the test files are implemented.
endif()

