# NopeUI

NopeUI 是一个仅面向 Windows 的开源 C++ 桌面 UI 框架。外观参考 Fluent，交互与性能优先；API 与控件命名独立，不追求 WinUI 兼容。

目标平台是 Windows 10 1809 及以上，支持 x64 与 ARM64。窗口与呈现使用 DWM 和 DirectComposition，文本使用 DirectWrite，输入法使用 Win32；布局、绘制、控件、外壳、XAML 与设计器由框架自建。

## 当前状态

项目处于 P0 第 3 天，`base` 最小内核与 `platform` 窗口层已建立：`Result` 与 `Error`、`Ref`、`Event` 与 `Subscription`、几何类型、UTF-8 与 UTF-16 转换，以及 Win32 空白窗口和消息循环。7 个测试在 `x64-debug` 与 `x64-release` 下全部通过；窗口目前只有创建、显示、隐藏、关闭和消息循环，尚未接入 DirectComposition 与绘制。

## 构建

工具链版本锁定为 CMake 4.4.4、Visual Studio 18 2026、MSVC 14.52.37025、Windows SDK 10.0.26100.0；生成器用 `Visual Studio 18 2026`，不引入 Ninja。

```powershell
.\eng\build.ps1 -Preset x64-debug
.\eng\test.ps1 -Preset x64-debug
```

`-Preset` 可选 `x64-debug` 或 `x64-release`。脚本优先使用 PATH 中的 CMake；找不到时会自动定位 winget 安装的 CMake 4.4.4。也可以用 `NOPEUI_CMAKE` 指向指定的 `cmake.exe`。

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
