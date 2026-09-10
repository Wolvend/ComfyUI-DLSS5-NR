# v0.3.1-fix1

Compatibility prerelease based on v0.3.0, focused on NVIDIA Optical Flow D3D11 failures reported on newer GPU/driver combinations.

## What changed

- NVOF output-grid capability probing (`NvOFGetCaps`).
- D3D11 NVOF surface-format probing (`NvOFGetSurfaceFormatCountD3D11` / `NvOFGetSurfaceFormatD3D11`).
- Dynamic input-format selection while preserving the v0.3.0 BGRA8 path when supported.
- Verification of the expected `R16G16_SINT` flow output format.
- Correct public NVOF status names and clearer handling of raw HRESULT-style failures.
- Much richer Runtime Info diagnostics for issue reports.

## Test target

The main target is the failure pattern where temporal frame 0 succeeds with explicit zero motion vectors, but frame 1 fails on the first real `NvOFExecute` (for example issue #5 on an RTX 5060 Ti).

Please attach the full `DLSS 5 NR Runtime Info` output after testing, especially the supported/selected NVOF formats and grids.

## Behavior intentionally unchanged

- `still images` remains independent-frame processing.
- `temporal` frame 0 still supplies zero MV.
- Later temporal frames still use current-to-previous NVOF vectors.
- NVOF performance preset remains FAST.
- NVOF temporal hints remain disabled for now.
