#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_4.h>

#include <cstdint>
#include <string>
#include <vector>

// Coarse S10.5 optical-flow field produced by NVIDIA Optical Flow (NVOFA).
// Each cell stores signed X,Y displacement in units of 1/32 pixel.
struct NvofFlowFrame {
    bool has_flow = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t grid = 0;
    std::vector<int16_t> xy;
};

// Prepare the temporal optical-flow field for one raw RGB input frame.
// - reset=true starts a new sequence; this frame only primes the previous-frame slot.
// - on the first frame out.has_flow is false and the caller should supply zero MVs.
// - subsequent frames return current->previous flow, matching the DLSS reprojection direction.
bool NvofPrepareFrame(
    IDXGIAdapter1* adapter,
    const float* rgb,
    uint32_t width,
    uint32_t height,
    bool reset,
    NvofFlowFrame& out,
    std::string& error);

// Releases the active OFA session and its D3D11 textures while preserving the
// latest diagnostic snapshot for Runtime Info / issue reports.
void NvofReleaseSession();

// Full teardown including nvofapi64.dll.
void NvofShutdown();

// Lightweight driver check used by Runtime Info. Does not create an OFA session.
bool NvofDriverApiAvailable();

// Selected settings. Grid may change after capability probing (preferred = 2).
uint32_t NvofGridSize();
uint32_t NvofPerfLevel();

// Human-readable diagnostic snapshot from the latest NVOF probe/session.
// Includes driver API version, capability-probed grid, supported D3D11 formats,
// selected formats, resolution, and the most recent NVOF failure when available.
const char* NvofDiagnostics();
