// nfsc - finding the game data (a disc image or an extracted folder) and checking that
// it is the Collector's Edition this build was recompiled from.

#include "game_path.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include <rex/filesystem/file.h>
#include <rex/filesystem/file_open.h>
#include <rex/filesystem/file_system.h>
#include <rex/logging/log_category.h>
#include <rex/settings_registry.h>

#include "settings.h"

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

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

// Where the shader cache goes by default. Not in Documents, which is often
// synced by OneDrive.
std::filesystem::path LocalCacheFolder() {
#if defined(_WIN32)
  if (const wchar_t* local_app_data = _wgetenv(L"LOCALAPPDATA")) {
    return std::filesystem::path(local_app_data) / "nfsc" / "cache";
  }
#endif
  return {};
}

// Moves the cache of earlier versions, so that it doesn't have to build up again.
void MoveCache(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code ec;
  if (!std::filesystem::is_directory(from, ec) || std::filesystem::exists(to, ec)) {
    return;
  }
  std::filesystem::create_directories(to.parent_path(), ec);
  std::filesystem::rename(from, to, ec);
  if (ec) {  // Another drive.
    std::filesystem::copy(from, to, std::filesystem::copy_options::recursive, ec);
    if (!ec) {
      std::filesystem::remove_all(from, ec);
    }
  }
}

// The first portable start takes the savegames and shader cache along. The
// originals stay where they are.
void CopyToPortable(const std::filesystem::path& user_dir, const std::filesystem::path& cache_dir,
                    const std::filesystem::path& portable_dir) {
  const auto options = std::filesystem::copy_options::recursive;
  std::error_code ec;
  std::filesystem::create_directories(portable_dir, ec);
  std::filesystem::copy(user_dir, portable_dir, options, ec);
  if (!cache_dir.empty() && std::filesystem::is_directory(cache_dir, ec)) {
    std::filesystem::copy(cache_dir, portable_dir / "cache", options, ec);
  }
}

// The player's Documents folder, where SDK v0.10 kept the user data.
std::filesystem::path DocumentsFolder() {
#if defined(_WIN32)
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path))) {
    std::filesystem::path documents(path);
    CoTaskMemFree(path);
    return documents;
  }
#endif
  return std::filesystem::current_path();
}

struct XexIdentity {
  bool read = false;
  uint32_t title_id = 0;
  uint32_t version = 0;
  std::array<uint8_t, 20> first_page_digest = {};
};

uint32_t LoadBE32(const std::vector<uint8_t>& d, size_t offset) {
  if (offset + 4 > d.size())
    return 0;
  return uint32_t(d[offset]) << 24 | uint32_t(d[offset + 1]) << 16 | uint32_t(d[offset + 2]) << 8 |
         d[offset + 3];
}

// The title ID and version from the execution info header (0x00040006), and the digest of
// the first page descriptor in the security info (see tools/xexinfo.py).
XexIdentity ReadXexIdentity(rex::filesystem::IFileSystem& file_system) {
  XexIdentity id;
  auto opened = file_system.Open("game:\\default.xex", rex::filesystem::OpenOptions{});
  if (!opened || !opened->file) {
    return id;
  }
  std::vector<uint8_t> header(64 * 1024);
  auto read = opened->file->Read(std::span<rex::u8>(header.data(), header.size()), 0);
  header.resize(read ? *read : 0);
  if (header.size() < 0x18 || LoadBE32(header, 0) != 0x58455832) {  // "XEX2"
    return id;
  }
  const uint32_t security = LoadBE32(header, 0x10);
  const uint32_t count = LoadBE32(header, 0x14);
  for (uint32_t i = 0; i < count && i < 64; i++) {
    if (LoadBE32(header, 0x18 + i * 8) == 0x00040006) {
      const uint32_t info = LoadBE32(header, 0x18 + i * 8 + 4);
      id.version = LoadBE32(header, info + 4);
      id.title_id = LoadBE32(header, info + 12);
    }
  }
  const size_t digest = size_t(security) + 0x184 + 4;
  if (LoadBE32(header, security + 0x180) == 0 || digest + 20 > header.size()) {
    return id;
  }
  std::copy_n(header.begin() + std::ptrdiff_t(digest), 20, id.first_page_digest.begin());
  id.read = true;
  return id;
}

// SDK v0.10 caches (v0.752 and older) can't be used by SDK v0.11: it doesn't find their shaders
// again and logs an error for each stored pipeline at every start. Clear them once; the cache
// builds up again while playing.
void ClearOldShaderCache(const std::filesystem::path& cache_dir) {
  if (cache_dir.empty()) {
    return;
  }
  std::error_code ec;
  const std::filesystem::path marker = cache_dir / "rexglue-sdk-0.11";
  if (std::filesystem::exists(marker, ec)) {
    return;
  }
  if (std::filesystem::exists(cache_dir / "shaders", ec)) {
    std::filesystem::remove_all(cache_dir / "shaders", ec);
    REXLOG_INFO("nfsc: cleared the shader cache of an older version in {}", cache_dir.string());
  }
  std::filesystem::create_directories(cache_dir, ec);
  std::ofstream(marker) << "The shader cache in this folder is for ReXGlue SDK 0.11.\n";
}

std::string Hex(const uint8_t* data, size_t size) {
  std::string out;
  for (size_t i = 0; i < size; ++i) {
    out += std::format("{:02X}", data[i]);
  }
  return out;
}

}  // namespace

void ConfigureDataPaths(const std::filesystem::path& exe_dir) {
  std::error_code ec;
  // SDK v0.10 kept savegames in Documents\nfsc; v0.11 would default to a "user" folder
  // next to the executable. Keep them where they are.
  const std::filesystem::path default_user_dir = DocumentsFolder() / "nfsc";
  const std::filesystem::path local_cache_dir = LocalCacheFolder();
  const bool user_chosen = ChosenByPlayer("app.user_root");
  const bool cache_chosen = ChosenByPlayer("app.cache_root");
  const bool portable = Settings().portable.Get();

  std::filesystem::path user_dir = user_chosen ? std::filesystem::path(SettingText("app.user_root"))
                                               : default_user_dir;
  if (!user_chosen && portable) {
    user_dir = exe_dir / "userdata";
    if (!std::filesystem::exists(user_dir, ec)) {
      CopyToPortable(default_user_dir, local_cache_dir, user_dir);
    }
  }

  std::filesystem::path cache_dir;
  if (!cache_chosen) {
    if (portable || user_chosen || local_cache_dir.empty()) {
      cache_dir = user_dir / "cache";
    } else {
      MoveCache(default_user_dir / "cache", local_cache_dir);
      cache_dir = local_cache_dir;
    }
  }

  ClearOldShaderCache(cache_chosen ? std::filesystem::path(SettingText("app.cache_root"))
                                   : cache_dir);

  auto& registry = REX_SETTINGS();
  if (!user_chosen) {
    (void)registry.SetText(rex::SettingLayer::kTitle, "app.user_root", user_dir.generic_string());
  }
  if (!cache_chosen) {
    (void)registry.SetText(rex::SettingLayer::kTitle, "app.cache_root",
                           cache_dir.generic_string());
  }
  REXLOG_INFO("nfsc: user data {}, cache {}", SettingText("app.user_root"),
              SettingText("app.cache_root"));
}

bool ResolveGamePath(const std::filesystem::path& exe_dir) {
  if (ChosenByPlayer("title.game_root")) {
    return true;
  }
  const std::filesystem::path user_dir = SettingText("app.user_root");
  std::filesystem::path path = ReadRememberedPath(user_dir);
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
      RememberPath(user_dir, path);
    }
  }
  if (path.empty()) {
    return false;
  }
  REXLOG_INFO("nfsc: game data: {}", path.string());
  (void)REX_SETTINGS().SetText(rex::SettingLayer::kTitle, "title.game_root",
                               path.generic_string());
  return true;
}

std::string CheckEdition(rex::filesystem::IFileSystem& file_system) {
  const XexIdentity xex = ReadXexIdentity(file_system);
  if (!xex.read) {
    REXLOG_WARN("nfsc: could not read the header of default.xex");
    return {};
  }
  REXLOG_INFO("nfsc: default.xex title {:08X}, version {:08X}, first page digest {}",
              xex.title_id, xex.version, Hex(xex.first_page_digest.data(), 20));
  if (xex.title_id == kTitleId && xex.version == kVersion &&
      xex.first_page_digest == kFirstPageDigest) {
    return {};
  }
  if (Settings().skip_edition_check.Get()) {
    REXLOG_WARN("nfsc: default.xex is not the expected one; starting anyway");
    return {};
  }
  const uint32_t version = xex.version;
  std::string message = std::format(
      "This build only runs Need for Speed: Carbon Collector's Edition for the Xbox 360 "
      "(title ID 454107EC, version 0.0.0.7).\n\n"
      "The game data you chose has title ID {:08X}, version {}.{}.{}.{}{}.\n\n"
      "Delete game_path.txt in your user data folder to choose a different disc image or "
      "folder at the next start.",
      xex.title_id, (version >> 28) & 0xF, (version >> 24) & 0xF, (version >> 8) & 0xFFFF,
      version & 0xFF,
      xex.title_id == kTitleId && xex.version == kVersion ? " but a different executable" : "");
  REXLOG_ERROR("nfsc: {}", message);
  return message;
}

}  // namespace nfsc
