# ComfyUI-DLSS5-NR

**Unofficial, experimental ComfyUI integration for NVIDIA DLSS 5 Neural Rendering (NGX feature 18).**

The node runs Neural Rendering **in-process** inside ComfyUI through a small native D3D12 bridge. It does not launch a helper executable and it does not write temporary image files.

> [!WARNING]
> This project targets an undocumented / pre-release Neural Rendering interface and is not affiliated with, endorsed by, or supported by NVIDIA or Comfy Org. Runtime behavior may change with NVIDIA driver or `nvngx_dlssnr.dll` versions. Native driver/runtime failures can crash the ComfyUI process.

## What it does

```text
ComfyUI IMAGE
    -> Python/ctypes
    -> dlss5nr_bridge.dll
    -> D3D12 + NGX
    -> nvngx_dlssnr.dll (feature 18)
    -> ComfyUI IMAGE
```

Current v0.3.1 still uses CPU staging for the ComfyUI tensor transfer:

```text
Torch IMAGE -> CPU float32 -> D3D12 RGBA16F -> DLSS NR -> CPU float32 -> Torch IMAGE
```

There is no subprocess or disk round-trip. CUDA/D3D12 interop is planned as a later optimization.

ComfyUI receives **frame-by-frame progress updates** while an IMAGE batch is processed.

## Temporal mode: NVIDIA Optical Flow

v0.3.1 uses an explicit motion-vector temporal path and adds per-frame motion estimation through the **NVIDIA Optical Flow Accelerator (NVOFA)**.

Only two batch modes remain:

```text
still images
    Reset=1 for every image
    no temporal history
    no motion-vector resource

temporal
    frame 0: Reset=1 + explicit zero R16G16_FLOAT MV
    frame 1+: Reset=0 + NVIDIA Optical Flow MV from raw previous/current input frames
```

Temporal flow is estimated from the **raw input frames**, never from the DLSS-processed output:

```text
previous raw frame ----\
                        > NVIDIA Optical Flow -> current-to-previous MV -> DLSSNR.MVec
current raw frame -----/                                      |
                                                              + DLSS temporal history
```

Implementation details in v0.3.1:

- private D3D11 device on the same NVIDIA DXGI adapter used by the D3D12/NGX bridge;
- driver-provided `nvofapi64.dll` (no separate model/download);
- NVOFA output grid is capability-probed per GPU/driver (prefers **2x2**, falls back to **1x1** or **4x4** when required);
- NVOFA performance level: **FAST**;
- D3D11 input/output surface formats are queried from the driver before allocation, as recommended by NVIDIA's NVOF programming guide;
- input is 8-bit luma using a driver-advertised simple format (`R8_UNORM` is preferred, then `B8G8R8A8_UNORM`, then `R8G8B8A8_UNORM`);
- native NVOFA flow uses driver-advertised `R16G16_SINT`, S10.5 fixed-point (1/32 pixel);
- conversion to full-resolution `R16G16_FLOAT` using nearest-cell reconstruction;
- the DLSSNR MV texture stores normalized UV motion and uses `MVecScale=(width,height)`;
- NVOFA is called with current frame as input and previous frame as reference, producing the current-to-previous reprojection direction used by the temporal contract;
- NVOFA temporal hints remain disabled in v0.3.1 so each frame pair is deterministic and a new ComfyUI batch can reset cleanly.

The first frame has no previous frame, so temporal mode bootstraps it with an explicit zero-MV field.

### NVOF compatibility probing (v0.3.1)

v0.3.0 hard-coded a BGRA8 D3D11 input surface and a 2x2 output grid. v0.3.1 queries `NvOFGetCaps` before `NvOFInit`, then queries `NvOFGetSurfaceFormatCountD3D11` / `NvOFGetSurfaceFormatD3D11` during buffer allocation after initialization, matching NVIDIA's documented D3D11 flow. It prefers native `R8_UNORM` luma when advertised and creates NVOF resources without unnecessary SRV/UAV bindings where possible. If optional queries are unavailable on an older driver, the bridge falls back to the v0.3.0 assumptions rather than breaking a previously working setup.

## Requirements

- Windows 10/11 x64
- NVIDIA RTX GPU
- Recent NVIDIA display driver
- NVIDIA Optical Flow driver API (`nvofapi64.dll`) for `temporal` mode
- A **compatible, legally obtained** `nvngx_dlssnr.dll`
- ComfyUI with Python, PyTorch and NumPy

NVIDIA Optical Flow hardware is available on Turing-generation NVIDIA GPUs and newer; all GeForce RTX generations satisfy that hardware-generation requirement. GPU support for Neural Rendering itself is still determined by the specific `nvngx_dlssnr.dll` you provide.

A `0xBAD00001` result means the Neural Rendering runtime rejected the feature on the current GPU/runtime/driver combination.

## Installation — normal users

**Do not download GitHub's automatically generated `Source code.zip` if you do not want to compile anything.**

1. Open **Releases** for this repository.
2. Download `ComfyUI-DLSS5-NR-vX.Y.Z-windows-x64.zip`.
3. Extract the contained `ComfyUI-DLSS5-NR` folder to:

   ```text
   ComfyUI/custom_nodes/ComfyUI-DLSS5-NR
   ```

4. Supply your own compatible NVIDIA Neural Rendering runtime:

   ```text
   ComfyUI-DLSS5-NR/runtime/nvngx_dlssnr.dll
   ```

5. Restart ComfyUI.
6. Add **experimental -> DLSS 5 NR -> DLSS 5 Neural Rendering (Unofficial)**.

The release ZIP already contains the project-owned native files:

```text
native/bin/dlss5nr_bridge.dll
runtime/caller/nvngx.dll_comfy.dll
```

**Visual Studio / MSVC is not required for normal installation.**

The prebuilt Windows release ZIP intentionally contains only the files needed to run the node. Developer tools, native source code, workflows and extended documentation remain in the repository and GitHub source archives.

### `_nvngx.dll`

Do not normally copy `_nvngx.dll` yourself. The bridge attempts, in order:

1. `runtime/_nvngx.dll` as an explicit local override;
2. normal Windows DLL search;
3. automatic discovery in NVIDIA DriverStore packages matching `nv*.inf_*`.

The project does not redistribute `_nvngx.dll`.

### `nvofapi64.dll`

Do not download or bundle `nvofapi64.dll`. It is supplied by the NVIDIA display driver and is loaded from the normal Windows driver installation when `batch_mode = temporal`.

`still images` does not initialize NVOFA.

## Developer build

Only developers building from source need Visual Studio Build Tools.

Requirements:

- Visual Studio 2022 Build Tools or Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK

Run from a normal `cmd.exe` or PowerShell window:

```bat
build_native.bat
```

The builder locates MSVC and the Windows SDK directly; a Developer Command Prompt is not required.

Outputs:

```text
native/bin/dlss5nr_bridge.dll
runtime/caller/nvngx.dll_comfy.dll
```

The bridge now links the standard Windows `d3d11.lib` in addition to D3D12/DXGI. `nvofapi64.dll` is loaded dynamically at runtime and is not linked or redistributed.

## Node parameters

| Parameter | Purpose |
|---|---|
| `style` | Neural Rendering style selector (`natural`, `cinematic`, `default`, numeric experimental styles). |
| `preset` | Internal render preset. `3` is the current default. |
| `intensity` | Overall Neural Rendering strength. |
| `tone` | Local tone / lighting strength (`DLSSNR.LocalToneStrength`). |
| `structure` | Local structure / micro-detail strength (`DLSSNR.LocalStructureStrength`). |
| `skin` | Skin-specific structure strength. `-1` leaves the runtime's default behavior. |
| `auto_mask` | Enables the runtime's automatic mask path. |
| `batch_mode` | `still images` processes independent frames; `temporal` keeps history and supplies zero/NVIDIA Optical Flow motion vectors. |
| `gpu_index` | NVIDIA DXGI adapter index. The private NVOFA D3D11 device is created on the same adapter. |
| `channel_order` | `auto`, `RGBA`, or `BGRA`; useful because different runtime builds have exposed different R/B ordering. |

Output resolution is identical to input resolution. This node is Neural Rendering/post-processing, not DLSS Super Resolution.

## Runtime Info node

`DLSS 5 NR Runtime Info` reports:

- plugin version;
- native bridge version;
- selected GPU name/index;
- whether the NVIDIA Optical Flow D3D11 API is visible;
- temporal NVOFA grid/performance settings;
- runtime directory;
- size and SHA-256 of the supplied `nvngx_dlssnr.dll`.

Use it first when diagnosing a new runtime/GPU combination.

## Troubleshooting

### `Native bridge is missing`

If you are a normal user, you probably installed GitHub's **Source code** archive instead of the prebuilt Windows release ZIP. Download the release asset. Developers can run `build_native.bat`.

### `nvngx_dlssnr.dll not found`

Place your compatible runtime at:

```text
runtime/nvngx_dlssnr.dll
```

This repository intentionally does not provide download links or mirrors for proprietary/leaked NVIDIA binaries.

### `Could not load NVIDIA NGX core _nvngx.dll`

The bridge searches NVIDIA DriverStore automatically. As a diagnostic fallback:

```powershell
Get-ChildItem "$env:SystemRoot\System32\DriverStore\FileRepository" -Filter _nvngx.dll -Recurse -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object FullName, LastWriteTime, Length
```

A matching driver copy can be placed at `runtime/_nvngx.dll` as a local override. Do **not** rename `nvngx_dlss.dll`; it is a different component.

### `NVIDIA Optical Flow: could not load nvofapi64.dll`

This only affects `temporal` mode. Verify that a recent NVIDIA display driver is installed. Run `diagnose_runtime.bat`; it reports the driver copy under `%SystemRoot%\System32` when present.

Do not download `nvofapi64.dll` from third-party DLL sites.

### `NVIDIA Optical Flow: nvOFInit / nvOFExecute failed`

Attach the complete ComfyUI exception and `DLSS 5 NR Runtime Info` output to an issue. v0.3.1 reports the driver-advertised D3D11 input/output formats, selected BindFlags, supported output grids, selected format/grid, NVOF maximum API version, session resolution, raw status code, and the driver's own last-error text when available.

v0.3.1 fixes a confirmed compatibility failure where frame 0 (zero MV) succeeds but frame 1 fails on the first real `NvOFExecute`. The fix was validated both on a locally reproducible failing input and by the reporter of issue #5.

### `CreateFeature(18) failed: 0xBAD00001`

Feature not supported by the supplied Neural Rendering runtime on the selected GPU/driver combination. Use a runtime that legitimately supports your configuration.

### `0xBAD00002`

The observed Neural Rendering snippet rejected the caller. The release includes the project-owned thin caller helper expected by this integration. If this occurs with an untouched release ZIP, open an issue and attach the full ComfyUI error log plus the `Runtime Info` output.

### Wrong / blue colors

Try `channel_order = RGBA` or `BGRA`. `auto` compares both interpretations against the source and normally selects the plausible one.

### Old workflow says `batch_mode` is invalid

v0.3.1 uses only `still images` and `temporal`; the older `temporal sequence` / `temporal sequence (legacy no MV)` values are no longer valid. Delete and re-add the node, or change the saved widget to one of the two current values:

```text
still images
temporal
```

## GitHub Releases

Tags matching `v*` trigger the Windows release workflow. GitHub Actions:

1. checks that NVIDIA proprietary DLLs were not committed;
2. builds the native bridge, NVIDIA Optical Flow integration and caller helper on `windows-latest`;
3. assembles a clean user ZIP with the prebuilt project-owned DLLs;
4. writes a SHA-256 checksum;
5. creates the GitHub Release.

See [`docs/PUBLISHING.md`](docs/PUBLISHING.md).

## References / implementation notes

Useful references include:

- NVIDIA DLSS / NGX public repository: https://github.com/NVIDIA/DLSS
- NVIDIA Optical Flow SDK public repository: https://github.com/NVIDIA/NVIDIAOpticalFlowSDK
- NVIDIA Optical Flow programming guide: https://docs.nvidia.com/video-technologies/optical-flow-sdk/nvofa-programming-guide/index.html
- Zonnery experimental DLSS 5 NR player/reference: https://github.com/Zonnery/dlss5-nr-player
- NIGos DLSS5 bridge: https://github.com/NIGos/dlss5-bridge
- NVIDIA RTX nodes for ComfyUI: https://github.com/Comfy-Org/Nvidia_RTX_Nodes_ComfyUI

The NVOF D3D11 function-table integration and measured flow-direction/normalization behavior were informed by the MIT-licensed NIGos `dlss5-bridge`; attribution is retained in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

No NVIDIA runtime DLLs or NVIDIA SDK headers are vendored in this repository.

## Legal / licensing

The project's original source code is MIT licensed. NVIDIA trademarks, SDK/API names and proprietary binaries are owned by NVIDIA and are **not** covered by this project's MIT license.

This project does not redistribute:

- `_nvngx.dll`
- `nvngx_dlssnr.dll`
- `nvofapi64.dll`
- other NVIDIA runtime binaries
- NVIDIA NGX/Optical Flow SDK headers

The integration uses undocumented/pre-release Neural Rendering behavior, including a thin caller-forwarding helper and observed project/application identifiers. Before distributing or using this project, review the terms that apply to the NVIDIA software/runtime you use. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) and [`docs/LEGAL_NOTES.md`](docs/LEGAL_NOTES.md).

## Status

v0.3.1 is the stable maintenance release following v0.3.0. It adds NVOF capability/surface-format probing and improved diagnostics for driver/GPU/input combinations that can fail on the first real optical-flow execute. The compatibility fix was validated on multiple systems, including the configuration reported in issue #5. The integration remains unofficial and experimental with respect to the NVIDIA Neural Rendering interface.
