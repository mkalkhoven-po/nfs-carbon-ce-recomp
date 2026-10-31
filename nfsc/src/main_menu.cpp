// nfsc - "PC SETTINGS" and "EXIT TO DESKTOP" options at the end of the main menu.
//
// Reverse engineered from default.xex (title 454107EC, v0.0.0.7):
//   sub_82642DB0  main menu setup: for each option allocates 152 bytes,
//                 constructs it with sub_8230EC78(obj, label_hash, option_id,
//                 &enabled, &flag) and adds it with sub_82313658(menu, obj).
//                 OPTIONS is label 0xD128F223, id 7.
//   sub_8230EE18  option React(): on the accept message 0x0C407210 it passes the
//                 option id (+48) to the front-end state manager.
//   sub_8231F7B8  language string lookup: hash -> string pointer, or 0 (callers
//                 then fall back to "DEFAULT STRING").

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include <rex/hook.h>
#include <rex/logging.h>

#include "main_menu.h"

REX_EXTERN(__imp__sub_82642DB0);  // main menu setup
REX_EXTERN(__imp__sub_82313658);  // menu: add option
REX_EXTERN(__imp__sub_821A3320);  // allocate
REX_EXTERN(__imp__sub_8230EC78);  // option constructor
REX_EXTERN(__imp__sub_8230EE18);  // option React()
REX_EXTERN(__imp__sub_8231F7B8);  // language string lookup

namespace {

constexpr uint32_t kOptionsLabel = 0xD128F223;
constexpr uint32_t kOptionsId = 7;
constexpr uint32_t kAcceptMessage = 0x0C407210;
constexpr uint32_t kOptionSize = 152;
constexpr uint32_t kOptionLabelOffset = 44;
constexpr uint32_t kOptionIdOffset = 48;
constexpr uint32_t kOptionEnabledOffset = 64;

// Our options, added after OPTIONS in this order. The label hashes are our own
// and not in the game's language tables, so the lookup hook below answers them.
// The ids are unused in the main menu (0-9), but the pause menu uses 10 for
// PHOTO MODE, so React() below recognizes our options by their labels.
struct MenuOption {
  uint32_t label;
  uint32_t id;
  const char* text;
  std::function<void()> handler;
  std::atomic<uint32_t> guest_text{0};  // guest address of text
};
std::array<MenuOption, 2> g_options = {{
    {0x4E465853, 11, "PC SETTINGS"},      // "NFXS"
    {0x4E465843, 10, "EXIT TO DESKTOP"},  // "NFXC"
}};
MenuOption& g_settings_option = g_options[0];
MenuOption& g_exit_option = g_options[1];

std::atomic<bool> g_in_main_menu_setup{false};

uint32_t LoadU32(uint8_t* base, uint32_t addr) {
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + addr));
}

}  // namespace

namespace nfsc {

void SetQuitHandler(std::function<void()> handler) {
  g_exit_option.handler = std::move(handler);
}

void SetSettingsHandler(std::function<void()> handler) {
  g_settings_option.handler = std::move(handler);
}

}  // namespace nfsc

// Main menu setup: only mark the window in which options are added, so the
// add-option hook below does not touch other menus.
REX_HOOK_RAW(sub_82642DB0) {
  g_in_main_menu_setup = true;
  __imp__sub_82642DB0(ctx, base);
  g_in_main_menu_setup = false;
}

// Add option: after the main menu's OPTIONS entry, add ours the same way the
// game builds its own options.
REX_HOOK_RAW(sub_82313658) {
  const uint32_t menu = ctx.r3.u32;
  const uint32_t option = ctx.r4.u32;
  const bool after_options = g_in_main_menu_setup && option != 0 &&
                             LoadU32(base, option + kOptionLabelOffset) == kOptionsLabel &&
                             LoadU32(base, option + kOptionIdOffset) == kOptionsId;
  __imp__sub_82313658(ctx, base);
  if (!after_options) {
    return;
  }

  // The constructor reads the "enabled" and second flag through pointers; the
  // setup function keeps them at r1+80 / r1+81 of its own frame for this.
  const uint32_t enabled_ptr = ctx.r1.u32 + 80;
  const uint32_t flag_ptr = ctx.r1.u32 + 81;

  for (auto& added : g_options) {
    // The label text lives in guest memory so the game can render it.
    if (added.guest_text == 0) {
      const size_t size = std::strlen(added.text) + 1;
      ctx.r3.u64 = size;
      ctx.r4.u64 = 0;
      ctx.r5.u64 = 0;
      ctx.r6.u64 = 0;
      __imp__sub_821A3320(ctx, base);
      if (ctx.r3.u32 != 0) {
        std::memcpy(base + ctx.r3.u32, added.text, size);
        added.guest_text = ctx.r3.u32;
      }
    }

    base[enabled_ptr] = 1;
    base[flag_ptr] = 0;

    ctx.r3.u64 = kOptionSize;
    ctx.r4.u64 = 0;
    ctx.r5.u64 = 0;
    ctx.r6.u64 = 0;
    __imp__sub_821A3320(ctx, base);
    const uint32_t new_option = ctx.r3.u32;
    if (new_option == 0) {
      REXLOG_WARN("nfsc: could not allocate the {} menu option", added.text);
      return;
    }

    ctx.r3.u64 = new_option;
    ctx.r4.u64 = added.label;
    ctx.r5.u64 = added.id;
    ctx.r6.u64 = enabled_ptr;
    ctx.r7.u64 = flag_ptr;
    __imp__sub_8230EC78(ctx, base);

    ctx.r3.u64 = menu;
    ctx.r4.u64 = new_option;
    __imp__sub_82313658(ctx, base);
    REXLOG_INFO("nfsc: added {} to the main menu", added.text);
  }
}

// Language lookup: answer our own label hashes.
REX_HOOK_RAW(sub_8231F7B8) {
  for (const auto& added : g_options) {
    if (ctx.r3.u32 == added.label && added.guest_text != 0) {
      ctx.r3.u64 = added.guest_text;
      return;
    }
  }
  __imp__sub_8231F7B8(ctx, base);
}

// Option React(): accepting one of ours runs its handler instead of switching
// front-end state.
REX_HOOK_RAW(sub_8230EE18) {
  const uint32_t option = ctx.r3.u32;
  if (ctx.r5.u32 == kAcceptMessage && base[option + kOptionEnabledOffset] != 0) {
    for (const auto& added : g_options) {
      if (LoadU32(base, option + kOptionLabelOffset) == added.label &&
          LoadU32(base, option + kOptionIdOffset) == added.id) {
        REXLOG_INFO("nfsc: {} selected in the main menu", added.text);
        if (added.handler) {
          added.handler();
        }
        return;
      }
    }
  }
  __imp__sub_8230EE18(ctx, base);
}
