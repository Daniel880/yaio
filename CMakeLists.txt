cmake_minimum_required(VERSION 3.10)
project(yaio)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Windows version for compatibility
add_definitions(-DMIN_WIN_VER=0x0501)
add_definitions(-DWINVER=0x0501)
add_definitions(-D_WIN32_WINNT=0x0501)

# Disable specific warnings
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/wd4996)
endif()

# Include directories
include_directories(irsdk_1_19)
include_directories(irsdk_1_19/irsdk_ir2ad)

# Source files for the iRSDK library
set(IRSDK_SOURCES
    irsdk_1_19/irsdk_client.cpp
    irsdk_1_19/irsdk_diskclient.cpp
    irsdk_1_19/yaml_parser.cpp
    irsdk_1_19/irsdk_utils.cpp
    irsdk_1_19/irsdk_ir2ad/serial.cpp
)

# Header files for the iRSDK library
set(IRSDK_HEADERS
    irsdk_1_19/irsdk_client.h
    irsdk_1_19/irsdk_diskclient.h
    irsdk_1_19/yaml_parser.h
    irsdk_1_19/irsdk_defines.h
    irsdk_1_19/irsdk_ir2ad/serial.h
)

# Create executable
add_executable(yaio yaio.cpp ${IRSDK_SOURCES} ${IRSDK_HEADERS})

# Link libraries
if(WIN32)
    target_link_libraries(yaio Winmm)
endif()

# Set output directory
set_target_properties(yaio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# For Visual Studio, organize files in folders
if(MSVC)
    source_group("iRSDK Sources" FILES ${IRSDK_SOURCES})
    source_group("iRSDK Headers" FILES ${IRSDK_HEADERS})
    source_group("Main" FILES yaio.cpp)
endif()