# Changelog

Every released version gets one section, newest last. Entries describe what that
version actually shipped, so there is no unreleased section; pending work is folded
into the version being prepared. Platforms are kept apart as subsections.

## [0.0.0.0] - 2026-11-28

First preview, Nightly channel. Windows x64 only: static libraries built against the
dynamic CRT, plus the two sample executables.

### Windows x64

#### Added

- Core: `Result` and `Error`, `Ref`, `Event` and `Subscription`, the geometry types,
  UTF-8 and UTF-16 conversion, and `VersionString`.
- Window and presentation: Win32 windows and the message loop, DirectComposition
  virtual surfaces, Direct2D painting, DirectWrite text. Async commit is on by default
  with 3 surfaces up to a 2560x1600 client area and 2 above it; `SetAsyncCommit(false)`
  falls back to synchronous commit, `LastPresentedFrame()` and `AsyncCommitStats()`
  report commit latency, queue depth, and dropped frames, and commits are throttled to
  the display refresh interval unless `SetCommitThrottle(false)` is called.
  `Present()` commits immediately.
- Sizing: `Window::Create` takes logical DIPs and scales pixels by the monitor DPI;
  `WM_DPICHANGED` rebuilds the surface and repaints instead of waiting for a later
  display query. A resize allocates surfaces with a margin of about one third for
  small windows and 256 pixels for large ones, compacts them 300 ms after the drag
  stops, and draws live frames while dragging.
- Rendering paths: the software raster path engages automatically in remote sessions
  and `SetSoftwareRender(true)` switches it manually; the experimental flip-model
  swap chain keeps its present off the UI thread. Software and hardware images match
  pixel for pixel in the golden images.
- Layout: element tree with Measure/Arrange/HitTest/Paint, then `StackPanel`, `Grid`
  with pixel, `Auto`, and star tracks plus spans and attached properties, `Border`,
  `Canvas`, `DockPanel`, `FlexPanel`, and `ScrollViewer`; containers clip to their own
  bounds.
- Controls: `Label`, `Button`, `CheckBox`, `RadioButton`, `ToggleButton`,
  `ToggleSwitch`, `TextBox` with selection, clipboard, and IME, `ProgressBar`,
  `Slider`, `Image`, `MenuBar`, plus `ToolTip`, `Flyout`, and `ContextMenu` hosted on
  the window overlay root. Focus order follows visual order; disabled subtrees leave
  hit testing, focus, and the wheel.
- Theme: light, dark, and high contrast palettes, system light/dark following,
  `SetTheme` override, and theme inheritance down the parent chain. Control colors
  come from the theme instead of hardcoded values.
- Properties, styles, and templates: `PropertyValue` and `PropertyMetadata` with the
  Affects flags, local-value precedence with `ClearValue`, `ClearStyleValues`, and
  `RemoveTriggers`; `Style`, `StyleSheet`, `ControlTemplate` parts and visual trees,
  `VisualStateGroup` patches, `TemplateBinding`, `ContentPresenter`, and property
  triggers. Resources resolve along the parent chain, `{ThemeResource Key}` falls back
  to the palette, and `Color`, `Number`, `Thickness`, and `String` value resources are
  accepted.
- XAML: `xaml::Parse`, `xaml::Load`, and `xaml::LoadFile`; per-element resource scopes,
  implicit styles applied from the outer scope inward, visual-tree templates, triggers,
  `Dock`/`Canvas.Left`/`Canvas.Top`/`FlexPanel` markup, `MenuBar` with menus and items,
  popup property elements, `Click`/`Submit`/`CheckedChanged`/`ValueChanged` callbacks,
  and `x:Name` lookup.
- Accessibility: a UI Automation server provider that answers `WM_GETOBJECT` with a
  Window root and exposes Name, ControlType, BoundingRectangle, IsEnabled, and focus
  state along the tree, with Invoke, Toggle, Value, Text, RangeValue, and Scroll
  patterns, UIA focus events, and menu item providers for `MenuBar` and `ContextMenu`.
- Samples and benchmarks: `nopeui_window_smoke` on the shared form page,
  `nopeui_xaml_smoke`, and the startup, theme, frame, resize, text-list, and present
  benchmarks.
- Tooling: `x64-debug` and `x64-release` CMake presets, plus local scripts for build,
  test, golden images, UIA, performance, code analysis, fuzzing, coverage, and
  packaging. `docs/REPRODUCE.md` walks a clean machine from tooling installation to the
  package. `package.ps1` assembles a portable zip with headers, the seven static
  libraries, the sample binaries, the docs, the AGPL source archive,
  `package-info.txt`, and a sha256 list.

#### Changed

- XAML resources are scoped to the element that owns them and resolve along the parent
  chain; styles are applied while loading. `LoadResult` no longer exposes a global style
  sheet, which breaks markup consumers reading the old field.
- Engineering scripts moved from `tools/` to `tools/scripts/`; the documented command
  paths follow.
- Golden image baselines moved out of the repository into the workspace `assets/golden`.
  `NOPEUI_GOLDEN_ASSETS` points the scripts at another baseline directory.
- README and `docs/REPRODUCE.md` are written in American English.

#### Fixed

- Layout and controls: `Grid` reports its true height; the progress bar fill starts at
  the element bounds; `Canvas` children stay inside the panel; the scrollbar thumb is
  capped to the track; changing a slider range raises a value change for the clamped
  value; menu state resets when a popup opens; explicit colors and local XAML values are
  honored.
- DPI and presentation: the first frame scales to the monitor DPI; a new DPI is taken
  from `WM_DPICHANGED` instead of a later query; painting happens in DIP and recovers
  lost graphics; the flip path keeps its target on buffer zero and is used for visible
  windows.
- Automation and markup: automation providers disconnect when a window closes, the UIA
  root is visible to clients, and menu items expose automation actions; theme keys that
  can never resolve are rejected at load; clearing values, removing styles, and closing
  popups hold up across runtime layout changes; code analysis findings are cleared.

#### Performance

- Composition: dirty-region redraw, virtual surface reuse, commit throttling, and draws
  overlapped with commits; the flip present runs off the UI thread.
- Text and painting: DirectWrite layouts are cached, up to 512 entries in the text-list
  path, and `ScenePainter` reuses brushes and text layouts across both render paths.
- Long lists: row-based virtual culling only creates layouts for the current viewport,
  and paced resize and scroll benchmarks keep the numbers measurable.
