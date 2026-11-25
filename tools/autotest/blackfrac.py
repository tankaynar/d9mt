# blackfrac.py shot.png x0,y0,x1,y1 ... — fraction of near-black pixels per region (for automated bisection)
import subprocess, sys, struct
p = sys.argv[1]; bmp = p.rsplit('.', 1)[0] + '.tmp.bmp'
subprocess.run(['sips', '-s', 'format', 'bmp', p, '--out', bmp], capture_output=True)
b = open(bmp, 'rb').read(); off = struct.unpack_from('<I', b, 10)[0]; w, h = struct.unpack_from('<ii', b, 18)
bpp = struct.unpack_from('<H', b, 28)[0] // 8; row = (w * bpp + 3) & ~3; top = h < 0; h = abs(h)
out = []
for r in sys.argv[2:]:
    x0, y0, x1, y1 = map(int, r.split(','))
    n = k = 0
    for y in range(y0, y1, 4):
        for x in range(x0, x1, 4):
            yy = y if top else h - 1 - y; o = off + yy * row + x * bpp
            n += 1; k += (b[o] + b[o + 1] + b[o + 2]) < 30
    out.append(round(k / n, 3))
import os; os.remove(bmp)
print(*out)
