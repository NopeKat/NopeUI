# Performance Baseline

- Date: 2026-11-28
- Machine: Microsoft Windows 11 Professional / Intel(R) Core(TM) i7-7700K CPU @ 4.20GHz / 31.9 GB
- Preset: x64-release, render: hardware, async submit: on
- Startup: 5 runs, 900x640 form page, min 244.2 ms / median 272.9 ms / max 282.6 ms
- Theme switching: 200 cycles, alternating light and dark
- Scroll: 100000 rows, 8 px steps, avg 1.6 ms / p99 2.8 ms / 626.2 fps
- Frame: 120 Hz target, avg 5.22 ms / p99 11.83 ms / max 12.66 ms, dropped 0

Startup total includes process creation and loader time; theme switching was measured with commit throttling off and excludes vblank waits. Scroll numbers are render time from nopeui_text_list_benchmark, and frame numbers are dirty-rect frames paced at 120 Hz.

## Raw Output

```
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=244.2 ms submit=244.2 entry=27.0 create=113.4 content=5.5 show=98.4 settle=0.0 present_wait=0.0 draw=0.4 begin=0.0 end=0.0 commit=3.6 present=0.3 commits=3 frames=9 ws=39.5 MB ws_peak=47.9 MB private_peak=13.2 MB ws_peak_first=47.9 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=278.2 ms submit=278.2 entry=24.9 create=145.2 content=6.7 show=101.3 settle=0.0 present_wait=0.0 draw=0.6 begin=0.0 end=0.0 commit=12.3 present=2.9 commits=3 frames=9 ws=39.2 MB ws_peak=47.6 MB private_peak=12.9 MB ws_peak_first=47.6 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=270.2 ms submit=270.2 entry=30.9 create=137.9 content=6.6 show=94.8 settle=0.0 present_wait=0.0 draw=1.1 begin=0.0 end=0.0 commit=6.8 present=4.5 commits=3 frames=9 ws=39.6 MB ws_peak=48.0 MB private_peak=13.4 MB ws_peak_first=48.0 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=272.9 ms submit=272.9 entry=28.1 create=134.8 content=5.6 show=104.5 settle=0.0 present_wait=0.0 draw=1.0 begin=0.0 end=0.0 commit=19.8 present=0.7 commits=3 frames=9 ws=39.2 MB ws_peak=47.6 MB private_peak=12.9 MB ws_peak_first=47.6 MB
startup content=form theme=system render=hardware async=on size=900x640 dpi=192 total=282.6 ms submit=282.6 entry=31.8 create=139.0 content=5.2 show=106.7 settle=0.0 present_wait=0.0 draw=1.0 begin=0.0 end=0.0 commit=26.2 present=6.9 commits=3 frames=9 ws=39.3 MB ws_peak=47.7 MB private_peak=13.1 MB ws_peak_first=47.7 MB
theme switches=200 cycle=light-dark async=on render=hardware throttle=off dpi=192 apply=9.042/23.687/26.571 settle=0.001/0.002/0.003 total=9.043/23.688/26.571 first=19.067 draw=0.538/1.183 commit=8.418/22.668 present=3.330/6.915 ws=41.2 MB ws_peak=47.6 MB private_peak=14.8 MB submitted=1 completed=1 dropped=0
text_list requested=900x600 client=1774x1129 dpi=192 awareness=2 surfaces=3 async=on mode=virtual rows=100000 cache=off scroll=8.0 avg=1.597 ms p99=2.836 ms begin=0.053/0.356 draw=1.499/2.679 end=0.031/0.092 commit=2.775/9.418 queue=0.026/0.067 present=5.656/13.860
content=full pace=fixed throttle=on async=off target_hz=120 dwm_refresh=144.0 load_rects=0 load_text=0 virtual=0 scroll=0.00 frames=300 attempts=300 commits=333 avg=5.217 ms p99=11.832 ms min=0.695 ms max=12.656 ms drain=0.0 ms begin=0.007/0.020 draw=0.277/0.655 end=0.000/0.000 commit=4.904/11.460 queue=0.000/0.000 present=2.790/6.860 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
```