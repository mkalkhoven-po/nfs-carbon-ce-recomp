# nfsc - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(nfsc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(NFSC_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(nfsc WIN32 ${NFSC_SOURCES})
else()
    add_executable(nfsc ${NFSC_SOURCES})
endif()

rexglue_setup_target(nfsc)
