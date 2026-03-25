#pragma once

#include <cstdint>

#ifdef SHOWCASE_TRACY_ENABLED
#include <tracy/Tracy.hpp>

// ── Frame ────────────────────────────────────────────────────────
#define SE_FRAME_MARK FrameMark

// ── Zones ────────────────────────────────────────────────────────
#define SE_ZONE_SCOPED ZoneScoped
#define SE_ZONE_SCOPED_N(name) ZoneScopedN(name)
#define SE_ZONE_SCOPED_C(color) ZoneScopedC(color)
#define SE_ZONE_SCOPED_NC(name, color) ZoneScopedNC(name, color)
#define SE_ZONE_TEXT(text, len) ZoneText(text, len)

// ── Metrics ──────────────────────────────────────────────────────
#define SE_PLOT(name, val) TracyPlot(name, val)

// ── Messages ─────────────────────────────────────────────────────
#define SE_MESSAGE(text, len) TracyMessage(text, len)
#define SE_MESSAGE_L(text) TracyMessageL(text)

#else
#define SE_FRAME_MARK (void)0
#define SE_ZONE_SCOPED (void)0
#define SE_ZONE_SCOPED_N(name) (void)0
#define SE_ZONE_SCOPED_C(color) (void)0
#define SE_ZONE_SCOPED_NC(name, color) (void)0
#define SE_ZONE_TEXT(text, len) (void)0
#define SE_PLOT(name, val) (void)0
#define SE_MESSAGE(text, len) (void)0
#define SE_MESSAGE_L(text) (void)0
#endif

// ── Subsystem colors ────────────────────────────────────────────
// Colors for instant subsystem identification in Tracy timeline.
// Use with SE_ZONE_SCOPED_C / SE_ZONE_SCOPED_NC.
namespace showcase::profile {
inline constexpr uint32_t kColorUpdate = 0x9B59B6;    // purple  — input / camera / logic
inline constexpr uint32_t kColorRendering = 0xE74C3C; // red     — scene rendering
inline constexpr uint32_t kColorGPUSync = 0xE67E22;   // orange  — CPU-GPU synchronization
inline constexpr uint32_t kColorImGui = 0x3498DB;     // blue    — ImGui / editor UI
inline constexpr uint32_t kColorAssetIO = 0x2ECC71;   // green   — asset loading / file I/O
} // namespace showcase::profile
