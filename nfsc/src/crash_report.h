// nfsc - a crash dump and a message when the game crashes, so that testers have
// something to report. Nothing is sent anywhere.

#pragma once

#include <filesystem>

namespace nfsc {

// Writes crash dumps to folder (the logs folder).
void InstallCrashReport(std::filesystem::path folder);

// Crashes on purpose if the test_crash cvar says so.
void TestCrash();

}  // namespace nfsc
