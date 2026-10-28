// nfsc - "EXIT TO DESKTOP" option at the end of the main menu.
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

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include <rex/hook.h>
#include <rex/logging.h>

#include "exit_option.h"

REX_EXTERN(__imp__sub_82642DB0);  // main menu setup
REX_EXTERN(__imp__sub_82313658);  // menu: add option
REX_EXTERN(__imp__sub_821A3320);  // allocate
REX_EXTERN(__imp__sub_8230EC78);  // option constructor
REX_EXTERN(__imp__sub_8230EE18);  // option React()
REX_EXTERN(__imp__sub_8231F7B8);  // language string lookup

namespace {

constexpr uint32_t kOptionsLabel = 0xD128F223;
constexpr uint32_t kOptionsId = 7;
// Our own label hash; not in the game's language tables, so the lookup hook
// below answers it.
constexpr uint32_t kExitLabel = 0x4E465843;  // "NFXC"
constexpr char kExitText[] = "EXIT TO DESKTOP";
// Unused in the main menu (0-9), but the pause menu uses 10 for PHOTO MODE, so
// React() below recognizes our option by its label.
constexpr uint32_t kExitId = 10;
constexpr uint32_t kAcceptMessage = 0x0C407210;
constexpr uint32_t kOptionSize = 152;
constexpr uint32_t kOptionLabelOffset = 44;
constexpr uint32_t kOptionIdOffset = 48;
constexpr uint32_t kOptionEnabledOffset = 64;

std::atomic<bool> g_in_main_menu_setup{false};
std::atomic<uint32_t> g_exit_text{0};  // guest address of kExitText
std::function<void()> g_quit_handler;

uint32_t LoadU32(uint8_t* base, uint32_t addr) {
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + addr));
}

}  // namespace

namespace nfsc {

void SetQuitHandler(std::function<void()> handler) {
  g_quit_handler = std::move(handler);
}

}  // namespace nfsc

// Main menu setup: only mark the window in which options are added, so the
// add-option hook below does not touch other menus.
REX_HOOK_RAW(sub_82642DB0) {
  g_in_main_menu_setup = true;
  __imp__sub_82642DB0(ctx, base);
  g_in_main_menu_setup = false;
}

// Add option: after the main menu's OPTIONS entry, add EXIT TO DESKTOP the
// same way the game builds its own options.
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

  // The label text lives in guest memory so the game can render it.
  if (g_exit_text == 0) {
    ctx.r3.u64 = sizeof(kExitText);
    ctx.r4.u64 = 0;
    ctx.r5.u64 = 0;
    ctx.r6.u64 = 0;
    __imp__sub_821A3320(ctx, base);
    if (ctx.r3.u32 != 0) {
      std::memcpy(base + ctx.r3.u32, kExitText, sizeof(kExitText));
      g_exit_text = ctx.r3.u32;
    }
  }

  // The constructor reads the "enabled" and second flag through pointers; the
  // setup function keeps them at r1+80 / r1+81 of its own frame for this.
  const uint32_t enabled_ptr = ctx.r1.u32 + 80;
  const uint32_t flag_ptr = ctx.r1.u32 + 81;
  base[enabled_ptr] = 1;
  base[flag_ptr] = 0;

  ctx.r3.u64 = kOptionSize;
  ctx.r4.u64 = 0;
  ctx.r5.u64 = 0;
  ctx.r6.u64 = 0;
  __imp__sub_821A3320(ctx, base);
  const uint32_t exit_option = ctx.r3.u32;
  if (exit_option == 0) {
    REXLOG_WARN("nfsc: could not allocate the EXIT TO DESKTOP menu option");
    return;
  }

  ctx.r3.u64 = exit_option;
  ctx.r4.u64 = kExitLabel;
  ctx.r5.u64 = kExitId;
  ctx.r6.u64 = enabled_ptr;
  ctx.r7.u64 = flag_ptr;
  __imp__sub_8230EC78(ctx, base);

  ctx.r3.u64 = menu;
  ctx.r4.u64 = exit_option;
  __imp__sub_82313658(ctx, base);
  REXLOG_INFO("nfsc: added EXIT TO DESKTOP to the main menu");
}

// Language lookup: answer our own label hash.
REX_HOOK_RAW(sub_8231F7B8) {
  if (ctx.r3.u32 == kExitLabel && g_exit_text != 0) {
    ctx.r3.u64 = g_exit_text;
    return;
  }
  __imp__sub_8231F7B8(ctx, base);
}

// Option React(): accepting EXIT TO DESKTOP exits instead of switching
// front-end state.
REX_HOOK_RAW(sub_8230EE18) {
  const uint32_t option = ctx.r3.u32;
  if (ctx.r5.u32 == kAcceptMessage && base[option + kOptionEnabledOffset] != 0 &&
      LoadU32(base, option + kOptionLabelOffset) == kExitLabel &&
      LoadU32(base, option + kOptionIdOffset) == kExitId) {
    REXLOG_INFO("nfsc: EXIT TO DESKTOP selected in the main menu");
    if (g_quit_handler) {
      g_quit_handler();
    }
    return;
  }
  __imp__sub_8230EE18(ctx, base);
}
