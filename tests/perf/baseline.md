# 性能基线

- 日期：2026-11-27
- 机器：Microsoft Windows 11 专业版 / Intel(R) Core(TM) i7-7700K CPU @ 4.20GHz / 31.9 GB
- 预设：x64-release，渲染：硬件，异步提交：开
- 启动：5 次，900×640 表单页，min 256.4 ms / median 271.5 ms / max 343.8 ms
- 主题切换：200 次，浅色与深色互切

启动 total 含进程创建与加载器；主题切换在关闭提交节流后测量，不含 vblank 等待。

## 原始输出

```
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=264.0 ms submit=264.0 entry=23.4 create=130.9 content=6.0 show=103.7 settle=0.0 present_wait=0.0 draw=0.7 begin=0.0 end=0.0 commit=13.1 present=4.3 commits=3 frames=9 ws=39.5 MB ws_peak=47.9 MB private_peak=13.2 MB ws_peak_first=47.9 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=343.8 ms submit=343.8 entry=25.1 create=188.4 content=6.0 show=124.3 settle=0.0 present_wait=0.0 draw=1.0 begin=0.0 end=0.0 commit=21.6 present=1.4 commits=3 frames=9 ws=39.5 MB ws_peak=47.9 MB private_peak=13.2 MB ws_peak_first=47.9 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=256.4 ms submit=256.4 entry=33.6 create=132.1 content=7.9 show=82.8 settle=0.0 present_wait=0.0 draw=0.6 begin=0.0 end=0.0 commit=2.0 present=5.4 commits=4 frames=9 ws=40.1 MB ws_peak=48.5 MB private_peak=13.8 MB ws_peak_first=48.5 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=271.5 ms submit=271.5 entry=38.5 create=133.2 content=5.4 show=94.4 settle=0.0 present_wait=0.0 draw=1.0 begin=0.0 end=0.0 commit=6.4 present=4.7 commits=3 frames=9 ws=39.5 MB ws_peak=47.9 MB private_peak=13.2 MB ws_peak_first=47.9 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=292.4 ms submit=292.4 entry=35.4 create=126.8 content=5.5 show=124.7 settle=0.0 present_wait=0.0 draw=0.6 begin=0.0 end=0.0 commit=2.0 present=4.4 commits=3 frames=9 ws=39.4 MB ws_peak=47.8 MB private_peak=13.1 MB ws_peak_first=47.8 MB
theme switches=200 cycle=light-dark async=on render=hardware throttle=off dpi=192 apply=8.870/25.401/28.363 settle=0.001/0.002/0.003 total=8.871/25.401/28.364 first=1.361 draw=0.529/1.256 commit=8.255/24.927 present=3.472/6.873 ws=41.4 MB ws_peak=47.9 MB private_peak=15.2 MB submitted=1 completed=1 dropped=0
```
