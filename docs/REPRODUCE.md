# 从干净机器复现构建

这份文档描述如何在一台干净的 Windows 机器上，从源码构建、验证并运行 NopeUI。所有步骤都是本地手动执行的命令，项目没有任何 CI。

## 1. 环境要求

- Windows 10 或 Windows 11，x64。当前只有 x64 目标。
- PowerShell 7（`pwsh`），Windows 自带的 5.1 不保证可用。
- Visual Studio 18 2026，安装工作负载「使用 C++ 的桌面开发」。
- Windows SDK 10.0.26100.0（在 VS 安装器里勾选「Windows 11 SDK (10.0.26100.0)」）。
- CMake 4.4.4。生成器固定为 `Visual Studio 18 2026`，不使用 Ninja。

工具链版本是硬约束：`eng/toolchain.ps1` 会检查 `cmake --version` 是否为 4.4.4，不符直接报错，不会自动降级或下载。

安装 CMake 可以走 winget：

```powershell
winget install --id Kitware.CMake --version 4.4.4
```

如果 PATH 里的 `cmake.exe` 版本不对，用环境变量指定：

```powershell
$env:NOPEUI_CMAKE = 'C:\Program Files\CMake\bin\cmake.exe'
```

## 2. 取源码

```powershell
git clone git@github.com:NopeKat/NopeUI.git
cd NopeUI
git checkout main
```

仓库里只有内层源码；工作区仓库（设计文档、路线图）不参与构建，拿不到也不影响。

## 3. 构建

```powershell
.\eng\build.ps1 -Preset x64-release
```

脚本会用 `CMakePresets.json` 里的 `x64-release` 预设配置到 `.build/x64-release`，再编译 Release 配置。编译选项是 `/W4 /WX /permissive- /utf-8`，任何警告都会中断构建，这是刻意设计。

产物位置：

- 样例：`.build\x64-release\samples\Release\nopeui_window_smoke.exe`、`nopeui_xaml_smoke.exe`
- 工具：`.build\x64-release\tools\Release\nopeui_golden.exe`、`nopeui_image_diff.exe`、`nopeui_xaml_fuzz.exe`
- 基准：`.build\x64-release\samples\Release\nopeui_startup_benchmark.exe`、`nopeui_theme_benchmark.exe`
- 单元测试：`.build\x64-release\tests\Release\`

Debug 配置把 `-Preset` 换成 `x64-debug` 即可，两条预设的目录互不干扰。

## 4. 冒烟运行

```powershell
.\.build\x64-release\samples\Release\nopeui_window_smoke.exe
.\.build\x64-release\samples\Release\nopeui_xaml_smoke.exe
```

第一条应该弹出一个 900×640 的表单窗口，标题带最近一帧耗时；第二条加载 `page.xaml`，窗口标题是 `NopeUI XAML Smoke`。两个窗口都能正常拖动、缩放、切主题。

目标机器需要 VC++ 运行库（VS 安装时自带；干净的预览机要单独装 x64 版运行库）。

## 5. 验证

按顺序跑，全部要在报告里留下输出。

```powershell
.\eng\test.ps1 -Preset x64-release        # 18 个单元测试
.\eng\golden.ps1 -Preset x64-release      # 24 张黄金图与基线比对
.\eng\uia.ps1 -Preset x64-release         # UIA 全量走查，44 项检查
.\eng\perf.ps1 -SkipBuild -Compare        # 启动与主题切换对照基线
.\eng\analyze.ps1                          # MSVC /analyze，默认只报告
```

几点说明：

- 黄金图基线在 `tests/golden/baseline`，当前图与差异图写到 `.build/golden`。换了显卡或缩放比例出现差异时先看差异图，别直接 `-Update`。
- `eng/uia.ps1` 会自己起 `nopeui_window_smoke` 并要求一个可交互的桌面会话；远程或纯虚拟显示器上把 `-TimeoutSeconds` 调大。
- `eng/perf.ps1 -Compare` 只有在 `tests/perf/baseline.json` 的预设与渲染路径都一致时才比数字；换机器要重新用 `-Out` 生成基线。
- `eng/analyze.ps1` 默认失败也不返回非零，要当门禁用加 `-MaxWarnings 0`。
- 模糊测试：`.\eng\fuzz.ps1 -SkipBuild -Hours 0.05` 先跑三分钟确认工具可用，正式回归用 `-Hours 24`，崩溃样本落在 `.build/fuzz/crash.xaml`。
- 覆盖率需要外部安装 OpenCppCoverage，脚本不代为下载，缺工具时会说明安装方式。

## 6. 常见问题

- **CMake 版本不符**：报错信息里会列出找到的版本，按第 1 节装 4.4.4 或设 `NOPEUI_CMAKE`。
- **LNK1104 打不开 exe**：上一个样例进程还在跑，先 `Stop-Process -Name nopeui_window_smoke` 再构建。
- **黄金图逐像素失败但肉眼看不出**：多半是显示器缩放或驱动抗锯齿差异，用 `nopeui_image_diff` 看差异区域再决定是否重建基线。
- **UIA 脚本报找不到窗口**：确认桌面会话可交互，且没有别的同名进程占着。

## 7. 还没覆盖的部分

预览包的组装还没有脚本，目前只能按第 3 步构建、按第 4 步运行。打包命令与包内清单会在 `eng/` 下补上后补进这份文档，预览 0.1 发布前必须补齐。
