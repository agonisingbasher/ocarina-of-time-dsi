#!/usr/bin/env python3
"""Le o cabecalho de colisao de uma cena extraida do baserom ntsc-1.2.

O port carrega a colisao estatica da cena (segmento 2) para uma consulta de
chao e de parede; este script le os mesmos bytes fora do emulador, para que o
numero que a tela mostrar tenha uma segunda opiniao independente.

    python ferramentas/analisar_colisao.py [caminho-da-cena]

O padrao e a Bmori1_scene (Inside the Deku Tree) do baserom extraido. Tudo o
que sai daqui e big-endian como no cartucho; os offsets sao relativos ao
inicio do arquivo e o campo de ponteiro e mascarado com 0xFFFFFF (o segmento
nao esta no arquivo).
"""

import argparse
import struct
from collections import Counter
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
PADRAO = RAIZ / "game" / "oot" / "extracted" / "ntsc-1.2" / "baserom" / "Bmori1_scene"

SCENE_CMD_NAMES = {
    0x00: "PLAYER_ENTRY_LIST",
    0x01: "ACTOR_LIST",
    0x02: "UNUSED_2",
    0x03: "COLLISION_HEADER",
    0x04: "ROOM_LIST",
    0x05: "WIND_SETTINGS",
    0x06: "SPAWN_LIST",
    0x07: "SPECIAL_FILES",
    0x08: "ROOM_BEHAVIOR",
    0x09: "UNDEFINED_9",
    0x0A: "ROOM_SHAPE",
    0x0B: "OBJECT_LIST",
    0x0C: "LIGHT_LIST",
    0x0D: "PATH_LIST",
    0x0E: "TRANSITION_ACTOR_LIST",
    0x0F: "LIGHT_SETTINGS_LIST",
    0x10: "TIME_SETTINGS",
    0x11: "SKYBOX_SETTINGS",
    0x12: "SKYBOX_DISABLES",
    0x13: "EXIT_LIST",
    0x14: "END",
    0x15: "SOUND_SETTINGS",
    0x16: "ECHO_SETTINGS",
    0x17: "CUTSCENE_DATA",
    0x18: "ALTERNATE_HEADER_LIST",
    0x19: "MISC_SETTINGS",
}


def be32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def be16(d, o):
    return struct.unpack_from(">H", d, o)[0]


def s16(d, o):
    return struct.unpack_from(">h", d, o)[0]


def off(ptr):
    return ptr & 0xFFFFFF


def scene_cmds(d):
    o = 0
    while o + 8 <= len(d):
        code = d[o]
        yield o, code, d[o + 1], be32(d, o + 4)
        if code == 0x14:
            break
        o += 8


def load(path):
    d = path.read_bytes()
    return d


def read_header(d):
    cmd = {}
    for o, code, data1, data2 in scene_cmds(d):
        cmd.setdefault(code, (o, data1, data2))
    if 0x03 not in cmd:
        return None
    co = off(cmd[0x03][2])
    return {
        "at": co,
        "min": [s16(d, co + 2 * i) for i in range(3)],
        "max": [s16(d, co + 6 + 2 * i) for i in range(3)],
        "nv": be16(d, co + 12),
        "vtx_at": off(be32(d, co + 16)),
        "np": be16(d, co + 20),
        "poly_at": off(be32(d, co + 24)),
        "surf_at": off(be32(d, co + 28)),
        "bgcam_at": off(be32(d, co + 32)),
        "nw": be16(d, co + 36),
        "water_at": off(be32(d, co + 40)),
    }


def poly_vertices(d, h, i):
    po = h["poly_at"] + i * 0x10
    ids = [be16(d, po + 2) & 0x1FFF, be16(d, po + 4) & 0x1FFF,
           be16(d, po + 6) & 0x1FFF]
    return [tuple(s16(d, h["vtx_at"] + k * 6 + 2 * c) for c in range(3))
            for k in ids]


def floor_hits(d, h, x, z, y_below):
    """Mesma conta do Math3D_TriChkPointParaYIntersectInsideTri: o plano vem
    da normal do poligono e a projecao XZ usa os tres determinantes."""
    hits = []
    for i in range(h["np"]):
        po = h["poly_at"] + i * 0x10
        ny = s16(d, po + 10)
        if ny <= 16384:
            continue
        v0, v1, v2 = poly_vertices(d, h, i)
        det01 = (v0[2] - z) * (v1[0] - x) - (v0[0] - x) * (v1[2] - z)
        det12 = (v1[2] - z) * (v2[0] - x) - (v1[0] - x) * (v2[2] - z)
        det20 = (v2[2] - z) * (v0[0] - x) - (v2[0] - x) * (v0[2] - z)
        if not ((300 >= det01 and 300 >= det12 and 300 >= det20) or
                (-300 <= det01 and -300 <= det12 and -300 <= det20)):
            continue
        nx, nz = s16(d, po + 8), s16(d, po + 12)
        dist = s16(d, po + 14)
        y = ((-nx / 32767.0) * x - (nz / 32767.0) * z - dist) / (ny / 32767.0)
        if y <= y_below:
            hits.append((y, i))
    hits.sort(reverse=True)
    return hits


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("cena", nargs="?", default=str(PADRAO))
    ap.add_argument("--floor", nargs=2, type=float, metavar=("X", "Z"),
                    help="consulta o chao mais alto abaixo de --y")
    ap.add_argument("--y", type=float, default=1e9, help="limite superior da consulta")
    args = ap.parse_args()
    path = Path(args.cena)
    d = load(path)
    print(f"arquivo {path.name} {len(d)} bytes")

    cmd = {}
    for o, code, data1, data2 in scene_cmds(d):
        cmd.setdefault(code, (o, data1, data2))
        name = SCENE_CMD_NAMES.get(code, f"?{code:02X}")
        print(f"  cmd @{o:04X} {name:<22} data1={data1:<3} ptr={data2:08X} -> @{off(data2):04X}")

    h = read_header(d)
    if h is None:
        print("sem SCENE_CMD_COLLISION_HEADER")
        return 1
    minx, miny, minz = h["min"]
    maxx, maxy, maxz = h["max"]
    nv, vtx_at = h["nv"], h["vtx_at"]
    np, poly_at = h["np"], h["poly_at"]
    surf_at, bgcam_at = h["surf_at"], h["bgcam_at"]
    nw, water_at = h["nw"], h["water_at"]

    print(f"\ncollision header @{h['at']:04X}")
    print(f"  bounds  x {minx}..{maxx}  y {miny}..{maxy}  z {minz}..{maxz}")
    print(f"  vertices {nv} @{vtx_at:04X}")
    print(f"  polygons {np} @{poly_at:04X}")
    print(f"  surfaceTypes @{surf_at:04X}  bgCam @{bgcam_at:04X}")
    print(f"  waterBoxes {nw} @{water_at:04X}")

    kinds = Counter()
    miny_v = 32767
    maxy_v = -32768
    idx_bad = 0
    for i in range(np):
        po = poly_at + i * 0x10
        ida = be16(d, po + 2) & 0x1FFF
        idb = be16(d, po + 4) & 0x1FFF
        idc = be16(d, po + 6) & 0x1FFF
        if max(ida, idb, idc) >= nv:
            idx_bad += 1
            continue
        ny = s16(d, po + 10)
        if ny > 16384:
            kinds["floor"] += 1
        elif ny < -26214:
            kinds["ceiling"] += 1
        else:
            kinds["wall"] += 1
        for k in (ida, idb, idc):
            y = s16(d, vtx_at + k * 6 + 2)
            miny_v = min(miny_v, y)
            maxy_v = max(maxy_v, y)

    print(f"  classificacao {dict(kinds)}  indices fora da lista {idx_bad}")
    print(f"  y dos vertices {miny_v}..{maxy_v}")

    if args.floor:
        x, z = args.floor
        hits = floor_hits(d, h, x, z, args.y)
        print(f"\nchao em x={x} z={z} abaixo de y={args.y}: {len(hits)} faces")
        for y, i in hits[:8]:
            print(f"  y={y:.3f} poly={i}")

    if 0x06 in cmd and 0x00 in cmd:
        so = off(cmd[0x06][2])
        pe_at = off(cmd[0x00][2])
        nent = cmd[0x00][1]
        for i in range(nent):
            eo = pe_at + i * 0x10
            aid = be16(d, eo)
            x, y, z = s16(d, eo + 2), s16(d, eo + 4), s16(d, eo + 6)
            ry = s16(d, eo + 8)
            hits = floor_hits(d, h, x, z, y + 4)
            chao = " ".join(f"{hy:.1f}" for hy, _ in hits[:3]) or "-"
            print(f"  player entry {i}: id={aid:04X} pos=({x},{y},{z}) "
                  f"rotY={ry} chao={chao}")
        # The spawn list has no terminator entry: it ends where the next
        # structure a scene command points at begins (the exit list, here).
        # Reading on until a 0xFF,0xFF pair found none and invented eleven
        # spawns out of the exit list's bytes.
        end = min((off(c[2]) for c in cmd.values() if off(c[2]) > so),
                  default=len(d))
        for i in range((end - so) // 2):
            idx = d[so + i * 2]
            room = d[so + i * 2 + 1]
            print(f"  spawn {i}: playerEntry={idx} room={room}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
