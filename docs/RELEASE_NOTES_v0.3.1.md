# v0.3.1

Stable maintenance release focused on NVIDIA Optical Flow compatibility and diagnostics.

## Fixed

- Fixed confirmed failures where temporal frame 0 succeeds with explicit zero motion vectors but frame 1 fails on the first real `NvOFExecute`.
- NVOF output-grid support is queried with `NvOFGetCaps`; grid 2 is preferred with supported fallbacks.
- D3D11 NVOF input/output surface formats are queried from the driver rather than assuming the same format set on every GPU/driver generation.
- `R8_UNORM` luma is preferred when advertised, with BGRA8/RGBA8 compatibility fallbacks.
- The expected `R16G16_SINT` flow output format is verified before use.
- NVOF-only D3D11 resources avoid unnecessary SRV/UAV bindings where possible, with compatibility fallbacks retained.
- Public `NV_OF_STATUS` decoding is corrected and raw HRESULT-style failures such as `0x80004005` (`E_FAIL`) are reported more clearly.

## Diagnostics

`DLSS 5 NR Runtime Info` now reports the NVOF maximum API version, DLL path, supported and selected D3D11 formats, supported and selected output grids, selected BindFlags, session resolution, raw status and driver last-error text when available.

## Validation

The compatibility fix was validated against a locally reproducible failing input on an RTX 4080 and independently confirmed by the reporter of issue #5 on the previously failing configuration.

## Unchanged temporal behavior

- `still images` remains independent-frame processing.
- `temporal` frame 0 supplies explicit zero MV.
- Later temporal frames use current-to-previous NVIDIA Optical Flow vectors computed from the raw input sequence.
- NVOF performance preset remains FAST.
- NVOF temporal hints remain disabled.

The Neural Rendering integration remains unofficial and experimental and does not redistribute NVIDIA runtime binaries.
