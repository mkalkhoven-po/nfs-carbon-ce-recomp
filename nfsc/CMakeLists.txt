# nfsc - Need for Speed: Carbon Collector's Edition, recompiled with the ReXGlue SDK.
#
# The SDK package provides rexglue_project and the rexglue_setup_* functions.

cmake_minimum_required(VERSION 3.25)
project(nfsc VERSION 0.76 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# At -O0 the recompiled game code is far too slow: FMVs decode at ~11 fps instead
# of 30 and drift away from the real-time audio. Optimize it in Debug as well;
# our own code stays unoptimized and debuggable.
set(REXGLUE_RECOMP_OPTIMIZATION_DEBUG "O2" CACHE STRING
    "Optimization of the recompiled code in Debug builds")

if (REXSDK_DIR)
  add_subdirectory("${REXSDK_DIR}" rexglue-sdk)
else ()
  find_package(rexglue 0.11.0 REQUIRED CONFIG)
endif ()
rexglue_project(nfsc OUTPUT_DIR generated/default)

# Host sources: hooks and the app's own code. src/main.cpp stays on the executable.
# OBJECT and never STATIC: an archive drops the hook symbols (see the SDK's hooks page).
add_library(nfsc_common OBJECT
    src/crash_report.cpp
    src/main_menu.cpp
    src/pad.cpp
    src/display.cpp
    src/framerate.cpp
    src/game_path.cpp
    src/profiles.cpp
    src/settings.cpp
    src/settings_window.cpp
    src/widescreen.cpp
)
rexglue_setup_host_library(nfsc_common)
target_compile_definitions(nfsc_common PRIVATE NFSC_VERSION="${PROJECT_VERSION}")

if (WIN32)
  add_executable(nfsc WIN32 src/main.cpp)
else ()
  add_executable(nfsc src/main.cpp)
endif ()

rexglue_setup_target(nfsc)
target_link_libraries(nfsc PRIVATE nfsc_common)
target_compile_definitions(nfsc PRIVATE NFSC_VERSION="${PROJECT_VERSION}")

# File dialog for choosing the disc image or game folder on the first start, crash dumps,
# the Documents folder and monitor information.
if (WIN32)
  target_link_libraries(nfsc PRIVATE comdlg32 dbghelp ole32 shell32 user32)
endif ()
