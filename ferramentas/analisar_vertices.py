"""Cores de vértice das listas de uma sala do OoT.

Uso:
    python ferramentas/analisar_vertices.py sala_room_0 [--root DIR] [--por-entrada]

Percorre o `RoomShape` como `analisar_combiner.py`, mantém o buffer de 80 slots
do decodificador e fica com **todo vértice carregado** por `G_VTX` no segmento 3,
não só o que sobrou no buffer no fim: a lista recarrega o buffer e contá-lo uma
vez só esconderia a maior parte da paleta. Reporta, sobre os vértices carregados:

  * quantos têm azul acima de vermelho (canal `cn` interpretado como cor);
  * as cores mais comuns e a luminância média Rec.601;
  * quantos são branco puro (o que decide se `SHADE` pode virar branco);
  * quantos `G_MOVEWORD` a lista emite, separando `G_MW_LIGHTCOL`
    (`G_SETLIGHT`/`G_DIRLIGHT`), que é onde vive a iluminação do RSP.

O layout de vértice é o do F3DEX2, 16 bytes:
`s16 ob[3]; s16 flag; s16 tc[2]; u8 cn[4]`, com `cn` em RGBA.
"""

import argparse
import struct
from collections import Counter
from pathlib import Path

OP_ENDDL = 0xDF
OP_DL = 0xDE
OP_TRI1 = 0x05
OP_TRI2 = 0x06
OP_MOVEWORD = 0xDB
OP_VTX = 0x01

MW_LIGHTCOL = 0x0A  # G_MW_LIGHTCOL
VTX_MAX = 80  # o mesmo `F3_VTX_MAX` do decodificador do port


class Walk:
    def __init__(self, data):
        self.data = data
        self.vtx = [None] * VTX_MAX
        self.loaded = []
        self.refs = Counter()
        self.movewords = 0
        self.lightcmds = 0
        self.external = Counter()

    def load_vtx(self, w0, w1):
        v0n = (w0 >> 1) & 0x7F
        n = (w0 >> 12) & 0xFF
        if n == 0 or n > VTX_MAX or v0n < n or v0n > VTX_MAX:
            return
        v0 = v0n - n
        seg = (w1 >> 24) & 0xFF
        # Só o segmento 3 (a própria sala) interessa aqui; outros são
        # referências externas e ficam contadas em vez de lidas.
        if seg != 3:
            self.external[seg] += n
            return
        off = w1 & 0x00FFFFFF
        for i in range(n):
            p = off + i * 16
            if p + 16 > len(self.data):
                return
            self.vtx[v0 + i] = self.data[p:p + 16]
            self.loaded.append(self.vtx[v0 + i])

    def tri(self, a, b, c):
        for i in (a >> 1, b >> 1, c >> 1):
            if i < VTX_MAX:
                self.refs[i] += 1

    def run(self, off, depth=0):
        pos = off
        seen = 0
        while pos + 8 <= len(self.data):
            w0, w1 = struct.unpack_from(">II", self.data, pos)
            op = w0 >> 24
            seen += 1
            if seen > 60000:
                return
            if op == OP_ENDDL:
                return
            if op == OP_VTX:
                self.load_vtx(w0, w1)
            elif op == OP_TRI1:
                self.tri((w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF)
            elif op == OP_TRI2:
                self.tri((w0 >> 16) & 0xFF, (w0 >> 8) & 0xFF, w0 & 0xFF)
                self.tri((w1 >> 16) & 0xFF, (w1 >> 8) & 0xFF, w1 & 0xFF)
            elif op == OP_MOVEWORD:
                self.movewords += 1
                if ((w0 >> 16) & 0xFF) == MW_LIGHTCOL:
                    self.lightcmds += 1
            elif op == OP_DL:
                nseg = (w1 >> 24) & 0xFF
                if nseg == 3 and depth < 8:
                    self.run(w1 & 0x00FFFFFF, depth + 1)
                else:
                    self.external[nseg] += 1
            pos += 8


def entries(data):
    mesh = None
    for pos in range(0, len(data) - 8, 8):
        if data[pos] == 0x14:
            break
        if data[pos] == 0x0A:
            mesh = struct.unpack_from(">I", data, pos + 4)[0] & 0xFFFFFF
    if mesh is None:
        return None
    stype = data[mesh]
    ent = struct.unpack_from(">I", data, mesh + 4)[0] & 0xFFFFFF
    ent_end = struct.unpack_from(">I", data, mesh + 8)[0] & 0xFFFFFF
    stride = 8 if stype == 0 else 16
    count = (ent_end - ent) // stride if ent_end > ent else 0
    return stype, ent, count, stride


def analyse(path, per_entry=False):
    data = Path(path).read_bytes()
    e = entries(data)
    if e is None:
        return None
    stype, ent, count, stride = e
    if stype not in (0, 2):
        return {"stage": stype}
    opa_at, xlu_at = (8, 12) if stype == 2 else (0, 4)
    out = {"stage": stype, "entries": count, "groups": []}
    for i in range(count):
        base = ent + i * stride
        w = Walk(data)
        for at in (opa_at, xlu_at):
            ptr = struct.unpack_from(">I", data, base + at)[0]
            if ptr:
                w.run(ptr & 0x00FFFFFF)
        used = w.loaded
        g = {"index": i, "refs": len(used), "loads": sum(w.refs.values())}
        if used:
            cols = [tuple(v[12:16]) for v in used]
            lum = [(299 * r + 587 * g2 + 114 * b) // 1000 for r, g2, b, _ in cols]
            g["blue_gt_red"] = sum(1 for r, _, b, _ in cols if b > r)
            g["white"] = sum(1 for c in cols if c == (255, 255, 255, 255))
            g["top"] = Counter(cols).most_common(5)
            TOP.update(cols)
            g["mean_lum"] = sum(lum) / len(lum)
            g["lum_hist"] = Counter("0-31" if x < 32 else "32-63" if x < 64
                                    else "64-127" if x < 128 else "128-191"
                                    if x < 192 else "192-255" for x in lum)
        g["movewords"] = w.movewords
        g["lightcmds"] = w.lightcmds
        g["external"] = dict(w.external)
        out["groups"].append(g)
        if per_entry:
            print("  entrada %d: vértices carregados %d, referências %d, "
                  "MOVEWORD %d (luz %d), externos %s"
                  % (i, g["refs"], g["loads"], g["movewords"], g["lightcmds"],
                     g["external"] or "-"))
    return out


TOP = Counter()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("room")
    ap.add_argument("--root", default="game/oot/extracted/ntsc-1.2/baserom")
    ap.add_argument("--por-entrada", action="store_true")
    args = ap.parse_args()
    r = analyse(Path(args.root) / args.room, args.por_entrada)
    if r is None:
        print("sem SCENE_CMD_MESH")
        return 1
    if "groups" not in r:
        print("%s: RoomShape tipo %s, sem geometria" % (args.room, r["stage"]))
        return 0
    tot = Counter()
    lum_sum = 0.0
    lum_w = 0
    print("%s: tipo %d, %d entradas" % (args.room, r["stage"], r["entries"]))
    for g in r["groups"]:
        if not g["refs"]:
            continue
        tot["refs"] += g["refs"]
        tot["blue"] += g["blue_gt_red"]
        tot["white"] += g["white"]
        lum_sum += g["mean_lum"] * g["refs"]
        lum_w += g["refs"]
        tot["movewords"] += g["movewords"]
        tot["lightcmds"] += g["lightcmds"]
        print("  azul>vermelho %d de %d | branco puro %d | L média %.1f | "
              "MOVEWORD %d (luz %d) | topo %s"
              % (g["blue_gt_red"], g["refs"], g["white"], g["mean_lum"],
                 g["movewords"], g["lightcmds"], g["top"]))
        print("  histograma de L: %s" % dict(g["lum_hist"]))
    print("total: %d vértices carregados, %d azul>vermelho, %d branco puro, "
          "L média %.1f (%.1f %% de 255), MOVEWORD %d (luz %d)"
          % (tot["refs"], tot["blue"], tot["white"],
             lum_sum / lum_w if lum_w else 0.0,
             100.0 * (lum_sum / lum_w) / 255.0 if lum_w else 0.0,
             tot["movewords"], tot["lightcmds"]))
    print("cores mais comuns (salas inteiras): %s" % TOP.most_common(6))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
