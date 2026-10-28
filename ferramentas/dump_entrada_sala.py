#!/usr/bin/env python3
"""Prints the commands of one RoomShape entry, one per line, in file order.

Used to see the order a real room writes its tile descriptors in: whether the
G_SETTILESIZE of a tile arrives after the G_LOADBLOCK that filled it, which is
what the port's texture bake has to assume.

Usage:
  python ferramentas/dump_entrada_sala.py Bmori1_room_0 2 opa
"""

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analisar_combiner as A

NAMES = {0xDF: "ENDDL", 0xDE: "DL", 0x05: "TRI1", 0x06: "TRI2",
         0xFC: "SETCOMBINE", 0xFB: "SETENV", 0xFA: "SETPRIM",
         0xF8: "SETFOG", 0xD9: "GEOMODE", 0xE3: "OM_H", 0xE2: "OM_L",
         0xD7: "TEXTURE", 0xF5: "SETTILE", 0xF2: "SETTILESIZE",
         0xF3: "LOADBLOCK", 0xF4: "LOADTILE", 0xF0: "LOADTLUT",
         0xFD: "SETTIMG", 0xDB: "MOVEWORD", 0xE6: "RDPLOADSYNC",
         0xE7: "RDPPIPESYNC", 0xE8: "RDPTILESYNC", 0xE9: "RDPFULLSYNC",
         0xBC: "MOVEMEM", 0x01: "VTX", 0x03: "CULLDL", 0x04: "BRANCH"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("room")
    ap.add_argument("entry", type=int)
    ap.add_argument("which", choices=("opa", "xlu"), default="opa", nargs="?")
    ap.add_argument("--root", default="game/oot/extracted/ntsc-1.2/baserom")
    args = ap.parse_args()
    data = (Path(args.root) / args.room).read_bytes()
    e = A.entries(data)
    if e is None:
        print("no SCENE_CMD_MESH")
        return 1
    stype, ent, count, stride = e
    if args.entry >= count:
        print("entry %d of %d" % (args.entry, count))
        return 1
    opa_at, xlu_at = (8, 12) if stype == 2 else (0, 4)
    at = opa_at if args.which == "opa" else xlu_at
    ptr = struct.unpack_from(">I", data, ent + args.entry * stride + at)[0] & 0xFFFFFF
    print("# %s entry %d %s: RoomShape type=%s, %d entries, list at %06X"
          % (args.room, args.entry, args.which, stype, count, ptr))
    pos = ptr
    while pos + 8 <= len(data):
        w0, w1 = struct.unpack_from(">II", data, pos)
        op = w0 >> 24
        name = NAMES.get(op, "0x%02X" % op)
        extra = ""
        if op == 0xF5:
            extra = " fmt=%d siz=%d line=%d tmem=%d tile=%d pal=%d" % (
                (w0 >> 21) & 7, (w0 >> 19) & 3, (w0 >> 9) & 0x1FF, w0 & 0x1FF,
                (w1 >> 24) & 7, (w1 >> 20) & 0xF)
        elif op == 0xF2:
            extra = " tile=%d uls=%d ult=%d lrs=%d lrt=%d" % (
                (w1 >> 24) & 7, (w0 >> 12) & 0xFFF, w0 & 0xFFF,
                (w1 >> 12) & 0xFFF, w1 & 0xFFF)
        elif op == 0xD7:
            extra = " on=%d tile=%d level=%d" % ((w0 >> 1) & 0x7F, (w0 >> 8) & 7,
                                                (w0 >> 11) & 7)
        elif op == 0xF3:
            extra = " tile=%d words=%d" % ((w1 >> 24) & 7, ((w1 >> 12) & 0xFFF) + 1)
        print("%04X %-11s %08X %08X%s" % (pos, name, w0, w1, extra))
        if op == 0xDF:
            break
        pos += 8
    return 0


if __name__ == "__main__":
    sys.exit(main())
