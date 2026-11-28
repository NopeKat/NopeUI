# Scroll Frame Baseline

- Date: 2026-11-28
- Machine: Microsoft Windows 11 Professional / Intel(R) Core(TM) i7-7700K CPU @ 4.20GHz / 31.9 GB
- Preset: x64-release, render: hardware, synchronous commit, commit throttle off
- Window: 900x600 DIP at dpi 192, parked mostly off screen so the run does not occupy the desktop
- Command: `nopeui_frame_benchmark --full --unpaced --no-throttle --virtual --scroll 8 --text <rows>`

Each run measures 300 whole frames after 30 warmup frames. The virtual list draws only
rows inside the viewport, so the row count changes the culling math rather than the paint
cost; the three tiers exist to show the frame rate stays flat as the list grows.
Throughput is `1000 / avg`, where `avg` is the per-frame time the benchmark reports and
includes the commit.

Runs that ended with more than 300 attempts, or with dropped commits, were discarded and
repeated; only clean 300-frame runs are listed. The measurement ran while the 24 hour
XAML fuzz run was holding one core, so the numbers carry that load and a quiet machine can
only come out higher.

| Rows | Run 1 | Run 2 | Run 3 | Median | p99 frame |
| ---- | ----- | ----- | ----- | ------ | --------- |
| 10000 | 110.1 | 100.2 | 97.9 | 100.2 | 25.8 ms |
| 50000 | 105.3 | 121.8 | 116.2 | 116.2 | 25.2 ms |
| 100000 | 98.6 | 98.5 | 103.8 | 98.6 | 25.3 ms |

All values are frames per second. The 100000 row case holds a median of 98.6 fps, well
above the 60 fps floor. The p99 frame time of about 25 ms comes from single frames waiting
on a commit, not from a sustained drop.

The three medians above are recorded in `baseline.json` under `scroll_frames`. A rerun on
the same day on a 24 Hz output produced no committed frames in unpaced mode, so these
numbers stay the reference until a 144 Hz display is available again.

This record is not gated by `perf.ps1`; it sits beside `baseline.json`, which covers
startup, theme switching and the fixed 120 Hz frame path.

## Raw Output

```
rows=10000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=10000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=9.083 ms p99=25.702 ms min=1.058 ms max=30.844 ms drain=0.0 ms begin=0.001/0.003 draw=0.747/1.473 end=0.000/0.000 commit=8.303/24.816 queue=0.000/0.000 present=3.248/6.848 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=10000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=10000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=9.983 ms p99=25.785 ms min=1.035 ms max=27.341 ms drain=0.0 ms begin=0.002/0.005 draw=0.819/1.490 end=0.000/0.000 commit=9.125/24.751 queue=0.000/0.000 present=3.375/6.862 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=10000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=10000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=10.218 ms p99=29.305 ms min=1.109 ms max=31.619 ms drain=0.0 ms begin=0.001/0.003 draw=0.852/1.598 end=0.000/0.000 commit=9.331/28.547 queue=0.000/0.000 present=3.386/6.887 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=50000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=50000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=9.496 ms p99=25.907 ms min=1.100 ms max=26.445 ms drain=0.0 ms begin=0.001/0.003 draw=0.750/1.402 end=0.000/0.000 commit=8.712/25.158 queue=0.000/0.000 present=3.368/6.770 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=50000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=50000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=8.212 ms p99=25.156 ms min=1.046 ms max=27.795 ms drain=0.0 ms begin=0.001/0.003 draw=0.669/1.335 end=0.000/0.000 commit=7.514/23.902 queue=0.000/0.000 present=3.390/6.880 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=50000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=50000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=8.606 ms p99=25.070 ms min=1.058 ms max=26.717 ms drain=0.0 ms begin=0.001/0.003 draw=0.703/1.455 end=0.000/0.000 commit=7.873/24.339 queue=0.000/0.000 present=3.585/6.893 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=100000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=100000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=10.147 ms p99=25.599 ms min=1.087 ms max=30.367 ms drain=0.0 ms begin=0.001/0.004 draw=0.812/1.612 end=0.000/0.000 commit=9.296/24.509 queue=0.000/0.000 present=3.593/6.906 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=100000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=100000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=10.153 ms p99=25.026 ms min=1.095 ms max=26.922 ms drain=0.0 ms begin=0.001/0.003 draw=0.809/1.487 end=0.000/0.000 commit=9.306/24.251 queue=0.000/0.000 present=3.588/6.864 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
rows=100000 content=full pace=unpaced throttle=off async=off target_hz=0 dwm_refresh=144.0 load_rects=0 load_text=100000 virtual=1 scroll=8.00 frames=300 attempts=300 commits=333 avg=9.634 ms p99=25.333 ms min=1.085 ms max=26.813 ms drain=0.0 ms begin=0.001/0.004 draw=0.777/1.489 end=0.000/0.000 commit=8.823/24.409 queue=0.000/0.000 present=3.540/6.905 submitted=0 completed=0 dropped=0 waits=0 wait_avg=0.000 maxq=0
```
