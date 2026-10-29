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
    src/exit_option.cpp
    src/framerate.cpp
    src/profiles.cpp
    src/widescreen.cpp
)

if(WIN32)
    add_executable(nfsc WIN32 ${NFSC_SOURCES})
else()
    add_executable(nfsc ${NFSC_SOURCES})
endif()

rexglue_setup_target(nfsc GPU_PLUGINS xenos)

# FidelityFX (CAS/FSR presentation effects) is a runtime DLL of the SDK. The SDK
# helper only stages it for in-tree builds (target amd_fidelityfx_vk), not for
# an installed SDK, where the target is rex::amd_fidelityfx_vk.
foreach(_fx rex::amd_fidelityfx_vk rex::amd_fidelityfx_dx12)
    if(TARGET ${_fx})
        add_custom_command(TARGET nfsc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${_fx}> $<TARGET_FILE_DIR:nfsc>
            VERBATIM)
    endif()
endforeach()

# At -O0 the recompiled game code is far too slow: FMVs decode at ~11 fps instead
# of 30 and drift away from the real-time audio. Optimize it in Debug as well;
# the runtime and src/ stay unoptimized and debuggable.
if(TARGET nfsc_recomp)
    target_compile_options(nfsc_recomp PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()
