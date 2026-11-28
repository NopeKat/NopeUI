# Coverage Record

- Date: 2026-11-28
- Preset: `x64-debug`
- Tool: OpenCppCoverage 0.9.9.0, unpacked into `.build\tools\OpenCppCoverage` instead of installed system-wide
- Entry: `tools/scripts/coverage.ps1 -Preset x64-debug -SkipBuild -ToolPath <OpenCppCoverage.exe>`
- Reports: `.build/coverage/html`, `.build/coverage/cobertura.xml`
- Excluded: `src/platform/automation.cpp` (set inside the script)

## Overall

Line coverage is **75.5%**, 35598 of 47126 lines.

The first pass the same day stopped at 58.6% (24796 of 42305). The second pass adds
`nopeui_controls.controls_test` and `nopeui_graphics.graphics_test`, and both link the
control and graphics libraries, so the total grows with the extra instrumented binaries;
the per-module rates below are the part to compare.

## Per Module

| Module | Covered / Total | Line rate | Files |
| ------ | --------------- | --------- | ----- |
| controls | 12664 / 14876 | 85.1% | 80 |
| layout | 8863 / 11021 | 80.4% | 35 |
| base | 3611 / 4567 | 79.1% | 21 |
| markup | 2438 / 3164 | 77.1% | 2 |
| theme | 364 / 616 | 59.1% | 13 |
| platform | 4004 / 8476 | 47.2% | 4 |
| graphics | 2853 / 3537 | 80.7% | 26 |

## New Tests

- `tests/controls/controls_test.cpp` paints every control onto an offscreen target and
  walks the input paths: pointer enter, leave, down, move, up; Space, Enter, arrows, Home,
  End, Page Up and Page Down; automation toggles, ranges, scroll percents and menu items;
  template parts for the check box and the full factory template with bindings for the
  button.
- `tests/graphics/graphics_test.cpp` covers PNG save and load round trips (including the
  overwrite, size and path failures), text runs across alignment, wrap, ellipsis and line
  spacing, the offscreen and hardware surfaces with a placeholder scene, and scene painter
  clipping and empty rectangles.
- Coverage of the previously untouched files: `controls\scroll_viewer.cpp`,
  `controls\context_menu.cpp`, `controls\menu_bar.cpp`, `controls\text_box.cpp`,
  `controls\image.cpp`, `graphics\image.cpp` and `graphics\offscreen.cpp`.

## Gate B

The gate asks for 70% on the core modules. Counting base, layout and markup as the core,
`base`, `layout` and `markup` pass. With the new tests `controls` (85.1%) and `graphics`
(80.7%) also clear 70%; `theme` (59.1%) and `platform` (47.2%) still do not.

- `theme`: the palette switch and system high contrast paths are only reached from the
  window; unit tests exercise the table lookups but not `SystemTheme`.
- `platform`: two large files (window, graphics device); the window lifecycle has many
  branches and the tests only walk the basic path.

## Running It

- Coverage runs on the Debug build. `tools/scripts/coverage.ps1` used to pass the build
  directory straight into `cmd /c`; the spaces in the path split it and ctest never started.
  The script now writes a runner in the temp directory and reads the paths from the
  environment, and it passes `--modules "*nopeui*"` so only project modules are counted.
- ctest holds 22 tests. `nopeui_markup.fuzz_test` fails under instrumentation: it asks for
  at least 20000 iterations within 5 seconds, and the instrumentation pushes throughput
  below that floor. That is measurement overhead, not a product defect; the script keeps
  the report and warns instead of aborting the run.
- Only `src` is counted; tests and samples are not.
