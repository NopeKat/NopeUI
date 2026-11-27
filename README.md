# NopeUI

NopeUI 是一个仅面向 Windows 的开源 C++ 桌面 UI 框架。外观参考 Fluent，交互与性能优先；API 与控件命名独立，不追求 WinUI 兼容。

目标平台是 Windows 10 1809 及以上，支持 x64 与 ARM64。窗口与呈现使用 DWM 和 DirectComposition，文本使用 DirectWrite，输入法使用 Win32；布局、绘制、控件、外壳、XAML 与设计器由框架自建。`Window::Create` 的宽高按逻辑 DIP 解释，窗口像素尺寸随所在显示器 DPI 缩放。

## 当前状态

项目进入 P1 早期，P0 的 `base`、`platform`、`layout`、`controls` 与无障碍闭环已完成：`Result` 与 `Error`、`Ref`、`Event` 与 `Subscription`、几何类型、UTF-8 与 UTF-16 转换，以及 Win32 窗口、消息循环、DirectComposition 虚拟表面、Direct2D 绘制与 DirectWrite 文字。窗口默认显示浅色背景、顶部强调条与一行示例文字的占位场景；`Window::SetContent` 可挂一棵元素树，覆盖客户区并接管鼠标命中。窗口默认启用按面积自适应的 2–3 块表面异步提交：客户区面积不超过 `2560×1600` 时用 3 块，更大的窗口用 2 块以控制显存。`SetAsyncCommit(false)` 可回退同步，`LastPresentedFrame()` 与 `AsyncCommitStats()` 可读取提交延迟、队列和丢帧。缩放时虚拟表面按小窗口约三分之一、较大窗口固定 256 像素的余量分配，停止缩放 300 ms 后回收到实际客户区尺寸，避免拖动过程中反复重建表面。帧提交默认按显示器刷新间隔节流，可用 `Window::Present()` 立即提交、`Window::SetCommitThrottle(false)` 关闭节流。`Window::LastFrameMilliseconds()`、`Window::FrameCount()`、`Window::CommitCount()` 与 `Window::RenderFrame()` 可读取和驱动帧，`nopeui_frame_benchmark` 提供 300 帧基准，`nopeui_resize_benchmark` 提供 resize 基准。软件回退已可用：远程会话自动切到 WIC 软件光栅，`Window::SetSoftwareRender(true)` 可手动切换，软件图与硬件图在黄金图里逐像素一致。黄金图基线与工具：`nopeui_golden` 出图、`nopeui_image_diff` 比对、`eng/golden.ps1` 管理。`layout` 提供 Measure/Arrange/HitTest/Paint 的最小元素树，`controls` 已落地 `Border`、`Grid`、`Label`、`Button`、`StackPanel`、`Canvas`、`DockPanel`、`FlexPanel`、`ScrollViewer`、`CheckBox`、`RadioButton`、`ToggleButton`、`ToggleSwitch`、`TextBox`、`ProgressBar`、`Slider`、`Image`、`MenuBar`，以及弹出层用的 `ToolTip`、`Flyout`、`ContextMenu`；`nopeui_window_smoke` 的默认界面是一页用 `Grid` 拼出的表单，顶部带一条 `MenuBar`，实现在 `samples/common/form_page.h`，黄金图里的 `form` 场景用的也是这棵树。`Theme` 定义浅色、深色与高对比三套调色板，窗口默认跟随系统应用深浅色偏好，可用 `SetTheme` 覆盖；元素沿父链继承主题，控件从主题取色，占位场景的调色板也跟随主题。16 个测试在 `x64-debug` 与 `x64-release` 下全部通过。

属性元数据放在 `property_value.h`：`PropertyValue` 用 `std::variant` 承载布尔、整数、浮点、颜色、字符串与 `Thickness`，`PropertyMetadata` 记录名字、类型、默认值与 `AffectsMeasure`、`AffectsArrange`、`AffectsRender`、`Inherits` 标志。`Element` 提供 `Properties`、`GetProperty`、`SetProperty` 与 `SubscribePropertyChanged`，基类固定暴露 `IsEnabled`、可继承的 `Foreground`，以及 `Margin`、`Width`、`Height`、`MinWidth`、`MinHeight`、`MaxWidth`、`MaxHeight`、`HorizontalAlignment`、`VerticalAlignment` 这组布局属性。`Foreground` 沿父链向上查找，最后回落到主题文字色。各控件通过自己的属性表暴露可枚举属性，`SetProperty` 会校验类型、发出变更事件，再按标志触发重绘或重新布局。

布局按槽位模型走：`Measure` 先扣掉 `Margin`，按显式尺寸与最大最小约束夹取可用空间，再交给 `MeasureOverride`，回写时加回 `Margin` 并同时记下内容期望尺寸；`Arrange` 拿到父给的槽位，先按 `Margin`、显式尺寸与水平/垂直对齐算出最终矩形，再进 `ArrangeOverride`。对齐默认是 `Stretch`，零边距时结果与旧行为一致。`StackPanel` 在交叉轴给满槽位，由子元素自己的对齐决定摆放，所以单行输入框、滑块和进度条会自然拉满一列。

`Grid` 三遍测量：先算列、再算行、最后排布。`ColumnDefinition` 与 `RowDefinition` 的 `Width`/`Height` 支持像素、`Auto` 与 `*` 星号，剩余空间按权重分配；`Auto` 只统计单格子元素，跨格内容不参与轨道尺寸，避免一段跨列标题把标签列撑开。`Grid.Row`、`Grid.Column`、`Grid.RowSpan`、`Grid.ColumnSpan` 是附加属性，可写在任意子元素上，也可在 XAML 里直接作为属性设置。

属性优先级是“本地值优先”：`SetProperty` 记本地值，样式经 `Style::Apply` 走 `ApplyStyleProperty` 写入，但不覆盖已有本地值——样式值仍会记下来，等 `ClearValue` 清掉本地值后立刻接管。`ClearValue` 的回落顺序是“触发器中生效的值 → 样式值 → 元数据默认值”，也是把属性还给样式与触发器的唯一入口。

`Canvas` 按 `Canvas.Left` 与 `Canvas.Top` 定位，测量时用无界空间，期望尺寸取子元素外沿的最大值；排布时把自身原点加回偏移，子元素坐标与其它容器一样是绝对坐标，嵌在带内边距的卡片里也不会跑偏。`DockPanel` 按 `Dock` 附加属性（`Left`、`Top`、`Right`、`Bottom`，取值 0–3）逐个子元素切走一条边，`LastChildFill` 默认让最后一个孩子吃掉剩余区域。两者都不做裁剪，超出边界的部分照常绘制。`FlexPanel` 沿 `Direction` 排布，`Gap` 控制间距，`Wrap` 打开后在主轴用尽时换行；子元素用 `FlexGrow` 按权重分剩余空间，不开换行时单行占满交叉轴，开换行时每行只占自己的高度。

资源与主题资源是同一套查找：`Element::SetResource` 写本地表，`FindResourceValue` 先查自身、再沿父链、最后落到 `Theme` 的调色板键，所以同名资源分层覆盖很自然。`{ThemeResource Key}` 可以绑到颜色、数值、`Thickness` 等任意类型匹配的属性；值在绑定当时就应用，加载期还没声明的键由加载器在树建完后统一刷新一次，之后随 `Window::SetTheme` 换色。`PropertyTrigger` 监听一个属性，值与 `Value` 相等时写入自己的 setter；样式 setter 会记下原值并让位给触发器，条件不再匹配时按样式值或元数据默认值回填，本地值仍然压过触发器。`ControlTemplate` 可带 `VisualStateGroup`，每个状态是一组部件补丁，`Button` 按 `Normal`、`PointerOver`、`Pressed`、`Disabled` 取部件色，模板色优先、否则回落主题态色。

样式与模板走最小闭环：`Style` 按目标类型与可选键匹配，保存 `PropertyValue` setter 和可选模板；`StyleSheet::ApplyTo` 递归下发，先应用属性再套模板。`ControlTemplate` 既可保存按名字索引的部件，也可保存一棵视觉树；`TemplatePart` 带背景、前景、描边、圆角与内边距。`Button` 同时消费旧部件模板与视觉树模板，`CheckBox` 已消费 `Box`、`Check` 与 `Content`，说明模板不是只存不用。

样式也有移除入口：`Element::ClearStyleValues()` 丢掉这个元素记住的样式值，`Element::RemoveTriggers(style)` 按来源摘掉触发器，两者都会让属性回落到本地值、剩余触发器或元数据默认值；`Style::RemoveTrigger(index)` 在样式对象上删一条触发器，方便复用样式。附加属性的本地值同样归 `ClearValue` 管，`Canvas.Left` 设过又清掉会回到 0。

弹出层由窗口内置的覆盖根承载，不塞进内容树：`ShowPopup`、`ClosePopup`、`HasPopup` 管理锚定面板，测量、排列、绘制、命中与焦点都优先覆盖层；Esc、外部点击和外部滚轮关闭。右键沿命中元素的父链找到 `ContextMenu` 并打开，超高菜单可用滚轮与键盘滚动；`ToolTipService` 以元素为键登记文本，悬停 600 ms 后弹出；`Flyout` 是单内容圆角面板。关闭走延迟消息，避免在控件的 `OnPointerUp` 回调里直接释放自身。

`Image` 复用 `graphics::LoadPng` 的 WIC 解码入口，提供 `None`、`Fill`、`Uniform`、`UniformToFill` 四种拉伸；绘制时按渲染目标缓存 `ID2D1Bitmap`，直通 alpha 在缓存时转成预乘，软件与硬件路径共用同一套源矩形计算。

## 构建

工具链版本锁定为 CMake 4.4.4、Visual Studio 18 2026、MSVC 14.52.37025、Windows SDK 10.0.26100.0；生成器用 `Visual Studio 18 2026`，不引入 Ninja。

```powershell
.\eng\build.ps1 -Preset x64-debug
.\eng\test.ps1 -Preset x64-debug
```

`-Preset` 可选 `x64-debug` 或 `x64-release`。脚本优先使用 PATH 中的 CMake；找不到时会自动定位 winget 安装的 CMake 4.4.4。也可以用 `NOPEUI_CMAKE` 指向指定的 `cmake.exe`。

帧基准默认按 120Hz 固定节拍运行，只统计真正 `Commit` 的帧；`--unpaced` 关闭节拍，`--full` 改跑全帧，`--no-throttle` 关闭提交节流，`--async-commit` 打开异步提交。`--rects N` 与 `--text N` 生成合成重内容；默认是同一小区域反复绘制，`--virtual` 改成按行展开的虚拟长列表，只绘制当前内容区域内的行，`--scroll N` 让该列表每帧下移 N 像素，用来验证滚动时的裁剪路径。基准默认关闭异步提交以便与同步路径对比；窗口运行时默认是异步提交。输出新增 `queue`、`present` 与 `submitted/completed/dropped/waits` 统计，其中 `present` 是按 DWM vblank 估算的提交到上屏延迟：

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

`nopeui_text_list_benchmark.exe` 生成指定行数的不同文本，按行高虚拟化，只对当前视口创建 `IDWriteTextLayout`；`--cache` 打开最多 512 条的布局缓存。`--scroll` 是每帧滚动像素数，`--width/--height` 指定窗口尺寸，`--client` 按客户区尺寸而不是外框尺寸调整，`--surfaces 2|3` 强制异步表面数用于对照，`--sync` 回退同步提交，`--flip` 切到实验性的 flip-model swap chain 路径；输出包含请求尺寸、实际客户区、DPI、awareness、表面数、异步状态、呈现模式，以及 `begin/draw/end`、`commit/queue/present` 的平均与 P99。

`nopeui_resize_benchmark.exe --interval <ms>` 在每步 resize 后等待指定毫秒数，用来模拟真实拖动的节奏；`--interval 0` 是连续无间隔的压力测试，`--flip` 用实验性 flip-model 路径，`--sync` 回退同步提交。

`nopeui_present_benchmark.exe` 在独立弹出窗口比较 DirectComposition 虚拟表面 `Commit` 与 flip-model swap chain `Present` 的提交 API 耗时；默认 virtual，`--flip` 切换 flip，`--width/--height` 指定表面尺寸。它只测提交调用，不含完整控件绘制。

`nopeui_window_smoke.exe --trace <csv>` 会在交互期间输出帧、提交和延迟统计，用于观察拖动与缩放；`--sync` 可切到同步提交做对比。

## 黄金图

`nopeui_golden` 用 WIC 位图作为 Direct2D 目标离屏渲染占位场景，文字固定为灰度抗锯齿，输出不依赖显示器配置；`--renderer hardware` 走 D3D11 纹理加暂存纹理回读，用来对照两条路径。`eng/golden.ps1` 负责构建、生成与比对：

```powershell
.\eng\golden.ps1                              # 构建并比对现有基线
.\eng\golden.ps1 -Update                      # 重新建立基线
.\eng\golden.ps1 -Preset x64-debug -SkipBuild  # 只跑比对
```

基线在 `tests/golden/baseline`，当前图与差异图在 `.build/golden`，共 24 张。占位场景有软件与硬件各两张基线；控件树与媒体场景各有浅色、深色两张软件基线，媒体场景另有浅色、深色两张硬件基线；表单场景有浅色、深色、高对比三套，其中浅色与深色各带一张硬件基线，高对比也带硬件对照；XAML 场景同样有浅色、深色两张软件基线与两张硬件基线；面板场景（`Canvas`、`DockPanel`、`FlexPanel`）有浅色、深色两张软件基线与两张硬件基线。脚本会把占位、媒体、表单、高对比表单、面板与 XAML 场景的软件图、硬件图分别互相比对。`nopeui_image_diff` 按单像素 2 级色差、总体 0.5% 判定，有差异时写出的图把变化像素标红。两个工具也可单独调用：

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

窗口的硬件与软件两条路径现在都通过 `graphics/scene` 的 `ScenePainter` 绘制，占位场景只剩一份实现；`ScenePainter` 缓存画刷与文本布局，按帧只做矩形填充、文字与强调条。

## 性能基准

`nopeui_startup_benchmark` 从进程创建时间起算到首帧提交完成，并把加载器、窗口创建、内容构建、`Show` 和提交等待拆开输出，同时读取工作集峰值与私有提交峰值。`nopeui_theme_benchmark` 在表单页上反复切换浅色与深色，把 `SetTheme`（含重新布局、绘制与提交）与异步完成的等待分开统计；它默认关闭提交节流，避免 vblank 混进样本。

```powershell
.\eng\perf.ps1                                          # 构建并跑一组，打印汇总
.\eng\perf.ps1 -Runs 5 -Switches 200 -Out tests\perf\baseline.md
.\.build\x64-release\samples\Release\nopeui_startup_benchmark.exe --no-content
.\.build\x64-release\samples\Release\nopeui_theme_benchmark.exe --switches 200 --contrast
```

启动的 `total` 含进程创建与加载器；`--no-content` 换成占位场景，`--sync` 与 `--software` 分别覆盖同步提交和软件光栅。第一组基线在 `tests/perf/baseline.md`：900×640 表单页启动 256 到 344 毫秒，工作集峰值约 48 MB；主题切换平均 8.9 毫秒、P99 25.4 毫秒。

## 布局与控件

元素树只做四件事：`Measure` 用可用尺寸算期望尺寸，`Arrange` 落最终矩形，`HitTest` 找最上层命中的元素，`Paint` 由窗口在硬件与软件两条路径里统一调用。`StackPanel` 支持纵向与横向排列，`Grid` 按行列轨道摆放并把跨格与附加属性一起算进去，`Border` 负责背景、1 像素描边、内边距与可选圆角，`ScrollViewer` 提供单子元素的横向与纵向滚动。`Label`、`Button`、`CheckBox`、`RadioButton`、`ToggleButton`、`ToggleSwitch`、`TextBox`、`ProgressBar` 与 `Slider` 共用 `graphics::TextRun` 缓存的文字布局；`Button` 带圆角、悬停与按下底色、焦点矩形和点击回调，`CheckBox` 与 `ToggleButton` 保留选中态，鼠标点击或 Enter/Space 都会切换；同一父容器下的 `RadioButton` 自动互斥，`ToggleSwitch` 用 Space 切换；`ProgressBar` 支持确定值与不确定值，`Slider` 支持鼠标拖动、方向键、Home/End 与 PageUp/PageDown。`SetEnabled(false)` 后按 `ControlState::Disabled` 取色，不参与命中、焦点和滚轮。

`TextStyle` 控制字号、粗细、字族、对齐、行高倍数、换行与省略。`Label` 换行时按可用宽度重建文本布局并在高度上增长，省略时用 DirectWrite 的尾部裁剪，居中或右对齐时占满可用宽度；布局变化通过 `InvalidateLayout` 回到窗口重新测量与排列，单纯改颜色只触发重绘。`TextBox` 是单行编辑框，支持字符输入、光标、拖选与 Shift 选区、Ctrl+A/C/X/V、退格/删除、方向键、Home/End 和回车提交，长文本会跟随光标横向滚动；IME 组合过程作为预览显示并加下划线，提交结果后才写入文本，`SetPlaceholder` 提供空内容提示。

容器在绘制子元素前按自身 bounds 压裁剪区，`Border`、`StackPanel`、`ScrollViewer` 都这么做。`ScrollViewer` 在内容溢出时按需显示 6 像素细滚动条，鼠标靠近时加粗到 10 像素，横向与纵向都可拖动，内容放得下时自动隐藏；滚轮从命中元素向上冒泡到最近的滚动容器，步长按系统“每次滚动几行”换算，设为整页滚动时改滚一屏，按住 Shift 或只有横向溢出时滚横向。键盘 PageUp/PageDown/Home/End 会在焦点控件不处理时冒泡到滚动容器；只有横向溢出时这几个键改滚横向。焦点移到视口外的控件时，窗口会沿父链调用 `BringChildIntoView`，最近的滚动容器自动把该控件滚进视口。

焦点按视觉顺序排序：`IsFocusable()` 标记可聚焦元素，`CollectFocusable()` 递归收集，窗口维护 Tab 与 Shift+Tab；窗口失去激活时保留逻辑焦点，只关闭弹层并取消按压，重新激活后可直接继续输入。禁用元素及其子树不进入焦点序列。`Button` 的 `SetOnClick` 保存单个回调，`SubscribeClick` 返回可管理的订阅。

无障碍走 UI Automation 的服务端 Provider：窗口响应 `WM_GETOBJECT`，根节点是 Window，控件按元素树暴露 Name、ControlType、BoundingRectangle、IsEnabled 和焦点状态。`Button`、`ToggleButton`、`CheckBox`、`RadioButton` 提供 Invoke 或 Toggle 模式，`TextBox` 提供 Value 与 Text 模式，`Slider` 和 `ProgressBar` 提供 RangeValue，`ScrollViewer` 提供 Scroll 模式，焦点变化时发出 UIA 焦点事件。`MenuBar` 与 `ContextMenu` 的菜单行是绘制出来的数据行，UIA 侧按 `MenuItem` 角色单独补一层子 Provider，挂在菜单元素的第一个子元素位置，可上下翻兄弟，名字就是菜单标题或菜单项文字。

`eng/uia.ps1` 是一次手动走查：启动 `nopeui_window_smoke`，用 UIA 遍历整棵名为 `NopeUI Window Smoke` 的树，检查菜单条与 `File`、`Edit`、`View` 三个菜单项是否出现，失败返回非零。`eng/coverage.ps1` 用 OpenCppCoverage 跑 `ctest` 并输出 HTML 与 Cobertura；工具不入库，找不到时脚本直接说明安装方式，不会自行下载。两个脚本都不进任何自动化流水线。

颜色不再写死在控件里：`Theme` 有浅色、深色与高对比三套值，`Window::SetTheme` 把主题指针交给内容树，子元素沿父链找到它；`Property<T>` 是带变更检测的最小属性槽，`Set` 返回是否真的变化，控件据此调用 `Invalidate` 触发重绘。窗口默认读系统应用深浅色偏好，`nopeui_window_smoke --light` 与 `--dark` 可强制指定；高对比调色板由 `HighContrastTheme()` 显式取得，黑底白字配黄色强调，黄金工具用 `--theme contrast` 覆盖它。

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

`markup` 提供 XAML 的最小解析与加载链路：`xaml::Parse` 把源文本解析成 `Node` 树，支持元素、属性、文本、注释、处理指令和五个预定义实体；`xaml::Load` 按标签创建控件、设置属性、挂接子元素并应用资源，`xaml::LoadFile` 读取 UTF-8 文件。根节点只有一个，命名空间前缀会被剥掉，所以 `local:Button` 与 `Button` 等价。

资源放在任意元素的 `*.Resources` 属性元素里，只对拥有它的元素及其后代可见。查找按父链进行，最近的同名资源先命中，因此不同分支可以各自定义同名样式与模板。`ControlTemplate` 有两种形态：`Part` 子元素继续描述命名部件；非 `Part` 内容视为视觉树，要求恰好一个根元素和非空 `TargetType`，可嵌套 `Border`、`StackPanel`、`ScrollViewer`、`Flyout`，叶子可用普通控件和 `ContentPresenter`。视觉树按控件实例化，`{TemplateBinding Property}` 在加载时写入目标属性，并随源控件属性变化自动同步。模板里的 `ContentPresenter` 未显式设置 `Text` 时，`Button` 会把自身文本填进去。视觉模板暂不支持模板内属性元素、`Resources`、事件和 `x:Class`。`Style` 用 `Setter` 写入属性，`Template` setter 通过 `{StaticResource key}` 引用同一作用域或祖先作用域的模板。样式按 `TargetType` 与 `Key` 匹配；加载器在树建完后沿父链由外向内应用样式，最后重新应用局部属性，保证局部值优先于样式，内层样式优先于外层。

`*.Resources` 除了样式与模板，现在也接受 `<Color>`、`<Number>`、`<Thickness>`、`<String>` 值资源，键名由 `Key` 给出，`{ThemeResource Key}` 解析时先查这条父链上的值，再落到主题调色板，因此局部覆盖与主题跟随是同一套写法。样式里可以写 `<Trigger Property=".." Value="..">` 与内层 `Setter`；`ControlTemplate` 的 `Part` 模板还能带 `<VisualStateGroup Name="CommonStates">`，其中每个 `<VisualState>` 用 `<StateSetter Part=".." Background=".."/>` 描述补丁。视觉模板（非 `Part` 视觉树）与视觉状态互斥，混用会在加载时报错。`Dock="Left|Top|Right|Bottom"` 与 `Canvas.Left=".."`、`Canvas.Top=".."` 可直接写在子元素上；`FlexPanel` 用 `Direction="Row|Column"`、`Gap`、`Wrap`，子元素用 `FlexGrow="1"` 领取剩余空间。`MenuBar` 用 `<Menu Title="..">` 分组、`<Item Text=".." Click=".."/>` 列项，键盘左右移动、Enter 或下箭头展开，Esc 关闭。

`Click`、`Submit`、`CheckedChanged`、`ValueChanged` 通过 `LoadContext::resolve_event` 取回调；`EventArgs` 同时带事件类型、回调名和发送者。`x:Name` 的元素会进入 `LoadResult::names`，便于代码侧查找。`ContextMenu` 可以用 `*.ContextMenu` 属性元素挂到任意元素，`ToolTip` 属性会登记到 `ToolTipService`。

```cpp
auto loaded = nopeui::xaml::LoadFile("page.xaml", context);
if (loaded.ok()) {
    window->SetContent(loaded.value().root);
}
```

样式与模板在加载时已经应用，`LoadResult` 不再返回全局 `StyleSheet`。`nopeui_xaml_smoke` 读取同目录的 `page.xaml`，其中两段资源各自定义同名多元素视觉模板，演示作用域隔离、模板绑定与弹出层。

## 目录

当前已建立 `include/`、`src/`、`tests/`、`tools/`、`eng/`；其余为计划目录：

- `include/`：公开头文件
- `src/`：框架源码
- `tests/`：单元测试、黄金图、UIA 与模糊测试
- `tools/`：黄金图渲染与图像比对
- `eng/`：本地构建、测试与打包脚本
- `docs/`：双语文档
- `designer/`：NopeUI Designer
- `samples/`：Gallery 等示例

## 许可

AGPL-3.0，见 `LICENSE`。不提供商业授权，不要求贡献者协议。
