// nfsc - the in-game PC SETTINGS window (see settings_window.h).
//
// Controller and keyboard: while the window is open, the game's own pad reads
// (sub_82941FE8, its XInputGetState wrapper) are handed to the window and the
// game sees an untouched pad. The SDK's input system rebuilds its device list
// on every poll, so it can't be polled from the UI thread while the game polls
// it; taking the state from the game's read avoids that. Keyboard keys arrive
// the same way, through the keyboard-to-controller mapping (mnk_mode).

#include "settings_window.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/input/input.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include "profiles.h"

REX_EXTERN(__imp__sub_82941FE8);  // XInputGetState wrapper

namespace {

using namespace rex::input;

// Pad state taken from the game's reads: the buttons, with both sticks folded
// into the d-pad (the keyboard's WASD and arrow keys are the two sticks).
// Capturing starts when the window opens and ends once the pad is released
// after it closed, so the button that closed it doesn't reach the game.
std::atomic<bool> g_capture{false};
std::atomic<bool> g_window_open{false};
std::atomic<uint32_t> g_pad{0};
std::atomic<uint32_t> g_pad_samples{0};

constexpr int kStickThreshold = 16384;

uint16_t LoadU16(const uint8_t* p) {
  return uint16_t(p[0] << 8 | p[1]);
}

uint32_t StickToDpad(int16_t x, int16_t y) {
  uint32_t dpad = 0;
  if (y > kStickThreshold)
    dpad |= X_INPUT_GAMEPAD_DPAD_UP;
  if (y < -kStickThreshold)
    dpad |= X_INPUT_GAMEPAD_DPAD_DOWN;
  if (x < -kStickThreshold)
    dpad |= X_INPUT_GAMEPAD_DPAD_LEFT;
  if (x > kStickThreshold)
    dpad |= X_INPUT_GAMEPAD_DPAD_RIGHT;
  return dpad;
}

}  // namespace

// The game's XInputGetState wrapper: r3 = user index, r4 = XINPUT_STATE.
REX_HOOK_RAW(sub_82941FE8) {
  const uint32_t user = ctx.r3.u32;
  const uint32_t state = ctx.r4.u32;
  __imp__sub_82941FE8(ctx, base);
  if (!g_capture || user != 0 || ctx.r3.u32 != 0 || state == 0) {
    return;
  }
  // XINPUT_GAMEPAD after dwPacketNumber: buttons, triggers, then the sticks.
  uint8_t* gamepad = base + state + 4;
  uint32_t pad = LoadU16(gamepad);
  pad |= StickToDpad(int16_t(LoadU16(gamepad + 4)), int16_t(LoadU16(gamepad + 6)));
  pad |= StickToDpad(int16_t(LoadU16(gamepad + 8)), int16_t(LoadU16(gamepad + 10)));
  std::memset(gamepad, 0, 12);
  g_pad = pad;
  g_pad_samples++;
  if (!g_window_open && pad == 0) {
    g_capture = false;
  }
}

namespace {

std::filesystem::path g_config_path;
std::function<void()> g_restart_handler;
// Cvars changed in the window this session; the restart leaves them off the
// command line so nfsc.toml decides.
std::set<std::string> g_changed;

bool SameValue(std::string_view a, std::string_view b) {
  if (a == b)
    return true;
  char* end_a = nullptr;
  char* end_b = nullptr;
  const std::string sa(a), sb(b);
  const double da = std::strtod(sa.c_str(), &end_a);
  const double db = std::strtod(sb.c_str(), &end_b);
  return !sa.empty() && !sb.empty() && *end_a == '\0' && *end_b == '\0' && da == db;
}

std::string FormatFps(std::string_view value) {
  return fmt::format("{:g} fps", std::strtod(std::string(value).c_str(), nullptr));
}

std::string FormatScale(std::string_view value) {
  if (value == "1")
    return "1x (720p)";
  if (value == "2")
    return "2x (1440p)";
  if (value == "3")
    return "3x (2160p)";
  return fmt::format("{}x", value);
}

std::string FormatEffect(std::string_view value) {
  if (value == "bilinear")
    return "Off";
  if (value == "cas")
    return "AMD CAS";
  return std::string(value);
}

std::string FormatAnisotropic(std::string_view value) {
  constexpr const char* kNames[] = {"The game's own", "Off", "1x", "2x", "4x", "8x", "16x"};
  const int level = std::atoi(std::string(value).c_str());
  if (level >= -1 && level <= 5)
    return kNames[level + 1];
  return std::string(value);
}

struct Choice {
  std::string value;  // "" = not set: the profile's value or the cvar's default
  std::string label;  // empty: from Setting::format
};

struct Setting {
  const char* label;
  std::vector<const char*> cvars;
  bool restart;       // takes effect after a restart
  bool advanced;
  bool from_profile;  // "" means the graphics profile's value
  std::string (*format)(std::string_view value);
  std::vector<Choice> choices;
};

constexpr size_t kProfileSetting = 0;

std::vector<Setting> BuildSettings() {
  std::vector<Setting> settings = {
      {"Graphics profile", {"profile"}, true, false, false, nullptr,
       {{"", "Original: 720p, 30 fps, like the Xbox 360"},
        {"remix", "Remix: 1440p, 120 fps, sharper"}}},
      {"Frame rate limit", {"guest_vblank_rate"}, false, false, true, FormatFps,
       {{"30", ""}, {"60", ""}, {"120", ""}, {"240", ""}}},
      {"Aspect ratio", {"aspect_ratio"}, true, false, false, nullptr,
       {{"", "16:9 (the game's own)"}, {"21:9", "21:9"}, {"32:9", "32:9"}}},
      {"Display", {"fullscreen"}, false, false, false, nullptr,
       {{"", "Fullscreen"}, {"false", "Window"}}},
      {"Resolution", {"draw_resolution_scale_x", "draw_resolution_scale_y"}, true, true, true,
       FormatScale, {{"1", ""}, {"2", ""}, {"3", ""}}},
      {"Sharpening", {"present_effect"}, true, true, true, FormatEffect,
       {{"bilinear", ""}, {"cas", ""}}},
      {"Texture filtering", {"anisotropic_override"}, false, true, true, FormatAnisotropic,
       {{"-1", ""}, {"3", ""}, {"4", ""}, {"5", ""}}},
      {"Graphics API", {"gpu_backend"}, true, true, false, nullptr,
       {{"", "Vulkan"}, {"d3d12", "Direct3D 12"}}},
  };
  // Leave out what this build doesn't have (CAS needs the SDK built with
  // FidelityFX).
  std::erase_if(settings, [](const Setting& setting) {
    for (const char* cvar : setting.cvars) {
      const auto* entry = rex::cvar::GetFlagInfo(cvar);
      if (!entry)
        return true;
      for (const auto& choice : setting.choices) {
        const auto& allowed = entry->constraints.allowed_values;
        if (!choice.value.empty() && !allowed.empty() &&
            std::find(allowed.begin(), allowed.end(), choice.value) == allowed.end()) {
          return true;
        }
      }
    }
    return false;
  });
  for (auto& setting : settings) {
    if (setting.from_profile) {
      setting.choices.insert(setting.choices.begin(), Choice{"", ""});
    }
  }
  return settings;
}

// The choice matching the cvar's current value; a value set by hand that none
// of the choices has is added as one.
std::string CurrentChoice(Setting& setting) {
  const auto* entry = rex::cvar::GetFlagInfo(setting.cvars[0]);
  if (!entry || entry->source == rex::cvar::Source::kDefault)
    return "";
  const std::string value = entry->getter();
  if (!setting.from_profile && SameValue(value, entry->default_value))
    return "";
  for (const auto& choice : setting.choices) {
    if (!choice.value.empty() && SameValue(choice.value, value))
      return choice.value;
  }
  setting.choices.push_back({value, setting.format ? "" : value});
  return value;
}

std::string ChoiceLabel(const Setting& setting, const std::string& value,
                        std::string_view profile) {
  if (value.empty() && setting.from_profile) {
    return fmt::format("Profile ({})",
                       setting.format(nfsc::ProfileDefault(profile, setting.cvars[0])));
  }
  for (const auto& choice : setting.choices) {
    if (choice.value == value && !choice.label.empty())
      return choice.label;
  }
  return setting.format ? setting.format(value) : value;
}

void ApplyLive(const char* cvar, const std::string& value) {
  if (!value.empty()) {
    rex::cvar::SetFlagByName(cvar, value);
    return;
  }
  // Back to the profile's value or the default. SetFlagByName runs the change
  // callbacks (fullscreen needs them); ResetToDefault marks it unset again.
  const auto* entry = rex::cvar::GetFlagInfo(cvar);
  if (!entry)
    return;
  const std::string default_value = entry->default_value;
  rex::cvar::SetFlagByName(cvar, default_value);
  rex::cvar::ResetToDefault(cvar);
}

std::string_view Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

// The key of a "key = value" line, or "" for comments, blank lines and table
// headers.
std::string KeyOf(std::string_view line) {
  line = Trim(line);
  if (line.empty() || line.front() == '#' || line.front() == '[')
    return "";
  const size_t eq = line.find('=');
  return eq == std::string_view::npos ? "" : std::string(Trim(line.substr(0, eq)));
}

// Updates the given keys in nfsc.toml ("" removes the key) and keeps
// everything else, comments included. The keys go in the root table, after
// its last key.
bool SaveToConfig(const std::vector<std::pair<std::string, std::string>>& changes) {
  std::vector<std::string> lines;
  {
    std::ifstream in(g_config_path);
    for (std::string line; std::getline(in, line);) {
      lines.push_back(line);
    }
  }
  if (lines.empty()) {
    lines.push_back("# Settings for nfsc.exe; see the README. PC SETTINGS in the game updates");
    lines.push_back("# the ones it shows and keeps the rest of this file.");
  }

  auto changed = [&](const std::string& key) {
    return std::any_of(changes.begin(), changes.end(),
                       [&](const auto& change) { return change.first == key; });
  };
  size_t insert_at = 0;
  for (size_t i = 0; i < lines.size();) {
    const std::string_view line = Trim(lines[i]);
    if (!line.empty() && line.front() == '[')
      break;  // End of the root table.
    const std::string key = KeyOf(line);
    if (!key.empty() && changed(key)) {
      lines.erase(lines.begin() + i);
      continue;
    }
    if (!key.empty() || (insert_at == i && (line.empty() || line.front() == '#')))
      insert_at = i + 1;
    i++;
  }

  std::vector<std::string> added;
  for (const auto& [cvar, value] : changes) {
    if (value.empty())
      continue;
    const auto* entry = rex::cvar::GetFlagInfo(cvar);
    if (entry && entry->type == rex::cvar::FlagType::String) {
      std::string escaped;
      for (char c : value) {
        if (c == '"' || c == '\\')
          escaped += '\\';
        escaped += c;
      }
      added.push_back(fmt::format("{} = \"{}\"", cvar, escaped));
    } else {
      added.push_back(fmt::format("{} = {}", cvar, value));
    }
  }
  lines.insert(lines.begin() + insert_at, added.begin(), added.end());

  std::ofstream out(g_config_path, std::ios::trunc);
  for (const auto& line : lines) {
    out << line << '\n';
  }
  if (!out) {
    REXLOG_ERROR("nfsc: could not save the settings to {}", g_config_path.string());
    return false;
  }
  REXLOG_INFO("nfsc: saved {} setting(s) to {}", changes.size(), g_config_path.string());
  return true;
}

class SettingsWindow final : public rex::ui::ImGuiDialog {
 public:
  explicit SettingsWindow(rex::ui::ImGuiDrawer* drawer)
      : ImGuiDialog(drawer), settings_(BuildSettings()) {
    for (auto& setting : settings_) {
      initial_.push_back(CurrentChoice(setting));
    }
    pending_ = initial_;

    g_pad = 0;
    pad_samples_at_open_ = g_pad_samples;
    g_window_open = true;
    g_capture = true;

    ImGuiIO& io = GetIO();
    had_gamepad_nav_ = (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  }

  void RequestClose() { closing_ = true; }

 protected:
  void OnClose() override;
  void OnDraw(ImGuiIO& io) override;

 private:
  struct PadKey {
    uint32_t buttons;
    ImGuiKey key;
  };
  static constexpr PadKey kPadKeys[] = {
      {X_INPUT_GAMEPAD_DPAD_UP, ImGuiKey_GamepadDpadUp},
      {X_INPUT_GAMEPAD_DPAD_DOWN, ImGuiKey_GamepadDpadDown},
      {X_INPUT_GAMEPAD_DPAD_LEFT, ImGuiKey_GamepadDpadLeft},
      {X_INPUT_GAMEPAD_DPAD_RIGHT, ImGuiKey_GamepadDpadRight},
      // Start too: on the keyboard that's Enter.
      {X_INPUT_GAMEPAD_A | X_INPUT_GAMEPAD_START, ImGuiKey_GamepadFaceDown},
      {X_INPUT_GAMEPAD_B, ImGuiKey_GamepadFaceRight},
  };

  void FeedPad(ImGuiIO& io);
  void DrawSetting(size_t index, float label_width, float combo_width);
  void Apply();
  bool NeedsRestart() const;

  std::vector<Setting> settings_;
  std::vector<std::string> initial_;
  std::vector<std::string> pending_;
  bool closing_ = false;
  bool first_frame_ = true;
  bool had_gamepad_nav_ = false;
  // Buttons held when the window opened (the one that opened it) are ignored
  // until released.
  uint32_t ignored_buttons_ = ~0u;
  uint32_t pad_samples_at_open_ = 0;
  uint32_t previous_pad_ = 0;
};

SettingsWindow* g_window = nullptr;

void SettingsWindow::OnClose() {
  ImGuiIO& io = GetIO();
  for (const auto& pad_key : kPadKeys) {
    io.AddKeyEvent(pad_key.key, false);
  }
  if (!had_gamepad_nav_) {
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
  }
  g_window_open = false;
  g_window = nullptr;
}

void SettingsWindow::FeedPad(ImGuiIO& io) {
  uint32_t pad = g_pad;
  if (g_pad_samples != pad_samples_at_open_) {
    ignored_buttons_ &= pad;
  }
  pad &= ~ignored_buttons_;
  for (const auto& pad_key : kPadKeys) {
    io.AddKeyEvent(pad_key.key, (pad & pad_key.buttons) != 0);
  }
  // B closes the window, unless it closes an open list first.
  const bool b_pressed = (pad & X_INPUT_GAMEPAD_B) && !(previous_pad_ & X_INPUT_GAMEPAD_B);
  if (b_pressed &&
      !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
    closing_ = true;
  }
  previous_pad_ = pad;
}

bool SettingsWindow::NeedsRestart() const {
  for (size_t i = 0; i < settings_.size(); i++) {
    if (settings_[i].restart && pending_[i] != initial_[i])
      return true;
  }
  return false;
}

void SettingsWindow::Apply() {
  std::vector<std::pair<std::string, std::string>> changes;
  for (size_t i = 0; i < settings_.size(); i++) {
    if (pending_[i] == initial_[i])
      continue;
    for (const char* cvar : settings_[i].cvars) {
      changes.emplace_back(cvar, pending_[i]);
      g_changed.insert(cvar);
      if (!settings_[i].restart) {
        ApplyLive(cvar, pending_[i]);
      }
    }
  }
  const bool restart = NeedsRestart();
  initial_ = pending_;
  if (SaveToConfig(changes) && restart && g_restart_handler) {
    g_restart_handler();
  }
}

void SettingsWindow::DrawSetting(size_t index, float label_width, float combo_width) {
  const Setting& setting = settings_[index];
  const std::string profile = pending_[kProfileSetting].empty() ? "original"
                                                                : pending_[kProfileSetting];
  ImGui::PushID(int(index));
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(setting.label);
  ImGui::SameLine(label_width);
  ImGui::SetNextItemWidth(combo_width);
  const std::string current = ChoiceLabel(setting, pending_[index], profile);
  if (ImGui::BeginCombo("##value", current.c_str())) {
    for (const auto& choice : setting.choices) {
      const bool selected = choice.value == pending_[index];
      if (ImGui::Selectable(ChoiceLabel(setting, choice.value, profile).c_str(), selected)) {
        pending_[index] = choice.value;
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (setting.restart) {
    ImGui::SameLine();
    ImGui::TextDisabled("*");
  }
  ImGui::PopID();
}

void SettingsWindow::OnDraw(ImGuiIO& io) {
  if (closing_) {
    Close();
    return;
  }
  FeedPad(io);
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    closing_ = true;
  }

  // The SDK's ImGui font is a small bitmap font; scale it in whole steps (3x at
  // 1440p, 1x in a 1280x720 window).
  const float scale = std::max(
      1.0f, std::floor(std::min(io.DisplaySize.y / 480.0f, io.DisplaySize.x / 850.0f)));
  const ImGuiStyle& style = ImGui::GetStyle();
  const float font_size = style.FontSizeBase > 0.0f ? style.FontSizeBase : 10.0f;
  ImGui::PushFont(nullptr, font_size * scale);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(style.WindowPadding.x * scale * 2, style.WindowPadding.y * scale * 2));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(style.FramePadding.x * scale, style.FramePadding.y * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(style.ItemSpacing.x * scale, style.ItemSpacing.y * scale * 1.5f));

  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.94f);
  if (first_frame_) {
    ImGui::SetNextWindowFocus();
    first_frame_ = false;
  }
  bool open = true;
  if (ImGui::Begin("PC Settings##nfsc", &open,
                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {
    const std::string profile = pending_[kProfileSetting].empty() ? "original"
                                                                  : pending_[kProfileSetting];
    float label_width = 0.0f;
    float combo_width = 0.0f;
    for (const auto& setting : settings_) {
      label_width = std::max(label_width, ImGui::CalcTextSize(setting.label).x);
      for (const auto& choice : setting.choices) {
        combo_width = std::max(
            combo_width, ImGui::CalcTextSize(ChoiceLabel(setting, choice.value, profile).c_str()).x);
      }
    }
    label_width += ImGui::GetStyle().ItemSpacing.x * 3 + ImGui::GetStyle().WindowPadding.x;
    combo_width += ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 4;

    for (size_t i = 0; i < settings_.size(); i++) {
      if (!settings_[i].advanced)
        DrawSetting(i, label_width, combo_width);
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Advanced")) {
      for (size_t i = 0; i < settings_.size(); i++) {
        if (settings_[i].advanced)
          DrawSetting(i, label_width, combo_width);
      }
    }

    ImGui::Separator();
    ImGui::TextDisabled("* Takes effect after a restart.");
    ImGui::Spacing();
    // Apply is the wider button, so moving down from the full-width Advanced
    // header lands on it (ImGui picks the candidate nearest to its center).
    const float buttons_width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginDisabled(pending_ == initial_);
    if (ImGui::Button(NeedsRestart() ? "Apply and restart" : "Apply",
                      ImVec2(buttons_width * 0.65f, 0.0f))) {
      Apply();
      closing_ = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(buttons_width * 0.35f, 0.0f))) {
      closing_ = true;
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Controller: A select, B back. Keyboard: WASD or arrows, Space, Backspace.");
  }
  ImGui::End();
  ImGui::PopStyleVar(3);
  ImGui::PopFont();
  if (!open) {
    closing_ = true;
  }
}

#if defined(_WIN32)
// Quotes an argument the way CommandLineToArgvW reads it back.
std::wstring QuoteArgument(const std::wstring& arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos)
    return arg;
  std::wstring quoted = L"\"";
  size_t backslashes = 0;
  for (wchar_t c : arg) {
    if (c == L'\\') {
      backslashes++;
      continue;
    }
    quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
    backslashes = 0;
    quoted += c;
  }
  quoted.append(backslashes * 2, L'\\');
  quoted += L'"';
  return quoted;
}

// Tells the new process which one to wait for. An environment variable rather
// than a cvar, so it can't end up in nfsc.toml.
constexpr wchar_t kWaitForPidVariable[] = L"NFSC_WAIT_FOR_PID";

void StartAgain() {
  std::wstring exe(32768, L'\0');
  exe.resize(GetModuleFileNameW(nullptr, exe.data(), DWORD(exe.size())));
  std::wstring command_line = QuoteArgument(exe);

  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; argv && i < argc; i++) {
    const std::wstring arg = argv[i];
    const bool dropped = std::any_of(g_changed.begin(), g_changed.end(), [&](const auto& cvar) {
      const std::wstring flag = L"--" + std::wstring(cvar.begin(), cvar.end());
      return arg == flag || arg.starts_with(flag + L"=");
    });
    if (!dropped) {
      command_line += L" " + QuoteArgument(arg);
    }
  }
  LocalFree(argv);

  SetEnvironmentVariableW(kWaitForPidVariable, std::to_wstring(GetCurrentProcessId()).c_str());
  STARTUPINFOW startup_info = {sizeof(startup_info)};
  PROCESS_INFORMATION process_info = {};
  if (CreateProcessW(exe.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                     nullptr, &startup_info, &process_info)) {
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
  } else {
    REXLOG_ERROR("nfsc: could not start the game again (error {})", GetLastError());
  }
  SetEnvironmentVariableW(kWaitForPidVariable, nullptr);
}
#endif

}  // namespace

namespace nfsc {

void SetSettingsConfigPath(std::filesystem::path path) {
  g_config_path = std::move(path);
}

void SetRestartHandler(std::function<void()> handler) {
  g_restart_handler = std::move(handler);
}

void OpenSettingsWindow(rex::ui::ImGuiDrawer* drawer) {
  if (!g_window && drawer) {
    g_window = new SettingsWindow(drawer);  // Deletes itself when closed.
  }
}

void ToggleSettingsWindow(rex::ui::ImGuiDrawer* drawer) {
  if (g_window) {
    g_window->RequestClose();
  } else {
    OpenSettingsWindow(drawer);
  }
}

void StartNewInstance() {
#if defined(_WIN32)
  StartAgain();
#endif
}

void WaitForPreviousInstance() {
#if defined(_WIN32)
  wchar_t pid[16] = {};
  if (!GetEnvironmentVariableW(kWaitForPidVariable, pid, DWORD(std::size(pid))))
    return;
  SetEnvironmentVariableW(kWaitForPidVariable, nullptr);
  if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(std::wcstoul(pid, nullptr, 10)))) {
    WaitForSingleObject(process, 30000);
    CloseHandle(process);
  }
#endif
}

}  // namespace nfsc
