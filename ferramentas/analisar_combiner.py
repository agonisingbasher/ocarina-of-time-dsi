#!/usr/bin/env python3
"""Enumerates the RDP state real room display lists ask for.

Walks a room's RoomShape entries (opa and xlu), follows G_DL inside the room
segment, and attributes every G_TRI1/G_TRI2 to the G_SETCOMBINE, OtherMode and
geometry mode in force at that point. The point is to say how many triangles
fall into the combiner shapes the DS geometry engine's fixed texel*shade stage
can express, instead of guessing from a room's reputation.

Usage:
  python ferramentas/analisar_combiner.py Bmori1_room_0      # one room, detail
  python ferramentas/analisar_combiner.py --todas            # every room, totals
"""

import argparse
import struct
import sys
from collections import Counter, OrderedDict
from pathlib import Path

OP_ENDDL = 0xDF
OP_DL = 0xDE
OP_TRI1 = 0x05
OP_TRI2 = 0x06
OP_COMBINE = 0xFC
OP_SETENV = 0xFB
OP_SETPRIM = 0xFA
OP_SETFOG = 0xF8
OP_GEOMODE = 0xD9
OP_OTHERMODE_H = 0xE3
OP_OTHERMODE_L = 0xE2
OP_TEXTURE = 0xD7

G_FOG = 0x00010000
G_LIGHTING = 0x00020000

# 5-bit colour muxes (a/c in cycle 0/1).
CC5 = {0: "COMBINED", 1: "TEXEL0", 2: "TEXEL1", 3: "PRIM", 4: "SHADE",
       5: "ENV", 6: "CENTER", 7: "NOISE", 8: "TEXEL0A", 9: "TEXEL1A",
       10: "PRIMA", 11: "SHADEA", 12: "ENVA", 13: "LODFRAC", 14: "PRIMLOD",
       15: "K5", 16: "K4", 17: "K3", 18: "K2", 19: "K1", 20: "K0",
       21: "LOD0", 22: "LOD1", 23: "LOD2", 24: "LOD3", 25: "LOD4",
       26: "LOD5", 27: "LOD6", 28: "LOD7", 29: "LOD8", 30: "LOD9", 31: "0"}
# 4-bit colour muxes (b) reuse 0..15 of the same table.
CC4 = {k: CC5[k] for k in range(16)}
# 3-bit colour muxes (d) are a different, shorter list.
CC3 = {0: "COMBINED", 1: "TEXEL0", 2: "TEXEL1", 3: "PRIM", 4: "SHADE",
       5: "ENV", 6: "1", 7: "0"}
AC3 = {0: "COMBINED", 1: "TEXEL0", 2: "TEXEL1", 3: "PRIM", 4: "SHADE",
       5: "ENV", 6: "1", 7: "0"}


def n(tab, v):
    return tab.get(v, str(v))


def decode_combine(w0, w1):
    return {
        "a0": (w0 >> 20) & 0xF, "c0": (w0 >> 15) & 0x1F,
        "Aa0": (w0 >> 12) & 0x7, "Ac0": (w0 >> 9) & 0x7,
        "b0": (w1 >> 28) & 0xF, "d0": (w1 >> 15) & 0x7,
        "Ab0": (w1 >> 12) & 0x7, "Ad0": (w1 >> 9) & 0x7,
        "a1": (w0 >> 5) & 0xF, "c1": w0 & 0x1F,
        "b1": (w1 >> 24) & 0xF, "d1": (w1 >> 6) & 0x7,
        "Aa1": (w1 >> 21) & 0x7, "Ac1": (w1 >> 18) & 0x7,
        "Ab1": (w1 >> 3) & 0x7, "Ad1": w1 & 0x7,
        "w0": w0, "w1": w1,
    }


def classify(k):
    """Shapes the port's fixed texel*shade stage can express, plus the ones it
    cannot. Mirrors decodeCC() in source/gfx/f3dex2.c, which reads the 4-bit
    b0 as 0xF because gbi's G_CCMUX_0 (31) does not fit the field."""
    a0, b0, c0, d0 = k["a0"], k["b0"], k["c0"], k["d0"]
    if a0 == 1 and b0 == 0xF and c0 == 4 and d0 in (0, 7):
        return "MODULATE"
    if a0 == 0xF and b0 == 0xF and c0 == 0x1F and d0 == 4:
        return "SHADE"
    if a0 == 0xF and b0 == 0xF and c0 == 0x1F and d0 == 1:
        return "DECAL"
    return "OTHER"


def texel_pair(k):
    """Classifies where the colour output's texture comes from on two tiles."""
    used = {k["a0"], k["b0"], k["c0"], k["d0"]}
    t0 = 1 in used or 8 in used
    t1 = 2 in used or 9 in used
    return "T0+T1" if (t0 and t1) else ("T1" if t1 else ("T0" if t0 else "-"))


def fmt_cc(k):
    return ("(%s-%s)*%s+%s  alpha (%s-%s)*%s+%s  |  "
            "(%s-%s)*%s+%s  alpha (%s-%s)*%s+%s"
            % (n(CC4, k["a0"]), n(CC4, k["b0"]), n(CC5, k["c0"]), n(CC3, k["d0"]),
               n(AC3, k["Aa0"]), n(AC3, k["Ab0"]), n(AC3, k["Ac0"]), n(AC3, k["Ad0"]),
               n(CC4, k["a1"]), n(CC4, k["b1"]), n(CC5, k["c1"]), n(CC3, k["d1"]),
               n(AC3, k["Aa1"]), n(AC3, k["Ab1"]), n(AC3, k["Ac1"]), n(AC3, k["Ad1"])))


def fmt_cc0(k):
    """Cycle 0 alone. Two display lists that share cycle 0 but differ in cycle 1
    are the same arithmetic on screen when OtherMode_H selects 1-cycle mode, so
    grouping by cycle 0 keeps the lerp family together instead of shattering it
    into per-alpha variants."""
    return ("(%s-%s)*%s+%s  alpha (%s-%s)*%s+%s"
            % (n(CC4, k["a0"]), n(CC4, k["b0"]), n(CC5, k["c0"]), n(CC3, k["d0"]),
               n(AC3, k["Aa0"]), n(AC3, k["Ab0"]), n(AC3, k["Ac0"]), n(AC3, k["Ad0"])))


class Walk:
    def __init__(self, data):
        self.data = data
        self.state = {
            "cc": None, "om_h": 0, "om_l": 0, "geomode": 0,
            "env": None, "prim": None, "fog": None, "texon": 0,
        }
        self.per_cc = OrderedDict()
        self.tri_total = 0
        self.external_refs = Counter()
        self.truncated = 0

    def bump(self, label, tris):
        cc = self.state["cc"]
        key = (label, tuple(sorted(cc.items())) if cc else ())
        ent = self.per_cc.get(key)
        if ent is None:
            ent = {"label": label, "cc": cc, "tris": 0,
                   "om_h": Counter(), "om_l": Counter(),
                   "geomode": Counter(), "texon": 0,
                   "prim": Counter(), "env": Counter()}
            self.per_cc[key] = ent
        ent["tris"] += tris
        ent["om_h"][self.state["om_h"]] += tris
        ent["om_l"][self.state["om_l"]] += tris
        ent["geomode"][self.state["geomode"]] += tris
        ent["prim"][self.state["prim"]] += tris
        ent["env"][self.state["env"]] += tris
        ent["texon"] += self.state["texon"] * tris
        self.tri_total += tris

    def run(self, label, off, depth=0):
        pos = off
        seen = 0
        while pos + 8 <= len(self.data):
            w0, w1 = struct.unpack_from(">II", self.data, pos)
            op = w0 >> 24
            seen += 1
            if seen > 60000:
                self.truncated += 1
                return
            if op == OP_ENDDL:
                return
            if op == OP_COMBINE:
                self.state["cc"] = decode_combine(w0, w1)
            elif op == OP_SETENV:
                self.state["env"] = w1
            elif op == OP_SETPRIM:
                self.state["prim"] = w1
            elif op == OP_SETFOG:
                self.state["fog"] = w1
            elif op == OP_GEOMODE:
                # F3DEX_GBI_2 packs a set and a clear into one command: the low
                # 24 bits of w0 are ~clear and w1 is what to set.
                clear = (~w0) & 0xFFFFFF
                self.state["geomode"] = (self.state["geomode"] & ~clear) | w1
            elif op == OP_OTHERMODE_H:
                self.state["om_h"] = w1
            elif op == OP_OTHERMODE_L:
                self.state["om_l"] = w1
            elif op == OP_TEXTURE:
                self.state["texon"] = w0 & 0xFF
            elif op == OP_TRI1:
                self.bump(label, 1)
            elif op == OP_TRI2:
                self.bump(label, 2)
            elif op == OP_DL:
                nseg = (w1 >> 24) & 0xFF
                if nseg == 3 and depth < 8:
                    self.run(label + ">", w1 & 0x00FFFFFF, depth + 1)
                else:
                    self.external_refs[nseg] += 1
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


def analyse(data, detail=False):
    e = entries(data)
    if e is None:
        return None
    stype, ent, count, stride = e
    if stype not in (0, 2):
        return {"stage": stype}
    opa_at, xlu_at = (8, 12) if stype == 2 else (0, 4)
    out = {"stage": stype, "tris": 0, "classes": Counter(), "pairs": Counter(),
           "alpha": Counter(), "fog": Counter(), "two_cycle": Counter(),
           "lit": Counter(), "lit_tri": Counter(), "sig": Counter(),
           "sig0": Counter(), "sig0_lit": Counter(), "groups": []}
    for i in range(count):
        base = ent + i * stride
        for label, at in (("opa", opa_at), ("xlu", xlu_at)):
            ptr = struct.unpack_from(">I", data, base + at)[0]
            if ptr == 0:
                continue
            w = Walk(data)
            w.run(label, ptr & 0xFFFFFF)
            for ent2 in w.per_cc.values():
                cc = ent2["cc"]
                cls = classify(cc) if cc else "none"
                out["classes"][cls] += ent2["tris"]
                if cc:
                    out["pairs"][texel_pair(cc)] += ent2["tris"]
                    out["alpha"][alpha_class(cc)] += ent2["tris"]
                    if cls == "OTHER":
                        out["sig"][fmt_cc(cc)] += ent2["tris"]
                gm = ent2["geomode"].most_common(1)[0][0]
                out["fog"]["on" if gm & G_FOG else "off"] += ent2["tris"]
                lit = any(g & G_LIGHTING for g in ent2["geomode"])
                out["lit"][(cls, lit)] += ent2["tris"]
                # The group key is the combiner, not the geometry mode, so a
                # single group can hold triangles both lit and unlit. Split by
                # triangle instead of by group for anything that decides code.
                lit_tri = sum(v for g, v in ent2["geomode"].items()
                              if g & G_LIGHTING)
                out["lit_tri"][(cls, True)] += lit_tri
                out["lit_tri"][(cls, False)] += ent2["tris"] - lit_tri
                if cc:
                    s0 = fmt_cc0(cc)
                    out["sig0"][s0] += ent2["tris"]
                    out["sig0_lit"][(s0, True)] += lit_tri
                    out["sig0_lit"][(s0, False)] += ent2["tris"] - lit_tri
                omh = ent2["om_h"].most_common(1)[0][0]
                out["two_cycle"][(omh >> 20) & 3] += ent2["tris"]
                out["tris"] += ent2["tris"]
                if detail:
                    out["groups"].append((i, label, ent2, cls))
    return out


def alpha_class(k):
    Ad = k["Ad0"]
    if Ad == 4:
        return "SHADE"
    if Ad == 1:
        return "TEXEL0"
    if Ad == 3:
        return "PRIM"
    if Ad == 0:
        return "COMBINED(cycle0)"
    return n(AC3, Ad)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("room", nargs="?")
    ap.add_argument("--root", default="game/oot/extracted/ntsc-1.2/baserom")
    ap.add_argument("--todas", action="store_true")
    ap.add_argument("--luz", action="store_true",
                    help="separa cada classe por G_LIGHTING ligado/desligado")
    ap.add_argument("--prim", action="store_true",
                    help="mostra PRIM/ENV/texOn/geomode por grupo de combiner")
    ap.add_argument("--lerp", action="store_true",
                    help="famílias de ciclo 0 que misturam TEXEL0 e TEXEL1, "
                         "cruzadas com G_LIGHTING do grupo e da sala")
    args = ap.parse_args()
    root = Path(args.root)

    if args.todas:
        files = sorted(p for p in root.iterdir() if p.name.endswith("_room_0")
                       or "_room_" in p.name)
        totals = Counter()
        pairs = Counter()
        alphas = Counter()
        fogs = Counter()
        cycles = Counter()
        lits = Counter()
        lits_tri = Counter()
        sigs = Counter()
        sig_rooms = {}
        sig0s = Counter()
        sig0_rooms = {}
        sig0_lit = Counter()
        sig0_roomlit = Counter()
        lerp_rooms_total = 0
        per_room = []
        skipped = Counter()
        for p in files:
            r = analyse(p.read_bytes())
            if r is None:
                skipped["sem SCENE_CMD_MESH"] += 1
                continue
            if r.get("stage") not in (0, 2):
                skipped["shape imagem (tipo %d)" % r["stage"]] += 1
                continue
            if r["tris"] == 0:
                skipped["sem triângulos"] += 1
                continue
            totals.update(r["classes"])
            pairs.update(r["pairs"])
            alphas.update(r["alpha"])
            fogs.update(r["fog"])
            cycles.update(r["two_cycle"])
            lits.update(r["lit"])
            lits_tri.update(r["lit_tri"])
            sigs.update(r["sig"])
            for s in r["sig"]:
                sig_rooms.setdefault(s, set()).add(p.name)
            sig0s.update(r["sig0"])
            for s in r["sig0"]:
                sig0_rooms.setdefault(s, set()).add(p.name)
            sig0_lit.update(r["sig0_lit"])
            room_lit = any(lit and v for (cls, lit), v in r["lit_tri"].items())
            if room_lit:
                lerp_rooms_total += 1
                sig0_roomlit.update(r["sig0"])
            other = r["classes"]["OTHER"]
            per_room.append((other / r["tris"], p.name, r["tris"], other))
        total = sum(totals.values())
        print("salas analisadas: %d | puladas: %s"
              % (len(per_room), dict(skipped) or "nenhuma"))
        print("triângulos: %d" % total)
        for k, v in totals.most_common():
            print("  %-9s %8d  %5.1f %%" % (k, v, 100.0 * v / total))
        print("texturas por triângulo:", {k: "%d (%.1f %%)" % (v, 100.0 * v / total)
                                          for k, v in pairs.most_common()})
        print("alpha (Ad0):", {k: "%d (%.1f %%)" % (v, 100.0 * v / total)
                               for k, v in alphas.most_common()})
        print("fog:", dict(fogs))
        print("cycle type (0=1CYC,1=2CYC):", dict(cycles))
        print("assinaturas OTHER:")
        for s, v in sigs.most_common(8):
            print("  %8d  %5.1f %%  %3d salas  %s"
                  % (v, 100.0 * v / total, len(sig_rooms.get(s, ())), s))
        if args.luz:
            print("classe × G_LIGHTING — por grupo de combiner (leitura da "
                  "OTD-028, mantida para comparar):")
            for (cls, lit), v in sorted(lits.items(), key=lambda kv: -kv[1]):
                print("  %-9s lighting=%-5s %8d  %5.1f %%"
                      % (cls, lit, v, 100.0 * v / total))
            print("classe × G_LIGHTING — por triângulo:")
            for (cls, lit), v in sorted(lits_tri.items(), key=lambda kv: -kv[1]):
                print("  %-9s lighting=%-5s %8d  %5.1f %%"
                      % (cls, lit, v, 100.0 * v / total))
        if args.lerp:
            print("ciclo 0 que mistura TEXEL0 e TEXEL1 (%d salas pedem luz em "
                  "algum ponto, de %d):" % (lerp_rooms_total, len(per_room)))
            fams = [(s, v) for s, v in sig0s.items()
                    if "TEXEL0" in s and "TEXEL1" in s]
            fams.sort(key=lambda kv: -kv[1])
            fam_total = 0
            fam_lit = 0
            fam_roomlit = 0
            for s, v in fams:
                on = sig0_lit.get((s, True), 0)
                off = sig0_lit.get((s, False), 0)
                in_lit_room = sig0_roomlit.get(s, 0)
                fam_total += v
                fam_lit += off
                fam_roomlit += in_lit_room
                print("  %8d  %5.1f %%  %3d salas  triângulo lighting: off=%d "
                      "on=%d  em sala com luz=%d"
                      % (v, 100.0 * v / total, len(sig0_rooms.get(s, ())),
                         off, on, in_lit_room))
                print("           %s" % s)
            print("  total T0+T1 no ciclo 0: %d (%.1f %%) | triângulos sem luz: "
                  "%d (%.1f %%) | em sala que pede luz: %d (%.1f %%)"
                  % (fam_total, 100.0 * fam_total / total,
                     fam_lit, 100.0 * fam_lit / total,
                     fam_roomlit, 100.0 * fam_roomlit / total))
        per_room.sort(reverse=True)
        print("piores salas por fração OTHER:")
        for frac, name, tris, other in per_room[:12]:
            print("  %-28s %4d/%4d  %5.1f %%" % (name, other, tris, 100 * frac))
        return 0

    data = (root / args.room).read_bytes()
    r = analyse(data, detail=True)
    if r is None:
        print("no SCENE_CMD_MESH")
        return 1
    print("%s: %d bytes, RoomShape type=%s, %d triangles"
          % (args.room, len(data), r["stage"], r["tris"]))
    for i, label, e, cls in r["groups"]:
        cc = e["cc"]
        omh = e["om_h"].most_common(1)[0][0]
        oml = e["om_l"].most_common(1)[0][0]
        gm = e["geomode"].most_common(1)[0][0]
        print(" entry %d %s: %-8s tris=%-4d texOn=%lu/%lu"
              % (i, label, cls, e["tris"], e["texon"], e["tris"]))
        print("   cc   %s" % (fmt_cc(cc) if cc else "unset"))
        print("   cycle=%s textlut=%s omH=%08X omL=%08X geomode=%08X fog=%s"
              % ((omh >> 20) & 3, (omh >> 14) & 3, omh, oml, gm,
                 "on" if gm & G_FOG else "off"))
        if args.prim:
            def dist(c):
                return [(("0x%08X" % k) if k is not None else "-", v)
                        for k, v in c.most_common(4)]
            print("   prim=%s env=%s texOn=%d"
                  % (dist(e["prim"]), dist(e["env"]), e["texon"]))
    print("classes:", dict(r["classes"]))
    print("pares de textura:", dict(r["pairs"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
