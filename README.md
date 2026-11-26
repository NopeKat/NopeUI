# NopeUI

NopeUI 是一个仅面向 Windows 的开源 C++ 桌面 UI 框架。外观参考 Fluent，交互与性能优先；API 与控件命名独立，不追求 WinUI 兼容。

目标平台是 Windows 10 1809 及以上，支持 x64 与 ARM64。窗口与呈现使用 DWM 和 DirectComposition，文本使用 DirectWrite，输入法使用 Win32；布局、绘制、控件、外壳、XAML 与设计器由框架自建。

## 当前状态

项目处于 P0 初期，`base` 最小内核与 `platform` 窗口层已建立：`Result` 与 `Error`、`Ref`、`Event` 与 `Subscription`、几何类型、UTF-8 与 UTF-16 转换，以及 Win32 窗口、消息循环、DirectComposition 虚拟表面、Direct2D 绘制与 DirectWrite 文字。窗口当前显示浅色背景、顶部强调条和一行示例文字，支持全帧与 dirty 矩形重绘；窗口默认启用双表面异步提交，`SetAsyncCommit(false)` 可回退同步，`LastPresentedFrame()` 与 `AsyncCommitStats()` 可读取提交延迟、队列和丢帧。帧提交默认按显示器刷新间隔节流，可用 `Window::Present()` 立即提交、`Window::SetCommitThrottle(false)` 关闭节流。`Window::LastFrameMilliseconds()`、`Window::FrameCount()`、`Window::CommitCount()` 与 `Window::RenderFrame()` 可读取和驱动帧，`nopeui_frame_benchmark` 提供 300 帧基准，`nopeui_resize_benchmark` 提供 resize 基准。软件回退尚未实现。8 个测试在 `x64-debug` 与 `x64-release` 下全部通过。

## 构建

工具链版本锁定为 CMake 4.4.4、Visual Studio 18 2026、MSVC 14.52.37025、Windows SDK 10.0.26100.0；生成器用 `Visual Studio 18 2026`，不引入 Ninja。

```powershell
.\eng\build.ps1 -Preset x64-debug
.\eng\test.ps1 -Preset x64-debug
```

`-Preset` 可选 `x64-debug` 或 `x64-release`。脚本优先使用 PATH 中的 CMake；找不到时会自动定位 winget 安装的 CMake 4.4.4。也可以用 `NOPEUI_CMAKE` 指向指定的 `cmake.exe`。

帧基准默认按 120Hz 固定节拍运行，只统计真正 `Commit` 的帧；`--unpaced` 关闭节拍，`--full` 改跑全帧，`--no-throttle` 关闭提交节流，`--async-commit` 打开异步提交。基准默认关闭异步提交以便与同步路径对比；窗口运行时默认是异步提交。输出新增 `queue`、`present` 与 `submitted/completed/dropped/waits` 统计，其中 `present` 是按 DWM vblank 估算的提交到上屏延迟：

```powershell
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe --full
.\.build\x64-release\samples\Release\nopeui_frame_benchmark.exe --full --async-commit
.\.build\x64-release\samples\Release\nopeui_resize_benchmark.exe
.\.build\x64-release\samples\Release\nopeui_resize_benchmark.exe --sync
```

## 目录

当前已建立 `include/`、`src/`、`tests/`、`eng/`；其余为计划目录：

- `include/`：公开头文件
- `src/`：框架源码
- `tests/`：单元测试、黄金图、UIA 与模糊测试
- `eng/`：本地构建、测试与打包脚本
- `docs/`：双语文档
- `designer/`：NopeUI Designer
- `samples/`：Gallery 等示例

## 许可

AGPL-3.0，见 `LICENSE`。不提供商业授权，不要求贡献者协议。
