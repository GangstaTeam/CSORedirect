cmake_minimum_required(VERSION 3.10)

# Set the project name
project(CSORedirect)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add MinHook sources as a static library
file(GLOB MINHOOK_SOURCES "3rdParty/MinHook/src/*.c" "3rdParty/MinHook/src/*.h")
add_library(MinHook STATIC ${MINHOOK_SOURCES})
target_include_directories(MinHook PUBLIC 3rdParty/MinHook/include)

# Add the source files for CSORedirect
file(GLOB SOURCES "dllmain.*" "torque3d/*.cpp" "torque3d/*.h")

# Add the executable and specify it as a shared library
add_library(CSORedirect SHARED ${SOURCES})

# Set the output name to have .asi extension
set_target_properties(CSORedirect PROPERTIES SUFFIX ".asi")

# Include MinHook headers
target_include_directories(CSORedirect PRIVATE 3rdParty/MinHook/include)

# Link MinHook library
target_link_libraries(CSORedirect PRIVATE MinHook)