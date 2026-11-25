# NopeUI

NopeUI 是一个仅面向 Windows 的开源 C++ 桌面 UI 框架。外观参考 Fluent，交互与性能优先；API 与控件命名独立，不追求 WinUI 兼容。

目标平台是 Windows 10 1809 及以上，支持 x64 与 ARM64。窗口与呈现使用 DWM 和 DirectComposition，文本使用 DirectWrite，输入法使用 Win32；布局、绘制、控件、外壳、XAML 与设计器由框架自建。

## 当前状态

项目处于 P0 启动前准备阶段。仓库目前只有许可、仓库配置与这份 README；源码、构建骨架与测试尚未建立。

## 构建

工具链版本锁定为 CMake 4.4.4、Visual Studio 18 2026、MSVC 14.52.37025、Windows SDK 10.0.26100.0；生成器用 `Visual Studio 18 2026`，不引入 Ninja。

构建骨架尚未建立。P0 第 1 天会加入 `CMakePresets.json` 与 `eng/build.ps1`，之后使用：

```powershell
.\eng\build.ps1 -Preset x64-debug
.\eng\test.ps1 -Preset x64-debug
```

在骨架加入之前，仓库没有可执行的构建命令。

## 目录

当前只有仓库级文件。计划中的目录：

- `src/`：框架源码
- `tests/`：单元测试、黄金图、UIA 与模糊测试
- `docs/`：双语文档
- `designer/`：NopeUI Designer
- `samples/`：Gallery 等示例
- `eng/`：本地构建、测试与打包脚本

## 许可

AGPL-3.0，见 `LICENSE`。不提供商业授权，不要求贡献者协议。
