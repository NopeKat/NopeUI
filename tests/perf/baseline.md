# Performance Baseline

- Date: 2026-11-27
- Machine: Microsoft Windows 11 Pro / Intel(R) Core(TM) i7-7700K CPU @ 4.20GHz / 31.9 GB
- Preset: x64-release, render: hardware, async submit: on
- Startup: 5 runs, 900x640 form page, min 266.7 ms / median 288.3 ms / max 291.8 ms
- Theme switching: 200 cycles, alternating light and dark

Startup total includes process creation and loader time; theme switching was measured with commit throttling off and excludes vblank waits.

## Raw Output

```
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=288.8 ms submit=288.8 entry=38.2 create=141.2 content=6.1 show=103.3 settle=0.0 present_wait=0.0 draw=0.6 begin=0.0 end=0.0 commit=3.8 present=3.8 commits=3 frames=9 ws=39.6 MB ws_peak=48.0 MB private_peak=13.3 MB ws_peak_first=48.0 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=266.7 ms submit=266.7 entry=28.9 create=147.1 content=7.7 show=83.0 settle=0.0 present_wait=0.0 draw=0.7 begin=0.0 end=0.0 commit=5.3 present=5.3 commits=3 frames=9 ws=39.4 MB ws_peak=47.8 MB private_peak=13.1 MB ws_peak_first=47.8 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=288.3 ms submit=288.3 entry=32.4 create=141.9 content=6.3 show=107.8 settle=0.0 present_wait=0.0 draw=0.6 begin=0.0 end=0.0 commit=22.0 present=4.3 commits=3 frames=9 ws=39.3 MB ws_peak=47.7 MB private_peak=13.0 MB ws_peak_first=47.7 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=291.8 ms submit=291.8 entry=32.5 create=131.7 content=7.2 show=120.4 settle=0.0 present_wait=0.0 draw=0.5 begin=0.0 end=0.0 commit=26.8 present=5.9 commits=3 frames=9 ws=39.6 MB ws_peak=48.0 MB private_peak=13.3 MB ws_peak_first=48.0 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=272.3 ms submit=272.3 entry=35.9 create=144.3 content=5.3 show=86.8 settle=0.0 present_wait=0.0 draw=1.4 begin=0.0 end=0.0 commit=8.9 present=1.7 commits=3 frames=9 ws=39.2 MB ws_peak=47.6 MB private_peak=13.0 MB ws_peak_first=47.6 MB
theme switches=200 cycle=light-dark async=on render=hardware throttle=off dpi=192 apply=9.646/26.288/28.031 settle=0.001/0.002/0.003 total=9.647/26.288/28.031 first=1.741 draw=0.627/1.364 commit=8.916/25.535 present=3.683/6.867 ws=41.4 MB ws_peak=47.9 MB private_peak=15.1 MB submitted=2 completed=1 dropped=0
```
