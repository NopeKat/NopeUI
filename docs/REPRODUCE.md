# Reproducing the Build on a Clean Machine

This document describes how to build, verify, and run NopeUI from source on a clean Windows machine. Every step is a manual command run locally; the project has no CI.

## 1. Prerequisites

- Windows 10 or Windows 11, x64. Only the x64 target exists right now.
- PowerShell 7 (`pwsh`); the 5.1 that ships with Windows is not guaranteed to work.
- Visual Studio 18 2026 with the "Desktop development with C++" workload installed.
- Windows SDK 10.0.26100.0 (check "Windows 11 SDK (10.0.26100.0)" in the VS installer).
- CMake 4.4.4. The generator is fixed to `Visual Studio 18 2026`; Ninja is not used.

The toolchain version is a hard requirement: `tools/scripts/toolchain.ps1` checks whether `cmake --version` reports 4.4.4 and fails immediately otherwise. It never downgrades or downloads anything on its own.

CMake can be installed through winget:

```powershell
winget install --id Kitware.CMake --version 4.4.4
```

If the `cmake.exe` on PATH is the wrong version, point at the right one with an environment variable:

```powershell
$env:NOPEUI_CMAKE = 'C:\Program Files\CMake\bin\cmake.exe'
```

## 2. Getting the Source

```powershell
git clone git@github.com:NopeKat/NopeUI.git
cd NopeUI
git checkout main
```

The repository contains only the inner source tree. The workspace repository (design docs, roadmap) is not part of the build, and not having it changes nothing.

## 3. Building

```powershell
.\tools\scripts\build.ps1 -Preset x64-release
```

The script configures the `x64-release` preset from `CMakePresets.json` into `.build/x64-release` and then compiles the Release configuration. The compiler flags are `/W4 /WX /permissive- /utf-8`, so any warning breaks the build. That is intentional.

Where the outputs land:

- Samples: `.build\x64-release\samples\Release\nopeui_window_smoke.exe`, `nopeui_xaml_smoke.exe`
- Tools: `.build\x64-release\tools\Release\nopeui_golden.exe`, `nopeui_image_diff.exe`, `nopeui_xaml_fuzz.exe`
- Benchmarks: `.build\x64-release\samples\Release\nopeui_startup_benchmark.exe`, `nopeui_theme_benchmark.exe`
- Unit tests: `.build\x64-release\tests\Release\`

For a Debug build, switch `-Preset` to `x64-debug`; the two presets use separate directories and do not interfere with each other.

## 4. Smoke Run

```powershell
.\.build\x64-release\samples\Release\nopeui_window_smoke.exe
.\.build\x64-release\samples\Release\nopeui_xaml_smoke.exe
```

The first should open a 900x640 form window whose title shows the most recent frame time. The second loads `page.xaml` and titles the window `NopeUI XAML Smoke`. Both windows should drag, resize, and switch themes normally.

The target machine needs the VC++ runtime (installed together with VS; a clean preview machine needs the x64 runtime installed separately).

## 5. Verification

Run these in order and keep the output of each one in the report.

```powershell
.\tools\scripts\test.ps1 -Preset x64-release        # 18 unit tests
.\tools\scripts\golden.ps1 -Preset x64-release      # 28 golden images compared against baselines
.\tools\scripts\uia.ps1 -Preset x64-release         # full UIA walkthrough, 44 checks
.\tools\scripts\perf.ps1 -SkipBuild -Compare        # startup and theme switching against the baseline
.\tools\scripts\analyze.ps1                          # MSVC /analyze, report-only by default
```

A few notes:

- Golden baselines live in `tests/golden/baseline`; current and diff images are written to `.build/golden`. When a different GPU or scale factor causes mismatches, look at the diff image first instead of running `-Update` right away.
- All 28 baselines live in the workspace `assets/golden` and none are in the repository. When you clone the inner repository on its own you must point `NOPEUI_GOLDEN_ASSETS` at a baseline directory, otherwise every case reports a missing baseline.
- `tools/scripts/uia.ps1` starts `nopeui_window_smoke` itself and requires an interactive desktop session; on a remote or purely virtual display, raise `-TimeoutSeconds`.
- `tools/scripts/perf.ps1 -Compare` only compares numbers when `tests/perf/baseline.json` matches on both preset and render path. On a different machine, regenerate the baseline with `-Out` first.
- `tools/scripts/analyze.ps1` does not return a non-zero exit code by default even when it fails; to use it as a gate, pass `-MaxWarnings 0`.
- Fuzzing: `.\tools\scripts\fuzz.ps1 -SkipBuild -Hours 0.05` runs three minutes first to confirm the tool works, `-Hours 24` is for the real regression run, and crash samples land in `.build/fuzz/crash.xaml`.
- Coverage needs OpenCppCoverage installed externally; the script does not download it and explains how to install it when the tool is missing.

## 6. Common Issues

- **CMake version mismatch**: the error lists the version it found. Install 4.4.4 as described in section 1, or set `NOPEUI_CMAKE`.
- **LNK1104 cannot open the exe**: a sample process from an earlier run is still alive. Run `Stop-Process -Name nopeui_window_smoke` before rebuilding.
- **Golden images fail the pixel comparison but look identical**: usually display scaling or driver antialiasing differences. Inspect the diff region with `nopeui_image_diff` before deciding whether to regenerate the baseline.
- **The UIA script cannot find the window**: make sure the desktop session is interactive and no other process with the same name is holding it.

## 7. Preview Packaging

```powershell
.\tools\scripts\package.ps1                     # build and package
.\tools\scripts\package.ps1 -SkipBuild -Smoke   # reuse the existing build and run a 3-second smoke test
```

The script requires a clean working tree: the source archive inside the package comes from `HEAD`, so the source has to match the binaries. For local experiments you can pass `-AllowDirty`, which clearly warns that the source archive comes from `HEAD`.

Artifacts land in `.build\package`:

- `NopeUI-<version>-win-x64.zip`: portable package, unzip and run.
- `NopeUI-<version>-win-x64.sha256`: checksums for the package and the two sample executables.

Package layout:

```
NopeUI-0.0.0.0-win-x64/
├─ include/nopeui/          public headers
├─ lib/x64/static/md/       seven static libraries, dynamic CRT build
├─ bin/x64/                 the two sample executables and page.xaml
├─ docs/REPRODUCE.md
├─ source/nopeui-<version>-source.zip   source archive for HEAD
├─ package-info.txt         version, build date, variant, and toolchain
├─ LICENSE
└─ README.md
```

The version is read from `src/base/version.cpp` and is not passed as an argument. `page.xaml` in `bin\x64` must sit next to the executables, because `nopeui_xaml_smoke` looks for it in the same directory.

This preview release ships only the x64 static-library `/MD` variant. Dynamic libraries, static CRT, and ARM64 belong to a quarterly all-variant SDK that is not built yet.
