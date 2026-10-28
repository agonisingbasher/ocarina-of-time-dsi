#!/usr/bin/env python3
"""Regera evidencias/MANIFESTO.csv a partir dos arquivos em disco.

Uma linha por arquivo (id, tag, arquivo, bytes, sha256, proveniencia), ordenada
por id e nome. Preserva a proveniência já registrada; arquivos novos recebem
proveniência desconhecida em vez de inventada. Linhas duplicadas ou órfãs são
descartadas e contadas.

Uso: python ferramentas/regerar_manifesto.py [--raiz DIR]
"""

import argparse
import csv
import hashlib
import os
import sys


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--raiz", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    args = parser.parse_args()
    root = os.path.abspath(args.raiz)
    evid = os.path.join(root, "evidencias")
    manifest = os.path.join(evid, "MANIFESTO.csv")

    old = {}
    raw_rows = 0
    if os.path.isfile(manifest):
        with open(manifest, "r", encoding="utf-8", newline="") as fh:
            for row in csv.DictReader(fh):
                raw_rows += 1
                key = (row["id"].strip(), row["arquivo"].strip())
                old.setdefault(key, row)

    rows = []
    duplicates = 0
    orphan_rows = 0
    seen = set()
    for name in sorted(os.listdir(evid)):
        folder = os.path.join(evid, name)
        if not os.path.isdir(folder) or not name.startswith("OTD-"):
            continue
        for arquivo in sorted(os.listdir(folder)):
            path = os.path.join(folder, arquivo)
            if not os.path.isfile(path):
                continue
            key = (name, arquivo)
            if key in seen:
                duplicates += 1
                continue
            seen.add(key)
            prior = old.get(key)
            rows.append({
                "id": name,
                "tag": prior["tag"] if prior else os.path.splitext(arquivo)[0],
                "arquivo": arquivo,
                "bytes": str(os.path.getsize(path)),
                "sha256": sha256(path),
                "proveniencia": prior["proveniencia"] if prior else "arquivo presente sem proveniência registrada",
            })

    orphan_rows = len(set(old) - seen)
    rows.sort(key=lambda r: (r["id"], r["arquivo"]))
    with open(manifest, "w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=["id", "tag", "arquivo", "bytes", "sha256", "proveniencia"], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    print("arquivos: %d | linhas antigas: %d (%d únicas) | duplicatas descartadas: %d"
          % (len(rows), raw_rows, len(old), raw_rows - len(old)))
    if duplicates or orphan_rows:
        print("OK: manifesto reconciliado com o disco")
    else:
        print("OK: manifesto já estava consistente")
    return 0


if __name__ == "__main__":
    sys.exit(main())
