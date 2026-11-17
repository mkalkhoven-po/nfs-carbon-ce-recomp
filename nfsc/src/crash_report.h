// nfsc - a crash dump and a message when the game crashes, so that testers have
// something to report. Nothing is sent anywhere.

#pragma once

#include <filesystem>

namespace nfsc {

// Writes crash dumps to folder (the logs folder). Call before the SDK installs its crash
// reporter (App::OnConfigure).
void InstallCrashReport(std::filesystem::path folder);

// Also reports abort(). Call after the SDK installed its crash reporter (App::OnInit).
void InstallAbortReport();

// Crashes on purpose if nfsc.test_crash says so.
void TestCrash();

}  // namespace nfsc
