# NopeUI

NopeUI 是一个仅面向 Windows 的开源 C++ 桌面 UI 框架。外观参考 Fluent，交互与性能优先；API 与控件命名独立，不追求 WinUI 兼容。

目标平台是 Windows 10 1809 及以上，支持 x64 与 ARM64。窗口与呈现使用 DWM 和 DirectComposition，文本使用 DirectWrite，输入法使用 Win32；布局、绘制、控件、外壳、XAML 与设计器由框架自建。

## 当前状态

项目处于 P0 初期，`base` 最小内核与 `platform` 窗口层已建立：`Result` 与 `Error`、`Ref`、`Event` 与 `Subscription`、几何类型、UTF-8 与 UTF-16 转换，以及 Win32 窗口、消息循环、DirectComposition 虚拟表面、Direct2D 绘制与 DirectWrite 文字。窗口默认显示浅色背景、顶部强调条与一行示例文字的占位场景；`Window::SetContent` 可挂一棵元素树，覆盖客户区并接管鼠标命中。窗口默认启用按面积自适应的 2–3 块表面异步提交：客户区面积不超过 `2560×1600` 时用 3 块，更大的窗口用 2 块以控制显存。`SetAsyncCommit(false)` 可回退同步，`LastPresentedFrame()` 与 `AsyncCommitStats()` 可读取提交延迟、队列和丢帧。缩放时虚拟表面按小窗口约三分之一、较大窗口固定 256 像素的余量分配，停止缩放 300 ms 后回收到实际客户区尺寸，避免拖动过程中反复重建表面。帧提交默认按显示器刷新间隔节流，可用 `Window::Present()` 立即提交、`Window::SetCommitThrottle(false)` 关闭节流。`Window::LastFrameMilliseconds()`、`Window::FrameCount()`、`Window::CommitCount()` 与 `Window::RenderFrame()` 可读取和驱动帧，`nopeui_frame_benchmark` 提供 300 帧基准，`nopeui_resize_benchmark` 提供 resize 基准。软件回退已可用：远程会话自动切到 WIC 软件光栅，`Window::SetSoftwareRender(true)` 可手动切换，软件图与硬件图在黄金图里逐像素一致。黄金图基线与工具：`nopeui_golden` 出图、`nopeui_image_diff` 比对、`eng/golden.ps1` 管理。`layout` 提供 Measure/Arrange/HitTest/Paint 的最小元素树，`controls` 已落地 `Border`、`Label`、`Button`、`StackPanel`、`ScrollViewer`；`nopeui_window_smoke` 的默认界面就是这些控件拼出的卡片。`Theme` 定义浅色与深色两套调色板，窗口默认跟随系统应用深浅色偏好，可用 `SetTheme` 覆盖；元素沿父链继承主题，控件从主题取色，占位场景的调色板也跟随主题。8 个测试在 `x64-debug` 与 `x64-release` 下全部通过。

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

基线在 `tests/golden/baseline`，当前图与差异图在 `.build/golden`。占位场景有软件与硬件各两张基线，控件树另有浅色、深色两张（软件路径）；脚本还会直接把占位场景浅色、深色的软件图与硬件图互相比对。`nopeui_image_diff` 按单像素 2 级色差、总体 0.5% 判定，有差异时写出的图把变化像素标红。两个工具也可单独调用：

```powershell
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\basic.png --theme light --renderer software
.\.build\x64-release\tools\Release\nopeui_golden.exe --out .\.build\golden\controls.png --scene controls --theme dark --renderer software
.\.build\x64-release\tools\Release\nopeui_image_diff.exe <baseline.png> <current.png> --diff <diff.png>
```

窗口的硬件与软件两条路径现在都通过 `graphics/scene` 的 `ScenePainter` 绘制，占位场景只剩一份实现；`ScenePainter` 缓存画刷与文本布局，按帧只做矩形填充、文字与强调条。

## 布局与控件

元素树只做四件事：`Measure` 用可用尺寸算期望尺寸，`Arrange` 落最终矩形，`HitTest` 找最上层命中的元素，`Paint` 由窗口在硬件与软件两条路径里统一调用。`StackPanel` 支持纵向与横向排列，`Border` 负责背景、1 像素描边与内边距，`ScrollViewer` 提供单子元素的纵向滚动，`Label` 和 `Button` 复用 `graphics::TextRun` 缓存的文字布局；`Button` 带悬停、按下底色、焦点矩形和点击回调，鼠标点击或 Enter/Space 都会触发。

`TextStyle` 控制字号、粗细、换行与省略。`Label` 换行时按可用宽度重建文本布局并在高度上增长，省略时用 DirectWrite 的尾部裁剪；布局变化通过 `InvalidateLayout` 回到窗口重新测量与排列，单纯改颜色只触发重绘。

容器在绘制子元素前按自身 bounds 压裁剪区，`Border`、`StackPanel`、`ScrollViewer` 都这么做。`ScrollViewer` 目前只有滚轮滚动，没有滚动条；偏移在测出内容高度后夹紧，滚轮从命中元素向上冒泡到最近的滚动容器。

焦点按视觉顺序排序：`IsFocusable()` 标记可聚焦元素，`CollectFocusable()` 递归收集，窗口维护 Tab 与 Shift+Tab，失去窗口焦点时清空。`Button` 的 `SetOnClick` 保存单个回调，`SubscribeClick` 返回可管理的订阅。

颜色不再写死在控件里：`Theme` 有浅色与深色两套值，`Window::SetTheme` 把主题指针交给内容树，子元素沿父链找到它；`Property<T>` 是带变更检测的最小属性槽，`Set` 返回是否真的变化，控件据此调用 `Invalidate` 触发重绘。窗口默认读系统应用深浅色偏好，`nopeui_window_smoke --light` 与 `--dark` 可强制指定。

```cpp
auto panel = nopeui::StackPanel::Create(12.0f);
panel->Add(nopeui::Label::Create(L"Title"));
panel->Add(nopeui::Button::Create(L"Continue"));

auto card = nopeui::Border::Create();
card->SetPadding(nopeui::Thickness{24.0f, 20.0f, 24.0f, 20.0f});
card->SetChild(panel);
window->SetContent(card);
```

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
