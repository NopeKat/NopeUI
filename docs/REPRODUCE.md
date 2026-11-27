# 从干净机器复现构建

这份文档描述如何在一台干净的 Windows 机器上，从源码构建、验证并运行 NopeUI。所有步骤都是本地手动执行的命令，项目没有任何 CI。

## 1. 环境要求

- Windows 10 或 Windows 11，x64。当前只有 x64 目标。
- PowerShell 7（`pwsh`），Windows 自带的 5.1 不保证可用。
- Visual Studio 18 2026，安装工作负载「使用 C++ 的桌面开发」。
- Windows SDK 10.0.26100.0（在 VS 安装器里勾选「Windows 11 SDK (10.0.26100.0)」）。
- CMake 4.4.4。生成器固定为 `Visual Studio 18 2026`，不使用 Ninja。

工具链版本是硬约束：`tools/scripts/toolchain.ps1` 会检查 `cmake --version` 是否为 4.4.4，不符直接报错，不会自动降级或下载。

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
.\tools\scripts\build.ps1 -Preset x64-release
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
.\tools\scripts\test.ps1 -Preset x64-release        # 18 个单元测试
.\tools\scripts\golden.ps1 -Preset x64-release      # 28 张黄金图与基线比对
.\tools\scripts\uia.ps1 -Preset x64-release         # UIA 全量走查，44 项检查
.\tools\scripts\perf.ps1 -SkipBuild -Compare        # 启动与主题切换对照基线
.\tools\scripts\analyze.ps1                          # MSVC /analyze，默认只报告
```

几点说明：

- 黄金图基线在 `tests/golden/baseline`，当前图与差异图写到 `.build/golden`。换了显卡或缩放比例出现差异时先看差异图，别直接 `-Update`。
- 二十六张基线不在仓库里，都放在工作区 `assets/golden`；仓库里只剩占位场景的浅色、深色两张软件基线。单独克隆内层仓库时用 `NOPEUI_GOLDEN_ASSETS` 指向它，否则这些项会报缺基线。
- `tools/scripts/uia.ps1` 会自己起 `nopeui_window_smoke` 并要求一个可交互的桌面会话；远程或纯虚拟显示器上把 `-TimeoutSeconds` 调大。
- `tools/scripts/perf.ps1 -Compare` 只有在 `tests/perf/baseline.json` 的预设与渲染路径都一致时才比数字；换机器要重新用 `-Out` 生成基线。
- `tools/scripts/analyze.ps1` 默认失败也不返回非零，要当门禁用加 `-MaxWarnings 0`。
- 模糊测试：`.\tools\scripts\fuzz.ps1 -SkipBuild -Hours 0.05` 先跑三分钟确认工具可用，正式回归用 `-Hours 24`，崩溃样本落在 `.build/fuzz/crash.xaml`。
- 覆盖率需要外部安装 OpenCppCoverage，脚本不代为下载，缺工具时会说明安装方式。

## 6. 常见问题

- **CMake 版本不符**：报错信息里会列出找到的版本，按第 1 节装 4.4.4 或设 `NOPEUI_CMAKE`。
- **LNK1104 打不开 exe**：上一个样例进程还在跑，先 `Stop-Process -Name nopeui_window_smoke` 再构建。
- **黄金图逐像素失败但肉眼看不出**：多半是显示器缩放或驱动抗锯齿差异，用 `nopeui_image_diff` 看差异区域再决定是否重建基线。
- **UIA 脚本报找不到窗口**：确认桌面会话可交互，且没有别的同名进程占着。

## 7. 打包预览

```powershell
.\tools\scripts\package.ps1                     # 构建并打包
.\tools\scripts\package.ps1 -SkipBuild -Smoke   # 复用现有构建，并启动样例做 3 秒冒烟
```

脚本要求工作区干净：包内的源码包取自 `HEAD`，源码必须和二进制对得上。本地试验可以加 `-AllowDirty`，它会明显提示源码包来自 HEAD。

产物在 `.build\package`：

- `NopeUI-<版本>-win-x64.zip`：便携包，解压即用。
- `NopeUI-<版本>-win-x64.sha256`：包与两个样例 exe 的校验和。

包内结构：

```
NopeUI-0.0.0.0-win-x64/
├─ include/nopeui/          公共头文件
├─ lib/x64/static/md/       七个静态库，动态 CRT 构建
├─ bin/x64/                 两个样例 exe 与 page.xaml
├─ docs/REPRODUCE.md
├─ source/nopeui-<版本>-source.zip   对应 HEAD 的源码包
├─ package-info.txt         版本、构建日期、变体与工具链
├─ LICENSE
└─ README.md
```

版本号从 `src/base/version.cpp` 读，不单独传参。`bin\x64` 里的 `page.xaml` 必须和 exe 放在一起，`nopeui_xaml_smoke` 会在同目录找它。

这一版预览只发 x64 静态库 `/MD` 变体；动态库、静态 CRT 与 ARM64 属于季度全变体 SDK，还没做。
