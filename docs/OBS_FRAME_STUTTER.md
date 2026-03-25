# OBS Studio Frame Stutter Analysis

> Investigated: 2026-03-25 | Status: Workaround applied (VSync ON during recording)

## Problem

Frame stuttering occurs in ShowcaseEditor **only** when all four conditions are met:

1. OBS Studio process is running (even minimized to tray, not recording)
2. VSync OFF
3. Edit mode
4. Fullscreen (F11 borderless windowed)

Removing any single condition eliminates the stutter.

## Verified Facts (PresentMon Data)

### PresentMon Data Summary

| Case | Conditions | FPS | Spike Rate (>3x median) | Present API mean |
|------|-----------|-----|--------------------------|-----------------|
| 1 | FS + VSync OFF + OBS | 452 | **10.4%** | **1.141ms** |
| 2 | FS + VSync OFF + No OBS | 1091 | 0.7% | 0.175ms |
| 3 | FS + VSync ON + OBS | 179 | 0.1% | 4.172ms |
| 4 | Windowed + VSync OFF + OBS | 791 | 1.7% | 0.280ms |

Raw CSV files: `tools/case1_stutter.csv` through `tools/case4_windowed.csv`

### Confirmed Findings

1. **Not presentation mode thrashing**: Case 1 stays in `Hardware Composed: Independent Flip` 99.9% of the time. The initial hypothesis of Independent Flip ↔ Composed Flip switching was disproven.

2. **OBS process correlates with Present() latency increase**: Present API call time jumps from 0.175ms (Case 2, no OBS) to 1.141ms mean (Case 1, OBS running), with spikes up to 26.7ms.

3. **Fullscreen worsens the effect**: With OBS running, Present API mean is 1.141ms in fullscreen (Case 1) vs 0.280ms in windowed (Case 4) — a 4x difference.

4. **VSync ON eliminates the stutter**: Case 3 (VSync ON + OBS + fullscreen) has only 0.1% spike rate.

5. **Frame rate alone is not the primary factor**: Case 4 (windowed, 791 FPS) has fewer spikes (1.7%) than Case 1 (fullscreen, 452 FPS, 10.4%).

## Hypotheses (Not Verified)

The following explanations are plausible but **have not been directly confirmed**:

1. **"OBS injects a DXGI Present() hook"**: OBS is widely known to use DXGI hooking for game capture, and the Present() latency data strongly correlates with OBS being running. However, we did not directly verify hook injection (e.g., via DLL inspection or API Monitor). An alternative cause (e.g., OBS GPU context contention) could also explain the latency increase.

2. **"Independent Flip mode is more susceptible to hook interference"**: Fullscreen is measurably worse (4x Present latency), but **why** Independent Flip is more affected than Composed Flip is unknown. It could be related to DWM bypass timing, kernel-mode present path differences, or something else entirely.

3. **"VSync ON absorbs hook latency via VBlank wait"**: VSync ON resolves the issue (fact), but the assumed mechanism — that the ~4.3ms VBlank wait masks the hook delay — is unverified. VSync ON changes multiple things simultaneously (syncInterval, present flags, DWM behavior), so the specific mechanism is unclear.

4. **"Edit mode stutters because of two-pass rendering (higher GPU load)"**: Edit mode is required to reproduce (fact), but we did not collect PresentMon data for Edit vs Play mode comparison. The actual reason Edit mode is affected could be GPU load, rendering pipeline differences, or something else.

5. **"Waitable Swap Chain would fix this"**: All four proposed fixes are speculative — none have been implemented or tested.

## Current Workaround

Use **VSync ON** when recording with OBS. Since OBS captures at 60fps, VSync OFF high-FPS frames are not captured anyway.

Toggle via console command: `vsync`

## Potential Permanent Fixes (Not Implemented, Not Verified)

If a proper fix is desired in the future, these are candidates to investigate. **None have been tested — effectiveness is speculative.**

### 1. Waitable Swap Chain

Add `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` to swap chain creation. Use `SetMaximumFrameLatency()` and wait on `GetFrameLatencyWaitableObject()` in BeginFrame. This lets the DXGI runtime control frame pacing, which may be more resilient to external interference.

Files: `SwapChain.cpp`, `SwapChain.h`, `RenderContext.cpp`

### 2. Tearing support check

The engine unconditionally uses `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` without querying `IDXGIFactory5::CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING)`. This should be checked regardless of the OBS issue.

Files: `Device.cpp`, `Device.h`, `SwapChain.cpp`, `SwapChain.h`

### 3. Signal/Present reorder

Current EndFrame order: Execute → Signal → Present. Recommended DX12 pattern: Execute → Present → Signal, so the fence reflects Present completion.

Files: `RenderContext.cpp`, `CommandQueue.cpp`

### 4. Frame rate limiter

Cap FPS when VSync is off (e.g., monitor refresh rate). Reduces GPU saturation and Present() call frequency. Data shows this alone is unlikely to solve fullscreen stutter, but useful for power/thermal reasons.

Files: `EditorApp.cpp`, `EditorApp.h`, `Timer.h`/`Timer.cpp`

## Suggested Next Steps (If Revisiting)

To narrow down the root cause further before implementing fixes:

1. **Verify OBS hook injection**: Use API Monitor or check loaded DLLs for OBS's capture module (`graphics-hook64.dll`) while ShowcaseEditor runs
2. **Collect Edit vs Play mode data**: Run PresentMon for Play mode + fullscreen + VSync OFF + OBS to isolate why Edit mode is required
3. **Test with other DXGI-hooking apps**: Discord overlay, Steam overlay, etc. to confirm it's a general DXGI hook issue, not OBS-specific
4. **Test Waitable Swap Chain**: Implement Fix 1 and measure before/after with PresentMon

## Reproduction Steps

1. Open OBS Studio (any state — recording, preview, or minimized to tray)
2. Launch ShowcaseEditor
3. Disable VSync: open console, type `vsync`
4. Enter fullscreen: press F11
5. Observe frame stuttering (visible hitching every few frames)

## Tools Used

- **PresentMon** (Microsoft): Frame timing and presentation mode analysis — `tools/PresentMon.exe`
- **Tracy Profiler**: Already integrated in engine (`SE_ZONE_SCOPED` macros) for CPU/GPU zone timing
