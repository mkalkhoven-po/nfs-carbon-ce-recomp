// nfsc - a crash dump and a message when the game crashes (see crash_report.h).
//
// The dump holds the threads' stacks and the loaded modules, not the guest
// memory (MiniDumpNormal), so it stays small and contains no game data.

#include "crash_report.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <string>
#include <utility>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system.h>

#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#endif

REXCVAR_DEFINE_INT32(test_crash, 0, "NFSC",
                     "Crash on purpose at startup, to test the crash report: 1 = invalid memory "
                     "access, 2 = abort")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace nfsc {

namespace {

std::filesystem::path g_folder;

#if defined(_WIN32)
std::filesystem::path WriteDump(EXCEPTION_POINTERS* exception) {
  SYSTEMTIME now;
  GetLocalTime(&now);
  std::error_code ec;
  std::filesystem::create_directories(g_folder, ec);
  std::filesystem::path path =
      g_folder / fmt::format("nfsc_crash_{:04}{:02}{:02}_{:02}{:02}{:02}.dmp", now.wYear,
                             now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return {};
  }
  MINIDUMP_EXCEPTION_INFORMATION info = {GetCurrentThreadId(), exception, FALSE};
  const BOOL written =
      MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
                        exception ? &info : nullptr, nullptr, nullptr);
  CloseHandle(file);
  return written ? path : std::filesystem::path();
}

void ReportCrash(EXCEPTION_POINTERS* exception) {
  // Several threads can crash at once; the first one reports.
  static std::atomic_flag reporting = ATOMIC_FLAG_INIT;
  if (reporting.test_and_set()) {
    Sleep(INFINITE);
  }
  const std::filesystem::path dump = WriteDump(exception);
  if (exception) {
    REXLOG_ERROR("nfsc: crashed with exception {:08X} at {}, crash dump {}",
                 uint32_t(exception->ExceptionRecord->ExceptionCode),
                 fmt::ptr(exception->ExceptionRecord->ExceptionAddress), dump.string());
  } else {
    REXLOG_ERROR("nfsc: aborted, crash dump {}", dump.string());
  }
  rex::FlushLogging();
  rex::ShowSimpleMessageBox(
      rex::SimpleMessageBoxType::Error,
      fmt::format("Need for Speed: Carbon has crashed.\n\n"
                  "If you want to report this, open an issue on GitHub and attach the newest "
                  ".log and .dmp file from this folder:\n{}\n\n"
                  "Nothing is sent automatically. These files can contain folder names from "
                  "your PC.",
                  g_folder.string()));
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* exception) {
  ReportCrash(exception);
  return EXCEPTION_EXECUTE_HANDLER;
}

// std::terminate and failed runtime checks end up in abort().
void OnAbort(int) {
  ReportCrash(nullptr);
  std::_Exit(3);
}
#endif

}  // namespace

void InstallCrashReport(std::filesystem::path folder) {
  g_folder = std::move(folder.make_preferred());
#if defined(_WIN32)
  SetUnhandledExceptionFilter(OnUnhandledException);
  std::signal(SIGABRT, OnAbort);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}

void TestCrash() {
  if (REXCVAR_GET(test_crash) == 1) {
    *static_cast<volatile int*>(nullptr) = 1;
  } else if (REXCVAR_GET(test_crash) == 2) {
    std::abort();
  }
}

}  // namespace nfsc
