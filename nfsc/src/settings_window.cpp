// nfsc - the in-game PC SETTINGS window (see settings_window.h).
//
// Controller and keyboard reach the window through pad.h; while it is open, the game gets
// the same notification as when the Xbox guide opens and pauses a race.
//
// A choice with an empty value means "not chosen": the setting then comes from the graphics
// profile (the SDK's title layer) or its default. Apply writes chosen values at the runtime
// layer and into nfsc.toml, and removes values set back to the profile.

#include "settings_window.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include <rex/hid/pad.h>
#include <rex/input_keys.h>
#include <rex/logging/log_category.h>
#include <rex/settings_registry.h>
#include <rex/ui/ui_system.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "display.h"
#include "pad.h"
#include "profiles.h"
#include "settings.h"

namespace {

using namespace rex::hid;

rex::ui::IUISystem* g_ui = nullptr;
std::filesystem::path g_config_path;
std::function<void()> g_restart_handler;
std::function<void(bool)> g_pause_handler;
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
  if (value == "monitor")
    return std::format("Monitor ({:.0f} fps)", nfsc::MonitorFrameRate());
  return std::format("{:g} fps", std::strtod(std::string(value).c_str(), nullptr));
}

std::string FormatScale(std::string_view value) {
  if (value == "1")
    return "1x (720p)";
  if (value == "2")
    return "2x (1440p)";
  if (value == "3")
    return "3x (2160p)";
  return std::format("{}x", value);
}

std::string FormatIntro(std::string_view value) {
  return value == "true" ? "Skip" : "Play";
}

std::string FormatAnisotropic(std::string_view value) {
  constexpr const char* kNames[] = {"The game's own", "Off", "1x", "2x", "4x", "8x", "16x"};
  const int level = std::atoi(std::string(value).c_str());
  if (level >= -1 && level <= 5)
    return kNames[level + 1];
  return std::string(value);
}

std::string FormatDriver(std::string_view value) {
  if (value == "vulkan")
    return "Vulkan";
  if (value == "d3d12")
    return "Direct3D 12";
  return std::string(value);
}

struct Choice {
  std::string value;  // "" = not chosen: the profile's value or the default
  std::string label;  // empty: from Setting::format
};

struct Setting {
  const char* label;
  std::vector<const char*> names;  // full setting names
  bool restart;                    // takes effect after a restart
  bool advanced;
  bool from_profile;  // "" shows the graphics profile's value
  std::string (*format)(std::string_view value);
  std::vector<Choice> choices;
  // A lower-level setting for the same thing, which a config may set instead (frame rate
  // limit: ui.video_mode_refresh_rate). It goes when this one changes.
  const char* replaces = nullptr;
};

constexpr size_t kProfileSetting = 0;

// The keyboard's controller mapping (hid.mnk_mode).
struct KeyBind {
  const char* label;
  const char* name;
  std::string initial = {};
  std::string pending = {};
};

std::vector<KeyBind> BuildKeyBinds() {
  std::vector<KeyBind> binds = {
      {"Steer left", "hid.keybind_lstick_left"},
      {"Steer right", "hid.keybind_lstick_right"},
      {"Up", "hid.keybind_lstick_up"},
      {"Down", "hid.keybind_lstick_down"},
      {"Throttle (RT)", "hid.keybind_right_trigger"},
      {"Brake (LT)", "hid.keybind_left_trigger"},
      {"A button", "hid.keybind_a"},
      {"B button", "hid.keybind_b"},
      {"X button", "hid.keybind_x"},
      {"Y button", "hid.keybind_y"},
      {"Left shoulder (LB)", "hid.keybind_left_shoulder"},
      {"Right shoulder (RB)", "hid.keybind_right_shoulder"},
      {"Start", "hid.keybind_start"},
      {"Back", "hid.keybind_back"},
  };
  std::erase_if(binds, [](const KeyBind& bind) { return !nfsc::FindSetting(bind.name); });
  for (auto& bind : binds) {
    bind.initial = nfsc::SettingText(bind.name);
    bind.pending = bind.initial;
  }
  return binds;
}

std::string DefaultText(const char* name) {
  const rex::SettingBase* setting = nfsc::FindSetting(name);
  return setting ? setting->GetDefaultText() : std::string();
}

// The name of a key that is down, or "".
std::string PressedKey() {
#if defined(_WIN32)
  for (int vk = 0x08; vk < 0xFF; ++vk) {
    if (GetAsyncKeyState(vk) & 0x8000) {
      std::string name = rex::VirtualKeyToString(static_cast<rex::VirtualKey>(vk));
      if (!name.empty())
        return name;
    }
  }
#endif
  return {};
}

std::vector<Setting> BuildSettings() {
  const nfsc::DisplayMode& display = nfsc::CurrentDisplay();
  std::vector<Setting> settings = {
      {"Graphics profile", {"nfsc.profile"}, true, false, false, nullptr,
       {{"", "Original: 720p, 30 fps, like the Xbox 360"},
        {"remix", "Remix: sharper, smoother, follows your monitor"}}},
      {"Frame rate limit", {"nfsc.frame_rate_limit"}, true, false, true, FormatFps,
       {{"monitor", ""}, {"30", ""}, {"60", ""}, {"120", ""}, {"240", ""}},
       "ui.video_mode_refresh_rate"},
      {"Aspect ratio", {"nfsc.aspect_ratio"}, true, false, false, nullptr,
       {{"", std::format("Monitor ({}x{})", display.width, display.height)},
        {"16:9", "16:9 (the game's own)"},
        {"21:9", "21:9"},
        {"32:9", "32:9"}}},
      {"Display", {"ui.fullscreen"}, false, false, false, nullptr,
       {{"", "Fullscreen"}, {"false", "Window"}}},
      {"Logos and intro videos", {"nfsc.skip_intro"}, false, false, true, FormatIntro,
       {{"false", ""}, {"true", ""}}},
      {"Resolution", {"gpu.resolution_scale_x", "gpu.resolution_scale_y"}, true, true, true,
       FormatScale, {{"1", ""}, {"2", ""}, {"3", ""}}},
      {"Texture filtering", {"gpu.anisotropic_override"}, false, true, true, FormatAnisotropic,
       {{"-1", ""}, {"3", ""}, {"4", ""}, {"5", ""}}},
      {"Graphics API", {"gpu.driver"}, true, true, true, FormatDriver,
       {{"vulkan", ""}, {"d3d12", ""}}},
      {"Save location", {"nfsc.portable"}, true, true, false, nullptr,
       {{"", "Documents"}, {"true", "Next to nfsc.exe (portable)"}}},
  };
  std::erase_if(settings, [](const Setting& setting) {
    return std::ranges::any_of(setting.names,
                               [](const char* name) { return !nfsc::FindSetting(name); });
  });
  for (auto& setting : settings) {
    if (setting.from_profile) {
      setting.choices.insert(setting.choices.begin(), Choice{"", ""});
    }
  }
  return settings;
}

// The choice matching the setting's current value; a value set by hand that none of the
// choices has is added as one.
std::string CurrentChoice(Setting& setting) {
  const char* name = setting.names[0];
  if (!nfsc::ChosenByPlayer(name)) {
    // A config that sets the lower-level setting instead shows that value.
    if (!setting.replaces || !nfsc::ChosenByPlayer(setting.replaces))
      return "";
    name = setting.replaces;
  }
  const std::string value = nfsc::SettingText(name);
  if (!setting.from_profile && SameValue(value, DefaultText(name)))
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
    return std::format("Profile: {}",
                       setting.format(nfsc::ProfileDefault(profile, setting.names[0])));
  }
  for (const auto& choice : setting.choices) {
    if (choice.value == value && !choice.label.empty())
      return choice.label;
  }
  return setting.format ? setting.format(value) : value;
}

// Applies a value now: at the runtime layer, or, for "", back to the profile's value or the
// default.
void ApplyLive(const char* name, const std::string& value) {
  rex::SettingBase* setting = nfsc::FindSetting(name);
  if (!setting)
    return;
  if (!value.empty()) {
    if (auto result = setting->SetText(rex::SettingLayer::kRuntime, value); !result) {
      REXLOG_WARN("nfsc: {} = {}: {}", name, value, result.error().GetDescription());
    }
    return;
  }
  (void)setting->Reset();
  const std::string profile_value = nfsc::ProfileDefault(nfsc::ActiveProfile(), name);
  if (!profile_value.empty()) {
    (void)setting->SetText(rex::SettingLayer::kTitle, profile_value);
  }
}

std::string_view Trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

std::string Unquote(std::string_view s) {
  s = Trim(s);
  if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front())
    s = s.substr(1, s.size() - 2);
  return std::string(s);
}

// The full name a "key = value" line sets inside a table ("" for the root table), or "" for
// comments, blank lines and table headers.
std::string FullNameOf(std::string_view line, const std::string& table) {
  line = Trim(line);
  if (line.empty() || line.front() == '#' || line.front() == '[')
    return "";
  const size_t eq = line.find('=');
  if (eq == std::string_view::npos)
    return "";
  const std::string key = Unquote(line.substr(0, eq));
  return table.empty() ? key : table + "." + key;
}

// Updates the given settings in nfsc.toml ("" removes them) and keeps everything else,
// comments included. A setting is removed wherever the file sets it (its table, the flat
// form or an old name); new values go into the root table in the flat form,
// "scope.name" = value.
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

  std::set<std::string> removed;
  for (const auto& [name, value] : changes) {
    removed.insert(name);
    for (const auto& old_name : nfsc::OldNamesOf(name)) {
      removed.insert(old_name);
    }
  }
  std::string table;
  size_t insert_at = 0;
  bool in_root = true;
  for (size_t i = 0; i < lines.size();) {
    const std::string_view line = Trim(lines[i]);
    if (!line.empty() && line.front() == '[') {
      const size_t end = line.find(']');
      table = Unquote(line.substr(1, end == std::string_view::npos ? 0 : end - 1));
      in_root = false;
      i++;
      continue;
    }
    const std::string name = FullNameOf(line, table);
    if (!name.empty() && removed.contains(name)) {
      lines.erase(lines.begin() + std::ptrdiff_t(i));
      continue;
    }
    if (in_root && (!name.empty() || (insert_at == i && (line.empty() || line.front() == '#'))))
      insert_at = i + 1;
    i++;
  }

  std::vector<std::string> added;
  for (const auto& [name, value] : changes) {
    if (value.empty())
      continue;
    const rex::SettingBase* setting = nfsc::FindSetting(name);
    const bool is_text = setting && (setting->GetType() == rex::SettingType::kString ||
                                     setting->GetType() == rex::SettingType::kPath);
    if (is_text) {
      std::string escaped;
      for (char c : value) {
        if (c == '"' || c == '\\')
          escaped += '\\';
        escaped += c;
      }
      added.push_back(std::format("\"{}\" = \"{}\"", name, escaped));
    } else {
      added.push_back(std::format("\"{}\" = {}", name, value));
    }
  }
  lines.insert(lines.begin() + std::ptrdiff_t(insert_at), added.begin(), added.end());

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

class SettingsWindow final : public rex::ui::DialogBase {
 public:
  SettingsWindow() : settings_(BuildSettings()), binds_(BuildKeyBinds()) {
    for (auto& setting : settings_) {
      initial_.push_back(CurrentChoice(setting));
    }
    pending_ = initial_;
    nfsc::SetPadCaptured(true);
    if (g_pause_handler) {
      g_pause_handler(true);
    }
  }

  void RequestClose() { closing_ = true; }

  bool Draw(ImGuiIO& io) override;
  void OnClose() override;

 private:
  struct PadKey {
    uint32_t buttons;
    ImGuiKey key;
  };
  static constexpr PadKey kPadKeys[] = {
      {kPadDPadUp, ImGuiKey_GamepadDpadUp},
      {kPadDPadDown, ImGuiKey_GamepadDpadDown},
      {kPadDPadLeft, ImGuiKey_GamepadDpadLeft},
      {kPadDPadRight, ImGuiKey_GamepadDpadRight},
      // Start too: on the keyboard that's Enter.
      {kPadA | kPadStart, ImGuiKey_GamepadFaceDown},
      {kPadB, ImGuiKey_GamepadFaceRight},
  };

  void FeedPad(ImGuiIO& io);
  void DrawSetting(size_t index, float label_width, float combo_width);
  void DrawSettings();
  void DrawKeyBinds();
  void UpdateKeyCapture();
  void FocusFirstItem();
  bool Apply();
  bool HasChanges() const;
  bool NeedsRestart() const;

  std::vector<Setting> settings_;
  std::vector<KeyBind> binds_;
  std::vector<std::string> initial_;
  std::vector<std::string> pending_;
  bool closing_ = false;
  bool save_failed_ = false;
  bool show_key_binds_ = false;
  bool focus_first_item_ = true;  // Also on opening, so the keys work right away.
  int capturing_key_ = -1;      // index in binds_
  bool capture_armed_ = false;  // the key that started the capture is released
  bool first_frame_ = true;
  int frames_ = 0;
  bool had_gamepad_nav_ = false;
  bool had_keyboard_nav_ = false;
  // Buttons held when the window opened (the one that opened it) are ignored until
  // released.
  uint32_t ignored_buttons_ = ~0u;
  uint32_t previous_pad_ = 0;
};

SettingsWindow* g_window = nullptr;

void SettingsWindow::OnClose() {
  ImGuiIO& io = ImGui::GetIO();
  for (const auto& pad_key : kPadKeys) {
    io.AddKeyEvent(pad_key.key, false);
  }
  if (!had_gamepad_nav_) {
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
  }
  if (had_keyboard_nav_) {
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  }
  nfsc::SetPadCaptured(false);
  if (g_pause_handler) {
    g_pause_handler(false);
  }
  g_window = nullptr;
}

void SettingsWindow::FeedPad(ImGuiIO& io) {
  uint32_t pad = capturing_key_ >= 0 ? 0 : nfsc::ReadPad();
  ignored_buttons_ &= pad;
  pad &= ~ignored_buttons_;
  if (pad != previous_pad_) {  // DEBUG
    REXLOG_INFO("nfsc-debug: frame {} pad {:04X} popup {} profile '{}' navkb {} navpad {}",
                ImGui::GetFrameCount(), pad,
                ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel),
                pending_.empty() ? "" : pending_[0],
                (io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0,
                (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0);
  }
  for (const auto& pad_key : kPadKeys) {
    io.AddKeyEvent(pad_key.key, (pad & pad_key.buttons) != 0);
  }
  // B goes back, unless it closes an open list first.
  const bool b_pressed = (pad & kPadB) && !(previous_pad_ & kPadB);
  if (b_pressed &&
      !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
    if (show_key_binds_) {
      show_key_binds_ = false;
      focus_first_item_ = true;
    } else {
      closing_ = true;
    }
  }
  previous_pad_ = pad;
}

bool SettingsWindow::HasChanges() const {
  return pending_ != initial_ ||
         std::any_of(binds_.begin(), binds_.end(),
                     [](const KeyBind& bind) { return bind.pending != bind.initial; });
}

bool SettingsWindow::NeedsRestart() const {
  for (size_t i = 0; i < settings_.size(); i++) {
    if (settings_[i].restart && pending_[i] != initial_[i])
      return true;
  }
  return false;
}

// Returns whether the settings were saved.
bool SettingsWindow::Apply() {
  std::vector<std::pair<std::string, std::string>> changes;
  for (size_t i = 0; i < settings_.size(); i++) {
    if (pending_[i] == initial_[i])
      continue;
    for (const char* name : settings_[i].names) {
      changes.emplace_back(name, pending_[i]);
      if (!settings_[i].restart) {
        ApplyLive(name, pending_[i]);
      }
    }
    if (settings_[i].replaces) {
      changes.emplace_back(settings_[i].replaces, "");
    }
  }
  for (const auto& bind : binds_) {
    if (bind.pending == bind.initial)
      continue;
    // A key back at its default goes out of nfsc.toml.
    const bool is_default = bind.pending == DefaultText(bind.name);
    changes.emplace_back(bind.name, is_default ? std::string() : bind.pending);
    ApplyLive(bind.name, is_default ? std::string() : bind.pending);
  }
  for (const auto& [name, value] : changes) {
    g_changed.insert(name);
    for (const auto& old_name : nfsc::OldNamesOf(name)) {
      g_changed.insert(old_name);
    }
  }
  save_failed_ = !SaveToConfig(changes);
  if (save_failed_) {
    return false;
  }
  const bool restart = NeedsRestart();
  initial_ = pending_;
  for (auto& bind : binds_) {
    bind.initial = bind.pending;
  }
  if (restart && g_restart_handler) {
    g_restart_handler();
  }
  return true;
}

void SettingsWindow::FocusFirstItem() {
  // Not in the first frame: the window only gets the focus then, and a focus request in
  // the same frame is lost.
  if (focus_first_item_ && frames_ > 0) {
    ImGui::SetKeyboardFocusHere();
    // Show the cursor, so that the first press selects instead of only revealing it.
    ImGui::SetNavCursorVisible(true);
    focus_first_item_ = false;
  }
}

void SettingsWindow::UpdateKeyCapture() {
  if (capturing_key_ < 0)
    return;
  const std::string key = PressedKey();
  if (!capture_armed_) {
    capture_armed_ = key.empty();
    return;
  }
  if (key.empty())
    return;
  if (!ImGui::IsKeyDown(ImGuiKey_Escape)) {
    binds_[size_t(capturing_key_)].pending = key;
  }
  capturing_key_ = -1;
  ignored_buttons_ = ~0u;  // Until the key is released again.
}

void SettingsWindow::DrawKeyBinds() {
  float label_width = 0.0f;
  for (const auto& bind : binds_) {
    label_width = std::max(label_width, ImGui::CalcTextSize(bind.label).x);
  }
  label_width += ImGui::GetStyle().ItemSpacing.x * 3 + ImGui::GetStyle().WindowPadding.x;
  const float key_width =
      ImGui::CalcTextSize("Press a key (Esc cancels)").x + ImGui::GetStyle().FramePadding.x * 2;
  for (size_t i = 0; i < binds_.size(); i++) {
    ImGui::PushID(int(i));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(binds_[i].label);
    ImGui::SameLine(label_width);
    FocusFirstItem();
    const bool capturing = capturing_key_ == int(i);
    if (ImGui::Button(capturing ? "Press a key (Esc cancels)" : binds_[i].pending.c_str(),
                      ImVec2(key_width, 0.0f))) {
      capturing_key_ = int(i);
      capture_armed_ = false;
    }
    const bool in_use = std::any_of(binds_.begin(), binds_.end(), [&](const KeyBind& other) {
      return &other != &binds_[i] && other.pending == binds_[i].pending;
    });
    if (in_use) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "Also used above or below");
    }
    ImGui::PopID();
  }
  ImGui::Spacing();
  if (ImGui::Button("Back to settings")) {
    show_key_binds_ = false;
    focus_first_item_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset keys")) {
    for (auto& bind : binds_) {
      bind.pending = DefaultText(bind.name);
    }
  }
}

void SettingsWindow::DrawSetting(size_t index, float label_width, float combo_width) {
  const Setting& setting = settings_[index];
  const std::string profile =
      pending_[kProfileSetting].empty() ? "original" : pending_[kProfileSetting];
  ImGui::PushID(int(index));
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(setting.label);
  ImGui::SameLine(label_width);
  FocusFirstItem();
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

void SettingsWindow::DrawSettings() {
  const std::string profile =
      pending_[kProfileSetting].empty() ? "original" : pending_[kProfileSetting];
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
  if (ImGui::Button("Keyboard controls", ImVec2(-1.0f, 0.0f))) {
    show_key_binds_ = true;
    focus_first_item_ = true;
  }
}

bool SettingsWindow::Draw(ImGuiIO& io) {
  if (closing_) {
    return false;
  }
  if (first_frame_) {
    had_gamepad_nav_ = (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0;
    had_keyboard_nav_ = (io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // The keyboard reaches the window as a pad (pad.h), like it reaches the game. ImGui's
    // own keyboard navigation would press Space a second time, sometimes a frame apart,
    // which opens a list and closes it again.
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  }
  FeedPad(io);
  if (capturing_key_ < 0 && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    closing_ = true;
  }
  UpdateKeyCapture();

  // Scale the SDK's ImGui font in whole steps (3x at 1440p, 1x in a 1280x720 window).
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
    if (show_key_binds_) {
      DrawKeyBinds();
    } else {
      DrawSettings();
    }

    ImGui::Separator();
    if (save_failed_) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                         "Could not save nfsc.toml. Move the game to a folder you can write to.");
    }
    if (NeedsRestart()) {
      ImGui::TextUnformatted("Restarting goes back to the main menu; unsaved progress is lost.");
    }
    if (!show_key_binds_) {
      ImGui::TextDisabled("* Takes effect after a restart.");
    }
    ImGui::Spacing();
    // Apply is the wider button, so moving down from the full-width Advanced header lands
    // on it (ImGui picks the candidate nearest to its center).
    const float buttons_width = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;
    ImGui::BeginDisabled(!HasChanges());
    if (ImGui::Button(NeedsRestart() ? "Apply and restart" : "Apply",
                      ImVec2(buttons_width * 0.65f, 0.0f))) {
      closing_ = Apply();
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
  frames_++;
  return true;
}

}  // namespace

namespace nfsc {

void SetSettingsUI(rex::ui::IUISystem* ui) {
  g_ui = ui;
}

void SetSettingsConfigPath(std::filesystem::path path) {
  g_config_path = std::move(path);
}

void SetRestartHandler(std::function<void()> handler) {
  g_restart_handler = std::move(handler);
}

void SetPauseHandler(std::function<void(bool)> handler) {
  g_pause_handler = std::move(handler);
}

void OpenSettingsWindow() {
  if (!g_window && g_ui) {
    auto window = std::make_unique<SettingsWindow>();
    g_window = window.get();
    g_ui->AddDialog(std::move(window));  // Removed and deleted after OnClose.
  }
}

void ToggleSettingsWindow() {
  if (g_window) {
    g_window->RequestClose();
  } else {
    OpenSettingsWindow();
  }
}

const std::set<std::string>& ChangedSettings() {
  return g_changed;
}

}  // namespace nfsc
