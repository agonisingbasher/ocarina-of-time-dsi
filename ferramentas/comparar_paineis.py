"""Compara duas capturas do painel: md5, pixels diferentes e caixa.

Uso:
    python ferramentas/comparar_paineis.py a.png b.png

O md5 responde "é a mesma imagem?"; a contagem e a caixa dos pixels diferentes
respondem "onde muda e quanto". Duas capturas do mesmo quadro dão md5 igual;
um A/B de um interruptor deve mudar só a região que o interruptor governa, e a
caixa é o que permite dizer se foi só ela.
"""

import hashlib
import sys

import medir_painel as M


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pa, pb = sys.argv[1], sys.argv[2]
    for p in (pa, pb):
        print("%s  md5=%s" % (p, hashlib.md5(open(p, "rb").read()).hexdigest()))
    wa, ha, cta, cha, ra, plta = M.read_png(pa)
    wb, hb, ctb, chb, rb, pltb = M.read_png(pb)
    if (wa, ha) != (wb, hb):
        raise SystemExit("tamanhos diferentes: %dx%d vs %dx%d" % (wa, ha, wb, hb))
    diff = 0
    minx = miny = 1 << 30
    maxx = maxy = -1
    ia = M.pixels(wa, ha, cta, cha, ra, plta)
    ib = M.pixels(wb, hb, ctb, chb, rb, pltb)
    for y in range(ha):
        for x in range(wa):
            if next(ia) != next(ib):
                diff += 1
                minx = min(minx, x)
                maxx = max(maxx, x)
                miny = min(miny, y)
                maxy = max(maxy, y)
    if diff:
        print("diff=%d de %d  bbox x %d..%d y %d..%d"
              % (diff, wa * ha, minx, maxx, miny, maxy))
    else:
        print("diff=0 de %d" % (wa * ha))
    return 0


if __name__ == "__main__":
    sys.exit(main())
