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
    src/profiles.cpp
)

if(WIN32)
    add_executable(nfsc WIN32 ${NFSC_SOURCES})
else()
    add_executable(nfsc ${NFSC_SOURCES})
endif()

rexglue_setup_target(nfsc GPU_PLUGINS xenos)

# At -O0 the recompiled game code is far too slow: FMVs decode at ~11 fps instead
# of 30 and drift away from the real-time audio. Optimize it in Debug as well;
# the runtime and src/ stay unoptimized and debuggable.
if(TARGET nfsc_recomp)
    target_compile_options(nfsc_recomp PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()
