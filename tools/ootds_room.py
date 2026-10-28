#!/usr/bin/env python3
"""Room packer for the Ocarina Dsi port.

Reads a room segment straight out of the decompressed baserom, walks the game's
own scene-command / RoomShape / Gfx chain and reports which segments a real
display list touches, so the DS-side loader can be built against measured
addresses instead of guesses.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

SEGMENT_NUMBER = 0x0F
SEGMENT_OFFSET_MASK = 0x00FFFFFF


def seg(addr: int) -> int:
    return (addr >> 24) & SEGMENT_NUMBER


def off(addr: int) -> int:
    return addr & SEGMENT_OFFSET_MASK


CMD_SIZE = 8

OPC_NAMES = {
    0x01: "G_VTX",
    0x02: "G_MODIFYVTX",
    0x03: "G_CULLDL",
    0x04: "G_BRANCH_Z",
    0x05: "G_TRI1",
    0x06: "G_TRI2",
    0x07: "G_QUAD",
    0x08: "G_LINE3D",
    0xD7: "G_TEXTURE",
    0xD8: "G_POPMTX",
    0xD9: "G_GEOMETRYMODE",
    0xDA: "G_MTX",
    0xDB: "G_MOVEWORD",
    0xDC: "G_MOVEMEM",
    0xDD: "G_LOADUCODE",
    0xDE: "G_DL",
    0xDF: "G_ENDDL",
    0xE0: "G_SPNOOP",
    0xE1: "G_RDPHALF_1",
    0xE2: "G_SETOTHERMODE_L",
    0xE3: "G_SETOTHERMODE_H",
    0xE4: "G_SETTIMG",
    0xE5: "G_SETTILE",
    0xE6: "G_FILLRECT",
    0xE7: "G_SETFILLCOLOR",
    0xE8: "G_SETSCISSOR",
    0xE9: "G_SETPRIMDEPTH",
    0xEA: "G_RDPSETOTHERMODE",
    0xEB: "G_LOADTLUT",
    0xEC: "G_SETRESAMPLE",
    0xED: "G_GEMOVID",
    0xF0: "G_LDRL",
    0xF1: "G_RDPHALF_2",
    0xF2: "G_SETTILESIZE",
    0xF3: "G_LOADBLOCK",
    0xF4: "G_LOADTILE",
    0xF5: "G_SETMASK",
    0xF6: "G_SETBLMOD",
    0xF7: "G_SETANCILLARY",
    0xF8: "G_SETDITHER",
    0xF9: "G_SETPRIMCOLOR",
    0xFA: "G_SETBLENDCOLOR",
    0xFB: "G_SETENVCOLOR",
    0xFC: "G_SETOTHERMODE_H2",
    0xFD: "G_TEXRECT",
    0xFE: "G_TEXRECTFLIP",
}

# Commands whose second word holds a pointer that the RSP follows.
POINTER_OPS = {
    0x01: (0, "G_VTX vertex"),
    0xDA: (0, "G_MTX matrix"),
    0xDE: (0, "G_DL sublist"),
    0xDC: (0, "G_MOVEMEM"),
    0xE4: (0, "G_SETTIMG image"),
    0xE5: (2, "G_SETTILE tlut"),
    0xEB: (0, "G_LOADTLUT tlut"),
    0xF3: (0, "G_LOADBLOCK image"),
    0xF4: (0, "G_LOADTILE image"),
    0xF2: (0, "G_SETTILESIZE image"),
    0x04: (0, "G_BRANCH_Z dlist"),
    0x03: (0, "G_CULLDL dlist"),
}


class Room:
    def __init__(self, name: str, data: bytes):
        self.name = name
        self.data = data
        self.cmds: list[tuple[int, int, int]] = []
        self.shape_addr = 0
        self.segment_refs: dict[int, int] = {}
        self.tri1 = 0
        self.tri2 = 0
        self.gfx_count = 0
        self.dlists: list[tuple[str, int, int]] = []

    def note(self, addr: int):
        self.segment_refs[seg(addr)] = self.segment_refs.get(seg(addr), 0) + 1

    def walk(self, label: str, addr: int, depth: int, limit: int = 40000):
        pos = off(addr)
        start = pos
        seen = 0
        while pos + 8 <= len(self.data) and seen < limit:
            hi = struct.unpack_from(">I", self.data, pos)[0]
            lo = struct.unpack_from(">I", self.data, pos + 4)[0]
            opc = hi >> 24
            name = OPC_NAMES.get(opc, f"0x{opc:02X}")
            seen += 1
            if opc == 0xDF:
                self.dlists.append((label, start, pos - start + 8))
                return
            if opc in POINTER_OPS:
                which, _ = POINTER_OPS[opc]
                cand = lo if which == 0 else struct.unpack_from(">I", self.data, pos + 4 * which)[0]
                if seg(cand) != 0:
                    self.note(cand)
                    self.refs.append((f"{label}@0x{pos:06X}", OPC_NAMES.get(opc, "?"), cand))
                    if opc == 0xDE and depth < 8 and seg(cand) == 3 and off(cand) < len(self.data):
                        self.walk(label + "/dl", cand, depth + 1)
            if opc == 0x05:
                self.tri1 += 1
            elif opc == 0x06:
                self.tri2 += 1
            self.gfx_count += 1
            pos += 8
        self.dlists.append((label, start, pos - start))

    def parse(self):
        pos = 0
        while pos + CMD_SIZE <= len(self.data):
            ctype = self.data[pos]
            param = self.data[pos + 1]
            cdata = struct.unpack_from(">I", self.data, pos + 4)[0]
            self.cmds.append((ctype, param, cdata))
            if ctype == 0x14:  # SCENE_CMD_ID_END
                break
            if ctype == 0x0A:  # SCENE_CMD_ID_ROOM_SHAPE
                self.shape_addr = cdata
            pos += CMD_SIZE
        if self.shape_addr:
            sp = off(self.shape_addr)
            stype, numentries = self.data[sp], self.data[sp + 1]
            entries = struct.unpack_from(">I", self.data, sp + 4)[0]
            entries_end = struct.unpack_from(">I", self.data, sp + 8)[0]
            self.note(entries)
            count = (entries_end - entries) // 8 if entries_end > entries else numentries
            for i in range(count):
                base = off(entries) + i * 8
                opa = struct.unpack_from(">I", self.data, base)[0]
                xlu = struct.unpack_from(">I", self.data, base + 4)[0]
                if opa and off(opa) < len(self.data):
                    self.note(opa)
                    self.walk(f"entry{i}.opa", opa, 0)
                if xlu and off(xlu) < len(self.data):
                    self.note(xlu)
                    self.walk(f"entry{i}.xlu", xlu, 0)
            return stype, numentries, count
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("room", help="room segment file, e.g. market_day_room_0")
    ap.add_argument("--root", default="game/oot/extracted/ntsc-1.2/baserom")
    ap.add_argument("--hex", nargs="*", default=[])
    args = ap.parse_args()

    path = Path(args.root) / args.room
    data = path.read_bytes()
    room = Room(args.room, data)
    print(f"{args.room}: {len(data)} bytes")
    shape = room.parse()
    print("scene commands:", ", ".join(f"0x{c:02X}/{p}" for c, p, _ in room.cmds))
    if shape is not None:
        stype, numentries, count = shape
        print(f"RoomShape type={stype} numEntries={numentries} entries={count}")
    room.walk("root", room.shape_addr or 0, 99) if False else None
    print(f"display lists walked: {len(room.dlists)}  gfx={room.gfx_count}"
          f"  G_TRI1={room.tri1} G_TRI2={room.tri2}")
    print("segment reference counts:", dict(sorted(room.segment_refs.items())))
    for label, start, length in room.dlists[:12]:
        print(f"  {label:16s} file_off=0x{start:06X} len=0x{length:X}")


if __name__ == "__main__":
    main()
