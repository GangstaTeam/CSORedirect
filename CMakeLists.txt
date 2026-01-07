cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CSORedirect)

# Add the source files
file(GLOB SOURCES "dllmain.*" "torque3d/*.cpp" "torque3d/*.h" "MinHook.h" "3rdParty/MinHook/Release/MinHook.lib")

# Add the executable and specify it as a shared library
add_library(CSORedirect SHARED ${SOURCES})

# Set the output name to have .asi extension
set_target_properties(CSORedirect PROPERTIES SUFFIX ".asi")

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include MinHook headers
include_directories(3rdParty/MinHook/include)