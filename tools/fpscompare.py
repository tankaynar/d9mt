#!/usr/bin/env python3
"""Compare per-second fps from GTA SA runs (wined3d `+fps` lines or d9mt `d9mt fps:` lines).

usage: fpscompare.py [--from S] [--to S] label=path/to/x87run.log [label=path ...]

--from/--to select a window in seconds counted from the first fps line of each log
(drop menus/loading). Without them the whole log is used; the per-second strip helps choosing.
"""
import re
import statistics
import sys

FPS_RE = re.compile(r"approx\s+([\d.]+)\s*fps(?:\s+worst=([\d.]+)ms)?")


def load(path):
    fps, worst = [], []
    with open(path, errors="replace") as f:
        for line in f:
            m = FPS_RE.search(line)
            if m:
                fps.append(float(m.group(1)))
                worst.append(float(m.group(2)) if m.group(2) else None)
    return fps, worst


def pct(vals, p):
    s = sorted(vals)
    k = max(0, min(len(s) - 1, int(round(p / 100 * (len(s) - 1)))))
    return s[k]


def strip(vals, width=60):
    # one char per second, block height by fps relative to 0..max(70, max)
    blocks = " ▁▂▃▄▅▆▇█"
    top = max(70.0, max(vals) if vals else 1)
    out = "".join(blocks[min(8, int(v / top * 8))] for v in vals)
    return "\n      ".join(out[i:i + width] for i in range(0, len(out), width))


def main(argv):
    a, b = None, None
    runs = []
    it = iter(argv)
    for arg in it:
        if arg == "--from":
            a = int(next(it))
        elif arg == "--to":
            b = int(next(it))
        elif "=" in arg:
            label, path = arg.split("=", 1)
            runs.append((label, path))
        else:
            sys.exit(__doc__)
    if not runs:
        sys.exit(__doc__)

    print(f"{'run':<14}{'secs':>5}{'avg':>8}{'median':>8}{'p5':>8}{'min':>8}{'max':>8}{'worst frame':>13}")
    strips = []
    for label, path in runs:
        fps, worst = load(path)
        fps, worst = fps[a:b], worst[a:b]
        if not fps:
            print(f"{label:<14} no fps lines in {path}")
            continue
        w = [x for x in worst if x is not None]
        wtxt = f"{max(w):.0f} ms" if w else "n/a"
        print(f"{label:<14}{len(fps):>5}{statistics.mean(fps):>8.1f}{statistics.median(fps):>8.1f}"
              f"{pct(fps, 5):>8.1f}{min(fps):>8.1f}{max(fps):>8.1f}{wtxt:>13}")
        strips.append((label, fps))
    print()
    for label, fps in strips:
        print(f"{label}:\n      {strip(fps)}")


if __name__ == "__main__":
    main(sys.argv[1:])
