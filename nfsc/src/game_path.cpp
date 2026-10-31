// nfsc - finding the game data (a disc image or an extracted folder) and checking that
// it is the Collector's Edition this build was recompiled from.

#include "game_path.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system.h>
#include <rex/system/kernel_state.h>
#include <rex/system/user_module.h>
#include <rex/system/xex_module.h>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#endif

REXCVAR_DEFINE_BOOL(skip_edition_check, false, "NFSC",
                    "Start even if default.xex is not the Collector's Edition executable this "
                    "build was recompiled from (it will most likely crash)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace nfsc {

namespace {

// Need for Speed: Carbon Collector's Edition, default.xex version 0.0.0.7
// ("NfsMWCollectorsRelease.pe"). The first page descriptor's digest starts the
// SHA-1 chain over all pages of the image, so it identifies the exact executable.
constexpr uint32_t kTitleId = 0x454107EC;
constexpr uint32_t kVersion = 0x00000007;
constexpr std::array<uint8_t, 20> kFirstPageDigest = {
    0x97, 0x4B, 0x32, 0x29, 0x9E, 0xF1, 0xC5, 0x8D, 0xF9, 0x46,
    0xDA, 0xDA, 0xA0, 0xCD, 0x03, 0x3A, 0x97, 0x2F, 0x91, 0x48,
};

constexpr const char* kRememberedPathFile = "game_path.txt";

bool IsGamePath(const std::filesystem::path& path) {
  std::error_code ec;
  if (std::filesystem::is_regular_file(path, ec)) {
    return true;  // A disc image.
  }
  return std::filesystem::is_regular_file(path / "default.xex", ec);
}

std::filesystem::path ReadRememberedPath(const std::filesystem::path& user_dir) {
  std::ifstream file(user_dir / kRememberedPathFile, std::ios::binary);
  std::string line;
  if (!file || !std::getline(file, line)) {
    return {};
  }
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.pop_back();
  }
  return std::filesystem::path(std::u8string(line.begin(), line.end()));
}

void RememberPath(const std::filesystem::path& user_dir, const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::create_directories(user_dir, ec);
  std::ofstream file(user_dir / kRememberedPathFile, std::ios::binary | std::ios::trunc);
  std::u8string utf8 = path.u8string();
  file.write(reinterpret_cast<const char*>(utf8.data()), std::streamsize(utf8.size()));
  file << '\n';
}

// A "game" folder with default.xex, or the only .iso, next to the executable.
std::filesystem::path FindNextToExecutable(const std::filesystem::path& exe_dir) {
  if (IsGamePath(exe_dir / "game") && std::filesystem::is_directory(exe_dir / "game")) {
    return exe_dir / "game";
  }
  std::vector<std::filesystem::path> isos;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(exe_dir, ec)) {
    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (entry.is_regular_file() && ext == ".iso") {
      isos.push_back(entry.path());
    }
  }
  return isos.size() == 1 ? isos.front() : std::filesystem::path();
}

std::filesystem::path PickGamePath() {
#if defined(_WIN32)
  wchar_t file[4096] = {};
  OPENFILENAMEW ofn = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFilter =
      L"Disc image or default.xex\0*.iso;default.xex\0All files\0*.*\0";
  ofn.lpstrFile = file;
  ofn.nMaxFile = DWORD(std::size(file));
  ofn.lpstrTitle =
      L"Need for Speed: Carbon Collector's Edition (Xbox 360): choose the disc image (.iso) "
      L"or default.xex in the extracted game folder";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&ofn)) {
    return {};
  }
  std::filesystem::path path(file);
  std::wstring name = path.filename().wstring();
  std::transform(name.begin(), name.end(), name.begin(), ::towlower);
  return name == L"default.xex" ? path.parent_path() : path;
#else
  return {};
#endif
}

std::string Hex(const uint8_t* data, size_t size) {
  std::string out;
  for (size_t i = 0; i < size; ++i) {
    out += fmt::format("{:02X}", data[i]);
  }
  return out;
}

}  // namespace

void ResolveGamePath(rex::PathConfig& paths, const std::filesystem::path& exe_dir) {
  if (!paths.game_data_root.empty()) {
    return;
  }
  std::filesystem::path path = ReadRememberedPath(paths.user_data_root);
  if (!path.empty() && !IsGamePath(path)) {
    REXLOG_WARN("nfsc: remembered game path {} is gone", path.string());
    path.clear();
  }
  if (path.empty()) {
    path = FindNextToExecutable(exe_dir);
    if (path.empty()) {
      path = PickGamePath();
    }
    if (!path.empty() && IsGamePath(path)) {
      RememberPath(paths.user_data_root, path);
    }
  }
  if (!path.empty()) {
    REXLOG_INFO("nfsc: game data: {}", path.string());
    paths.game_data_root = path;
  }
}

bool CheckEdition(rex::Runtime& runtime) {
  auto module = runtime.kernel_state()->GetExecutableModule();
  if (!module) {
    return true;
  }
  const rex::runtime::XexModule* xex = module->xex_module();
  const rex::xex2_opt_execution_info* info = xex->opt_execution_info();
  uint32_t title_id = info ? uint32_t(info->title_id) : 0;
  uint32_t version = info ? uint32_t(info->version_value) : 0;
  const auto* security = xex->xex_security_info();
  std::array<uint8_t, 20> digest = {};
  if (security->page_descriptor_count) {
    std::copy_n(reinterpret_cast<const uint8_t*>(security->page_descriptors[0].data_digest),
                digest.size(), digest.begin());
  }
  REXLOG_INFO("nfsc: default.xex title {:08X}, version {:08X}, first page digest {}", title_id,
              version, Hex(digest.data(), digest.size()));
  if (title_id == kTitleId && version == kVersion && digest == kFirstPageDigest) {
    return true;
  }
  if (REXCVAR_GET(skip_edition_check)) {
    REXLOG_WARN("nfsc: default.xex is not the expected one; starting anyway");
    return true;
  }
  std::string message = fmt::format(
      "This build only runs Need for Speed: Carbon Collector's Edition for the Xbox 360 "
      "(title ID 454107EC, version 0.0.0.7).\n\n"
      "The game data you chose has title ID {:08X}, version {}.{}.{}.{}{}.\n\n"
      "Delete game_path.txt in your user data folder to choose a different disc image or "
      "folder at the next start.",
      title_id, (version >> 28) & 0xF, (version >> 24) & 0xF, (version >> 8) & 0xFFFF,
      version & 0xFF,
      title_id == kTitleId && version == kVersion ? " but a different executable" : "");
  REXLOG_ERROR("nfsc: {}", message);
  rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, message);
  return false;
}

}  // namespace nfsc
