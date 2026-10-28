#!/usr/bin/env python3
"""Census of the alpha bit of the second image in the two-texture lerp family.

The DS takes a pixel's alpha from the texel, and topInit() runs the cutout at
the lowest threshold for the whole frame, so a second image whose every texel
has the alpha bit clear can never be drawn by the second pass. The RDP does not
care, because this combiner's output alpha comes from the alpha mux
((0-0)*0+1, i.e. opaque), not from TEXEL1.

The census walks every room, follows G_DL inside the room segment, attributes
each G_TRI1/G_TRI2 to the G_SETCOMBINE in force, keeps the triangles whose
cycle 0 is (TEXEL1-TEXEL0)*ENVA+TEXEL0, and for each of them looks at the second
tile the list described: its format, which segment the image lives in, and
whether any texel of it has the alpha bit set. Second images live either in the
room file (segment 3) or in the scene file (segment 2); the scene is read
through the matching <scene>_scene file.

Usage:
  python ferramentas/analisar_lerp_alpha.py
  python ferramentas/analisar_lerp_alpha.py --root game/oot/extracted/ntsc-1.2/baserom
"""

import argparse
import os
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analisar_combiner as A


def scan(path, root):
    name = os.path.basename(str(path))
    scene_path = Path(root) / (name.split("_room_")[0] + "_scene")
    scene = scene_path.read_bytes() if scene_path.exists() else None
    data = Path(path).read_bytes()
    e = A.entries(data)
    if e is None:
        return None
    st, ent, cnt, stride = e
    if st not in (0, 2):
        return None
    opa, xlu = (8, 12) if st == 2 else (0, 4)
    res = Counter()
    img = [None, None]
    tile = {0: None, 1: None}
    cc = None

    def walk(ptr, depth):
        nonlocal cc
        pos = ptr
        guard = 0
        while pos + 8 <= len(data) and guard < 60000:
            guard += 1
            w0, w1 = struct.unpack_from(">II", data, pos)
            op = w0 >> 24
            if op == 0xDF:
                return
            if op == 0xDE:
                # A room's entry list splits its geometry into sublists that a
                # linear walk never reaches; without following them the census
                # would see only part of the family.
                if depth < 8 and ((w1 >> 24) & 0xFF) == 3:
                    walk(w1 & 0xFFFFFF, depth + 1)
            elif op == 0xFC:
                cc = A.decode_combine(w0, w1)
            elif op == 0xFD:
                img[0] = w1
                img[1] = w1
            elif op == 0xF5:
                t = (w1 >> 24) & 7
                if t in (0, 1):
                    tile[t] = {"fmt": (w0 >> 21) & 7, "siz": (w0 >> 19) & 3}
            elif op == 0xF2:
                t = (w1 >> 24) & 7
                if t in (0, 1) and tile[t] is not None:
                    uls = (w0 >> 12) & 0xFFF
                    ult = w0 & 0xFFF
                    lrs = (w1 >> 12) & 0xFFF
                    lrt = w1 & 0xFFF
                    tile[t]["w"] = ((lrs - uls) >> 2) + 1
                    tile[t]["h"] = ((lrt - ult) >> 2) + 1
                    tile[t]["img"] = img[t]
            elif op in (0x05, 0x06):
                n = 1 if op == 0x05 else 2
                if cc and cc["a0"] == 2 and cc["b0"] == 1 and cc["c0"] == 12:
                    t1 = tile[1]
                    if t1 is None or "w" not in t1 or t1.get("img") is None:
                        res["sem_descritor"] += n
                    elif t1["fmt"] != 0 or t1["siz"] != 2:
                        res["nao_rgba16"] += n
                    else:
                        a = t1["img"]
                        seg = a >> 24
                        off = a & 0xFFFFFF
                        src = data if seg == 3 else (scene if seg == 2 else None)
                        if src is None or off + t1["w"] * t1["h"] * 2 > len(src):
                            res["imagem_nao_resolvida"] += n
                        else:
                            bits = 0
                            for k in range(t1["w"] * t1["h"]):
                                if struct.unpack_from(">H", src, off + 2 * k)[0] & 0x8000:
                                    bits += 1
                                    break
                            key = "t1_todo_alfa0" if bits == 0 else "t1_tem_alfa"
                            res[key] += n
                            res[("seg3_" if seg == 3 else "seg2_") +
                                ("alfa0" if bits == 0 else "alfa")] += n
            pos += 8

    for i in range(cnt):
        for at in (opa, xlu):
            ptr = struct.unpack_from(">I", data, ent + i * stride + at)[0] & 0xFFFFFF
            if not ptr:
                continue
            # The tile descriptor is written inside the list, so every entry
            # starts with nothing described.
            img[0] = img[1] = None
            tile[0] = tile[1] = None
            cc = None
            walk(ptr, 0)
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="game/oot/extracted/ntsc-1.2/baserom")
    args = ap.parse_args()
    root = Path(args.root)

    tot = Counter()
    for p in sorted(root.iterdir()):
        if "_room_" not in p.name:
            continue
        r = scan(p, root)
        if r:
            tot.update(r)
    allt = sum(v for k, v in tot.items() if not k.startswith("seg"))
    print("triangulos do lerp com ciclo-0 (TEXEL1-TEXEL0)*ENVA+TEXEL0: %d" % allt)
    for k, v in tot.most_common():
        print("  %6d  %5.1f%%  %s" % (v, 100.0 * v / allt, k))
    return 0


if __name__ == "__main__":
    sys.exit(main())
