# NopeUI

NopeUI is an open-source C++ desktop UI framework for Windows only. Its appearance follows Fluent, with interaction and performance taking priority; the API and control names are independent, and WinUI compatibility is not a goal.

The target platform is Windows 10 1809 and later, on x64 and ARM64. Windows and presentation use DWM and DirectComposition, text uses DirectWrite, and IME handling uses Win32; layout, painting, controls, shell, XAML, and the designer are all built by the framework itself. The width and height passed to `Window::Create` are treated as logical DIPs, and the window's pixel size scales with the DPI of the display it is on; a new DPI delivered through `WM_DPICHANGED` rebuilds the surface and repaints immediately instead of waiting for a display query after the move finishes.

## Current state

The project is in early P1, and the P0 loop covering `base`, `platform`, `layout`, `controls`, and accessibility is complete: `Result` and `Error`, `Ref`, `Event` and `Subscription`, the geometry types, UTF-8 and UTF-16 conversion, plus Win32 windows, the message loop, DirectComposition virtual surfaces, Direct2D drawing, and DirectWrite text. A window shows a placeholder scene by default - a light background, an accent strip along the top, and a single line of sample text; `Window::SetContent` attaches an element tree that covers the client area and takes over mouse hit testing. Windows enable area-aware async commit with 2-3 surfaces by default: 3 surfaces when the client area is at most `2560x1600`, and 2 for larger windows to keep video memory in check. `SetAsyncCommit(false)` falls back to synchronous commit, and `LastPresentedFrame()` and `AsyncCommitStats()` expose commit latency, queue depth, and dropped frames. During a resize, virtual surfaces are allocated with a margin of roughly one third for small windows and a fixed 256 pixels for larger ones, then compacted to the actual client size 300 ms after the resize stops, so the surface is not rebuilt on every step of a drag. Frame commits are throttled to the display refresh interval by default; `Window::Present()` commits immediately, and `Window::SetCommitThrottle(false)` turns throttling off. `Window::LastFrameMilliseconds()`, `Window::FrameCount()`, `Window::CommitCount()`, and `Window::RenderFrame()` read and drive frames; `nopeui_frame_benchmark` provides a 300-frame benchmark and `nopeui_resize_benchmark` a resize benchmark. The software fallback works: remote sessions switch to WIC software rasterization automatically, `Window::SetSoftwareRender(true)` switches manually, and software and hardware images match pixel for pixel in the golden images. Golden image baselines and tools: `nopeui_golden` renders, `nopeui_image_diff` compares, and `tools/scripts/golden.ps1` manages. `layout` provides a minimal element tree with Measure/Arrange/HitTest/Paint; `controls` has landed `Border`, `Grid`, `Label`, `Button`, `StackPanel`, `Canvas`, `DockPanel`, `FlexPanel`, `ScrollViewer`, `CheckBox`, `RadioButton`, `ToggleButton`, `ToggleSwitch`, `TextBox`, `ProgressBar`, `Slider`, `Image`, and `MenuBar`, plus `ToolTip`, `Flyout`, and `ContextMenu` for popups; the default UI of `nopeui_window_smoke` is a single form page assembled from `Grid` with a `MenuBar` on top, implemented in `samples/common/form_page.h`, and the `form` scene in the golden images uses the same tree. `Theme` defines light, dark, and high contrast palettes; windows follow the system app light/dark preference by default and `SetTheme` overrides it; elements inherit the theme along the parent chain, controls take their colors from it, and the placeholder scene's palette follows the theme as well. All 18 tests pass under both `x64-debug` and `x64-release`.

Property metadata lives in `property_value.h`: `PropertyValue` carries booleans, integers, floats, colors, strings, and `Thickness` in a `std::variant`, and `PropertyMetadata` records the name, type, default value, and the `AffectsMeasure`, `AffectsArrange`, `AffectsRender`, and `Inherits` flags. `Element` provides `Properties`, `GetProperty`, `SetProperty`, and `SubscribePropertyChanged`; the base class always exposes `IsEnabled`, the inheritable `Foreground`, and the layout set of `Margin`, `Width`, `Height`, `MinWidth`, `MinHeight`, `MaxWidth`, `MaxHeight`, `HorizontalAlignment`, and `VerticalAlignment`. `Foreground` walks up the parent chain and finally falls back to the theme text color. Each control exposes its enumerable properties through its own property table; `SetProperty` validates the type, raises a change event, and then triggers a repaint or relayout according to the flags.

Layout follows the slot model: `Measure` first subtracts `Margin`, clamps the available space by the explicit size and the min/max constraints, then hands it to `MeasureOverride`; when writing back it adds `Margin` again and records the desired content size. `Arrange` takes the slot from the parent, computes the final rectangle from `Margin`, the explicit size, and horizontal/vertical alignment, then enters `ArrangeOverride`. Alignment defaults to `Stretch`, so with zero margins the result matches the previous behavior. `StackPanel` gives the full slot on the cross axis and lets each child's own alignment decide placement, so a single-line text box, a slider, and a progress bar naturally fill a column.

`Grid` measures in three passes: columns first, then rows, then arrangement. The `Width`/`Height` of `ColumnDefinition` and `RowDefinition` accept pixels, `Auto`, and `*` star sizing, with leftover space distributed by weight; `Auto` only accounts for elements in a single cell, and spanning content does not participate in track sizing, so a spanning title cannot stretch the label column. `Grid.Row`, `Grid.Column`, `Grid.RowSpan`, and `Grid.ColumnSpan` are attached properties that can be set on any child, including directly as attributes in XAML.

Property precedence is local value first: `SetProperty` records a local value, and styles write through `ApplyStyleProperty` during `Style::Apply` without overwriting an existing local value - the style value is still recorded and takes over as soon as `ClearValue` removes the local one. `ClearValue` falls back in the order active trigger value -> style value -> metadata default, and it is the only way to hand a property back to styles and triggers.

`Canvas` positions by `Canvas.Left` and `Canvas.Top`, measures with unbounded space, and takes the maximum child outer edge as its desired size; arrangement adds the panel origin back to the offset, so child coordinates stay absolute like in any other container and do not drift inside a padded card. `DockPanel` peels one edge off per child according to the `Dock` attached property (`Left`, `Top`, `Right`, `Bottom`, values 0-3), and `LastChildFill` lets the last child take the remainder by default. Neither clips: content past the bounds still paints. `FlexPanel` lays out along `Direction`, with `Gap` as spacing; with `Wrap` on, it wraps once the main axis runs out. Children claim leftover space through `FlexGrow` by weight; without wrapping a single row fills the cross axis, and with wrapping each row only takes its own height.

Resources and theme resources share one lookup: `Element::SetResource` writes the local table, and `FindResourceValue` checks the element itself, then the parent chain, then the `Theme` palette keys, so layered overrides of the same name come naturally. `{ThemeResource Key}` binds to any property whose type matches a color, number, `Thickness`, and so on; the value applies at bind time, keys not yet declared during loading are refreshed once by the loader after the tree is built, and colors follow `Window::SetTheme` afterward. `PropertyTrigger` watches one property and writes its own setters when the value equals `Value`; style setters remember their original values and yield to the trigger, and when the condition no longer matches they are filled back from the style value or the metadata default, while a local value still beats the trigger. `ControlTemplate` can carry `VisualStateGroup`s, where each state is a set of part patches; `Button` picks part colors for `Normal`, `PointerOver`, `Pressed`, and `Disabled`, template colors first and theme state colors otherwise.

Styles and templates form a minimal loop: `Style` matches on target type and an optional key and stores `PropertyValue` setters plus an optional template; `StyleSheet::ApplyTo` applies recursively, setting properties first and then applying the template. `ControlTemplate` can store named parts or a visual tree; `TemplatePart` carries background, foreground, stroke, corner radius, and padding. `Button` consumes both the old part template and the visual tree template, and `CheckBox` already consumes `Box`, `Check`, and `Content`, which shows templates are not just stored and ignored.

Styles can also be removed: `Element::ClearStyleValues()` drops the style values this element remembers, and `Element::RemoveTriggers(style)` detaches triggers by source; both let properties fall back to local values, remaining triggers, or metadata defaults. `Style::RemoveTrigger(index)` removes one trigger on the style object so a style can be reused. Local values of attached properties are governed by `ClearValue` as well: `Canvas.Left` set and then cleared goes back to 0.

Popups are hosted by the window's built-in overlay root rather than pushed into the content tree: `ShowPopup`, `ClosePopup`, and `HasPopup` manage anchored panels, and measure, arrange, paint, hit testing, and focus all prefer the overlay; Esc, an outside click, and an outside wheel close it. A right click walks the parent chain of the hit element to find a `ContextMenu` and opens it; oversized menus scroll with the wheel and the keyboard. `ToolTipService` registers text keyed by element and shows it after a 600 ms hover; `Flyout` is a single-content rounded panel. Closing goes through a posted message so a control does not release itself inside its own `OnPointerUp` callback.

`Image` reuses the WIC decode path in `graphics::LoadPng` and offers four stretches: `None`, `Fill`, `Uniform`, and `UniformToFill`. At paint time it caches an `ID2D1Bitmap` per render target, converting straight alpha to premultiplied while caching, and the software and hardware paths share the same source rectangle math.

## Building

Toolchain versions are pinned to CMake 4.4.4, Visual Studio 18 2026, MSVC 14.52.37025, and Windows SDK 10.0.26100.0; the generator is `Visual Studio 18 2026`, and Ninja is not used.

```powershell
.\tools\scripts\build.ps1 -Preset x64-debug
.\tools\scripts\test.ps1 -Preset x64-debug
```

`-Preset` accepts `x64-debug` or `x64-release`. The scripts prefer CMake from PATH; when it is missing, they locate the winget-installed CMake 4.4.4 automatically. `NOPEUI_CMAKE` can point at a specific `cmake.exe`.

The full steps for reproducing from scratch on a clean machine are in `docs/REPRODUCE.md`, covering toolchain installation, building, the smoke run, and each check command. The preview package is produced by `.\tools\scripts\package.ps1`, which writes `.build\package\NopeUI-<version>-win-x64.zip` and a matching checksum file.

The frame benchmark runs at a fixed 120 Hz cadence by default and only counts frames that actually `Commit`; `--unpaced` drops the cadence, `--full` runs whole frames, `--no-throttle` turns commit throttling off, and `--async-commit` enables async commit. `--rects N` and `--text N` generate synthetic heavy content; by default the same small region is drawn repeatedly, `--virtual` switches to a row-unrolled virtual long list that only draws rows inside the current content area, and `--scroll N` moves that list N pixels per frame to exercise the clipping path during scrolling. The benchmark disables async commit by default so it can be compared against the synchronous path; a running window uses async commit by default. The output adds `queue`, `present`, and `submitted/completed/dropped/waits` statistics, where `present` is the latency from commit to display estimated from the DWM vblank:

```powershell
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe --full
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe --full --async-commit --text 100000 --virtual
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe --full --async-commit --text 100000 --virtual --scroll 4
.\.build\x64-release\samples\Release\nopeui_resize_benchmark.exe
.\.build\x64-release\samples\Release\nopeui_resize_benchmark.exe --sync
.\.build\x64-release\samples\Release\nopeui_resize_benchmark.exe --interval 16
.\.build\x64-release\samples\Release\nopeui_resize_benchmark.exe --interval 16 --flip
.\.build\x64-release\samples\Release\nopeui_text_list_benchmark.exe --rows 100000 --scroll 4
.\.build\x64-release\samples\Release\nopeui_text_list_benchmark.exe --rows 100000 --scroll 4 --cache
.\.build\x64-release\samples\Release\nopeui_text_list_benchmark.exe --rows 100000 --scroll 4 --cache --width 3840 --height 2160
.\.build\x64-release\samples\Release\nopeui_text_list_benchmark.exe --rows 100000 --scroll 4 --cache --width 3840 --height 2160 --surfaces 2
.\.build\x64-release\samples\Release\nopeui_text_list_benchmark.exe --rows 100000 --scroll 4 --cache --width 3840 --height 2160 --client
.\.build\x64-release\samples\Release\nopeui_text_list_benchmark.exe --rows 100000 --scroll 4 --cache --width 1920 --height 1080 --client --flip
.\.build\x64-release\samples\Release\nopeui_present_benchmark.exe --width 3840 --height 2160
.\.build\x64-release\samples\Release\nopeui_present_benchmark.exe --width 3840 --height 2160 --flip
```

`nopeui_text_list_benchmark.exe` generates distinct text for the given number of rows, virtualizes by row height, and creates `IDWriteTextLayout` only for the current viewport; `--cache` turns on a layout cache of up to 512 entries. `--scroll` is the scroll distance per frame, `--width/--height` sets the window size, `--client` sizes by the client area instead of the frame, `--surfaces 2|3` forces the async surface count for comparison, `--sync` falls back to synchronous commit, and `--flip` switches to the experimental flip-model swap chain path; the output includes the requested size, the actual client area, DPI, awareness, surface count, async state, present mode, and the average and P99 for `begin/draw/end` and `commit/queue/present`.

`nopeui_resize_benchmark.exe --interval <ms>` waits the given number of milliseconds after each resize step to mimic the rhythm of a real drag; `--interval 0` is a continuous no-interval stress test, `--flip` uses the experimental flip-model path, and `--sync` falls back to synchronous commit.

`nopeui_present_benchmark.exe` compares the commit API cost of DirectComposition virtual surface `Commit` against flip-model swap chain `Present` in a separate popup window; the default is virtual, `--flip` switches to flip, and `--width/--height` sets the surface size. It measures only the commit call, without full control painting.

`nopeui_window_smoke.exe --trace <csv>` writes frame, commit, and latency statistics during interaction for watching drags and resizes; `--sync` switches to synchronous commit for comparison.

## Golden images

`nopeui_golden` renders placeholder scenes offscreen into a WIC bitmap as a Direct2D target, with grayscale antialiased text fixed, so the output does not depend on the display configuration; `--renderer hardware` goes through a D3D11 texture plus a staging texture readback to compare the two paths. `tools/scripts/golden.ps1` builds, renders, and compares:

```powershell
.\tools\scripts\golden.ps1                              # build and compare against the existing baselines
.\tools\scripts\golden.ps1 -Update                      # rebuild the baselines
.\tools\scripts\golden.ps1 -Preset x64-debug -SkipBuild  # compare only
```

Baselines live in `tests/golden/baseline`, except the two hardware placeholder baselines, which sit in the workspace `assets/golden`; `NOPEUI_GOLDEN_ASSETS` points the script at another directory. Current and diff images go to `.build/golden`, 28 in total. The placeholder scene has light and dark software baselines in the repo and light and dark hardware baselines in the shared directory; the controls tree has light, dark, and high contrast software baselines plus a high contrast hardware baseline; the media scene has light and dark baselines for both renderers; the form scene has light, dark, and high contrast sets, each with a hardware counterpart; the panel scene (`Canvas`, `DockPanel`, `FlexPanel`) has light, dark, and high contrast baselines for both renderers; the XAML scene has light and dark baselines for both renderers. The script cross-compares the software and hardware images of the placeholder, media, form, and XAML scenes in light and dark, the panel scene in light, dark, and high contrast, and the high contrast controls and form pairs. `nopeui_image_diff` judges by a 2-level per-channel color difference within a 0.5% overall budget, and the diff image marks changed pixels in red. Both tools can also be called directly:

```powershell
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\basic.png --theme light --renderer software
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\controls.png --scene controls --theme dark --renderer software
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\form.png --scene form --theme light --renderer software
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\media.png --scene media --theme light --renderer hardware
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\xaml.png --scene xaml --xaml samples\xaml_smoke\page.xaml --theme dark --renderer software
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\panels.png --scene panels --theme dark --renderer software
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\contrast.png --scene form --theme contrast --renderer software
.\.build\x64-release\tools\Release\nopeui_image_diff.exe <baseline.png> <current.png> --diff <diff.png>
```

Both the hardware and software paths in a window now paint through `ScenePainter` in `graphics/scene`, so the placeholder scene has a single implementation; `ScenePainter` caches brushes and text layouts and only fills rectangles, draws text, and paints the accent strip each frame.

## Performance baselines

`nopeui_startup_benchmark` measures from process creation to the first committed frame and reports the loader, window creation, content construction, `Show`, and the commit wait separately, along with peak working set and peak private commit. `nopeui_theme_benchmark` switches light and dark repeatedly on the form page and reports `SetTheme` (including relayout, paint, and commit) separately from waiting for async completion; it disables commit throttling by default so vblank does not leak into the samples.

```powershell
.\tools\scripts\perf.ps1                                          # build and run one group, print a summary
.\tools\scripts\perf.ps1 -Runs 5 -Switches 200 -Out tests\perf\baseline.md
.\tools\scripts\perf.ps1 -SkipBuild -Compare                      # compare against the baseline, exit 1 when over budget
.\.build\x64-release\samples\Release\nopeui_startup_benchmark.exe --no-content
.\.build\x64-release\samples\Release\nopeui_theme_benchmark.exe --switches 200 --contrast
```

Startup `total` includes process creation and the loader; `--no-content` substitutes the placeholder scene, and `--sync` and `--software` override synchronous commit and software rasterization respectively. The first baseline group is in `tests/perf/baseline.md`: the 900x640 form page starts in 256 to 344 ms with a peak working set around 48 MB; theme switching averages 8.9 ms with a P99 of 25.4 ms.

`-Out` also writes a `baseline.json` of the same name, `-Compare` reads only that, and `-Baseline` points elsewhere. Comparison first checks the preset and render path and fails outright on a mismatch, then holds two lines: the startup median at most 1.25x the baseline and at most 800 ms, and the max at most 1.5x; theme switching and its P99 each at most 1.5x and at most 50 ms; peak working set on both sides at 1.2x with an 80 MB cap. Any line over budget prints `FAIL` per item and exits with 1; a full pass prints `PASS: performance stayed within budget`.

## Layout and controls

The element tree does exactly four things: `Measure` computes the desired size from the available size, `Arrange` settles the final rectangle, `HitTest` finds the topmost hit element, and `Paint` is called uniformly by the window on both the hardware and software paths. `StackPanel` supports vertical and horizontal stacks, `Grid` places children on row and column tracks with spans and attached properties accounted for, `Border` handles background, a 1-pixel stroke, padding, and an optional corner radius, and `ScrollViewer` provides horizontal and vertical scrolling for a single child. `Label`, `Button`, `CheckBox`, `RadioButton`, `ToggleButton`, `ToggleSwitch`, `TextBox`, `ProgressBar`, and `Slider` share text layouts cached by `graphics::TextRun`; `Button` has a corner radius, hover and pressed backgrounds, a focus rectangle, and a click callback, `CheckBox` and `ToggleButton` keep a checked state and toggle on click or Enter/Space; `RadioButton`s under the same parent are mutually exclusive, and `ToggleSwitch` toggles on Space; `ProgressBar` supports determinate and indeterminate values, and `Slider` supports mouse drag, arrow keys, Home/End, and PageUp/PageDown. After `SetEnabled(false)`, a control takes colors from `ControlState::Disabled` and drops out of hit testing, focus, and the wheel.

`TextStyle` controls font size, weight, family, alignment, line height multiple, wrapping, and trimming. A wrapping `Label` rebuilds its text layout at the available width and grows in height, trimming uses DirectWrite trailing trimming, and centered or right-aligned text fills the available width; layout changes go back through `InvalidateLayout` so the window re-measures and re-arranges, while a plain color change only triggers a repaint. `TextBox` is a single-line editor supporting character input, a caret, drag selection and Shift selection, Ctrl+A/C/X/V, Backspace/Delete, arrow keys, Home/End, and Enter to commit, with long text scrolling horizontally to follow the caret; IME composition shows as underlined preview text and is written to the text only after it is committed, and `SetPlaceholder` supplies a hint for empty content.

Containers push a clip region from their own bounds before painting children, and `Border`, `StackPanel`, and `ScrollViewer` all do this. `ScrollViewer` shows 6-pixel thin scroll bars on demand when content overflows, thickening to 10 pixels when the pointer is near, drags both horizontally and vertically, and hides them when the content fits; the wheel bubbles from the hit element up to the nearest scrolling container, stepping by the system lines-per-scroll setting, one screen when set to scroll a page, and horizontally when Shift is held or only horizontal overflow exists. PageUp/PageDown/Home/End bubble to the scrolling container when the focused control does not handle them, and those keys scroll horizontally when only horizontal overflow exists. When focus moves to a control outside the viewport, the window calls `BringChildIntoView` along the parent chain and the nearest scrolling container scrolls that control into the viewport.

Focus is ordered by visual order: `IsFocusable()` marks focusable elements, `CollectFocusable()` collects them recursively, and the window maintains Tab and Shift+Tab. When the window loses activation it keeps the logical focus, closes popups, and cancels presses, so typing can resume directly after reactivation. Disabled elements and their subtrees stay out of the focus order. `Button::SetOnClick` stores a single callback, and `SubscribeClick` returns a manageable subscription.

Accessibility goes through a UI Automation server provider: the window answers `WM_GETOBJECT`, the root is a Window, and controls expose Name, ControlType, BoundingRectangle, IsEnabled, and focus state along the element tree. `Button`, `ToggleButton`, `CheckBox`, and `RadioButton` provide Invoke or Toggle patterns, `TextBox` provides Value and Text patterns, `Slider` and `ProgressBar` provide RangeValue, `ScrollViewer` provides Scroll, and focus changes raise UIA focus events. Menu rows in `MenuBar` and `ContextMenu` are painted data rows, so on the UIA side they get a separate child provider layer with the `MenuItem` role, attached at the first child slot of the menu element; it walks siblings up and down, and the name is the menu title or item text.

`tools/scripts/uia.ps1` is a manual walkthrough: it starts `nopeui_window_smoke`, walks the whole tree with UIA, checks control types, labels, and menu items, then reads and writes Value, Toggle, RangeValue, Invoke, and ExpandCollapse patterns; menus must expand, items must fire and close, and any failure returns nonzero. `tools/scripts/analyze.ps1` configures a separate `.build/x64-analyze` with `-DNOPEUI_ANALYZE=ON`, compiles with `/analyze` and `/WX` off, collects all warnings into `.build/analyze.log`, and summarizes them by code; by default it only reports, and passing `-MaxWarnings` makes it a gate. `tools/scripts/fuzz.ps1` runs `tools/xaml_fuzz`, restarting the process every half hour, and on a crash it leaves the input in `.build/fuzz/crash.xaml` and returns nonzero; the default run is 24 hours. `tools/scripts/coverage.ps1` runs `ctest` under OpenCppCoverage and writes HTML and Cobertura output; the tool is not in the repo, and when it is missing the script explains how to install it instead of downloading anything. None of these scripts run in any automation pipeline.

Colors are no longer hardcoded in controls: `Theme` has light, dark, and high contrast sets, `Window::SetTheme` hands the theme pointer to the content tree and children find it along the parent chain; `Property<T>` is a minimal property slot with change detection, `Set` returns whether it actually changed, and controls use that to call `Invalidate` and trigger a repaint. Windows read the system app light/dark preference by default, and `nopeui_window_smoke --light` and `--dark` force one; the high contrast palette is obtained explicitly through `HighContrastTheme()`, white on black with a yellow accent, and the golden tool overlays it with `--theme contrast`.

```cpp
auto panel = nopeui::StackPanel::Create(12.0f);
panel->Add(nopeui::Label::Create(L"Title"));
panel->Add(nopeui::Button::Create(L"Continue"));
panel->Add(nopeui::CheckBox::Create(L"Remember me"));
panel->Add(nopeui::TextBox::Create(L"Editable line"));

auto card = nopeui::Border::Create();
card->SetPadding(nopeui::Thickness{24.0f, 20.0f, 24.0f, 20.0f});
card->SetChild(panel);
window->SetContent(card);
```

## XAML

`markup` provides a minimal XAML parse and load path: `xaml::Parse` parses source text into a `Node` tree supporting elements, attributes, text, comments, processing instructions, and five predefined entities; `xaml::Load` creates controls by tag, sets properties, attaches children, and applies resources, and `xaml::LoadFile` reads a UTF-8 file. There is a single root, namespace prefixes are stripped, so `local:Button` and `Button` are equivalent.

Resources go in `*.Resources` property elements on any element and are visible only to that element and its descendants. Lookup follows the parent chain and the nearest resource of the same name wins, so different branches can define their own styles and templates with the same name. `ControlTemplate` has two shapes: `Part` children keep describing named parts; non-`Part` content is treated as a visual tree and requires exactly one root element and a non-empty `TargetType`, may nest `Border`, `StackPanel`, `ScrollViewer`, and `Flyout`, and leaves may be ordinary controls and `ContentPresenter`. The visual tree is instantiated per control, `{TemplateBinding Property}` writes to the target property at load time and stays in sync with changes on the source control. When the template's `ContentPresenter` has no explicit `Text`, `Button` fills in its own text. Visual templates do not yet support property elements inside the template, `Resources`, events, or `x:Class`. `Style` writes properties through `Setter`, and a `Template` setter references a template in the same or an ancestor scope through `{StaticResource key}`. Styles match on `TargetType` and `Key`; after the tree is built, the loader applies styles along the parent chain from outside in and then re-applies local properties, so local values win over styles and inner styles win over outer ones.

Besides styles and templates, `*.Resources` now also accepts `<Color>`, `<Number>`, `<Thickness>`, and `<String>` value resources keyed by `Key`; `{ThemeResource Key}` checks the value along that parent chain first and then falls to the theme palette, so local overrides and theme following are one and the same syntax. Styles accept `<Trigger Property=".." Value="..">` with nested `Setter`s; `ControlTemplate` `Part` templates can also carry `<VisualStateGroup Name="CommonStates">`, where each `<VisualState>` describes patches with `<StateSetter Part=".." Background=".."/>`. Visual templates (non-`Part` visual trees) and visual states are mutually exclusive, and mixing them fails at load time. `Dock="Left|Top|Right|Bottom"` and `Canvas.Left=".."`/`Canvas.Top=".."` can be written directly on children; `FlexPanel` uses `Direction="Row|Column"`, `Gap`, and `Wrap`, and children claim leftover space with `FlexGrow="1"`. `MenuBar` groups with `<Menu Title="..">` and lists items with `<Item Text=".." Click=".."/>`; left and right move between them, Enter or the down arrow expands, and Esc closes.

`Click`, `Submit`, `CheckedChanged`, and `ValueChanged` resolve callbacks through `LoadContext::resolve_event`; `EventArgs` carries the event type, the callback name, and the sender. Elements with `x:Name` land in `LoadResult::names` for lookup from code. A `ContextMenu` can be attached to any element with a `*.ContextMenu` property element, and the `ToolTip` property registers with `ToolTipService`.

```cpp
auto loaded = nopeui::xaml::LoadFile("page.xaml", context);
if (loaded.ok()) {
    window->SetContent(loaded.value().root);
}
```

Styles and templates are already applied at load time, so `LoadResult` no longer returns a global `StyleSheet`. `nopeui_xaml_smoke` reads `page.xaml` from the same directory, where two resource sections define multi-element visual templates with the same name, demonstrating scope isolation, template binding, and popups.

## Directories

The tree currently has `include/`, `src/`, `tests/`, and `tools/`; the rest are planned:

- `include/`: public headers
- `src/`: framework sources
- `tests/`: unit tests, golden images, UIA, and fuzzing
- `tools/`: golden image rendering and image comparison sources
- `tools/scripts/`: local build, test, and packaging scripts
- `docs/`: bilingual documentation
- `designer/`: NopeUI Designer
- `samples/`: the Gallery and other samples

## License

AGPL-3.0, see `LICENSE`. No commercial licensing is offered, and no contributor agreement is required.
