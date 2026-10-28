"""Estatísticas de luminância de uma captura do painel (PNG).

Uso:
    python ferramentas/medir_painel.py captura.png [captura2.png ...]

Aceita RGB, RGBA e paleta. A luminância é Rec.601 em milésimos
(L = (299R + 587G + 114B) / 1000). Duas leituras saem juntas: `L`/`L>20` sobre
**todos** os pixels (o preto entra como zero) e `Lnd`/`nd>20` só sobre os
pixels com L ≥ 8, que é a geometria. A primeira compara quadros cuja área
iluminada muda; a segunda compara cor onde há superfície. As frações >48
separam o que continua claro do que escureceu.
"""

import struct
import sys
import zlib


def read_png(path):
    d = open(path, "rb").read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s: não é PNG" % path)
    i = 8
    idat = b""
    w = h = bit = ct = 0
    plte = None
    while i < len(d):
        ln = struct.unpack(">I", d[i:i + 4])[0]
        tag = d[i + 4:i + 8]
        data = d[i + 8:i + 8 + ln]
        i += 12 + ln
        if tag == b"IHDR":
            w, h, bit, ct = struct.unpack(">IIBB", data[:10])
        elif tag == b"IDAT":
            idat += data
        elif tag == b"PLTE":
            plte = data
    if bit != 8:
        raise SystemExit("%s: só 8 bits por canal" % path)
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    raw = zlib.decompress(idat)
    stride = w * ch
    rows = []
    prev = bytearray(stride)
    o = 0
    for _ in range(h):
        f = raw[o]
        o += 1
        line = bytearray(raw[o:o + stride])
        o += stride
        for x in range(stride):
            a = line[x - ch] if x >= ch else 0
            b = prev[x]
            c = prev[x - ch] if x >= ch else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        prev = line
        rows.append(line)
    return w, h, ct, ch, rows, plte


def pixels(w, h, ct, ch, rows, plte):
    for y in range(h):
        line = rows[y]
        for x in range(w):
            o = x * ch
            if ct == 3:
                j = line[x] * 3
                yield plte[j], plte[j + 1], plte[j + 2]
            elif ct in (0, 4):
                g = line[o]
                yield g, g, g
            else:
                yield line[o], line[o + 1], line[o + 2]


def measure(path):
    w, h, ct, ch, rows, plte = read_png(path)
    all_n = 0
    all_l = 0
    all_20 = 0
    n = 0
    sl = 0
    up20 = 0
    up48 = 0
    blue = 0
    red = 0
    for r, g, b in pixels(w, h, ct, ch, rows, plte):
        lum = (299 * r + 587 * g + 114 * b) // 1000
        all_n += 1
        all_l += lum
        all_20 += lum > 20
        if lum < 8:
            continue
        n += 1
        sl += lum
        up20 += lum > 20
        up48 += lum > 48
        blue += b > r + 6 and b > g + 6
        red += r > g + 6 and r > b + 6
    if n == 0 or all_n == 0:
        return w, h, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
    return (w, h, n, all_l / all_n, 100.0 * all_20 / all_n,
            sl / n, 100.0 * up20 / n, 100.0 * up48 / n,
            100.0 * blue / n, 100.0 * red / n)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for path in sys.argv[1:]:
        w, h, n, al, a20, nl, n20, n48, blue, red = measure(path)
        print("%s  %dx%d  px=%d  L=%.1f  L>20=%.1f%%  |  "
              "Lnd=%.1f  nd>20=%.1f%%  nd>48=%.1f%%  |  "
              "azul=%.1f%%  vermelho=%.1f%%"
              % (path, w, h, n, al, a20, nl, n20, n48, blue, red))


if __name__ == "__main__":
    main()
