#!/usr/bin/env python3
"""Gera o recorte público `docs/data/oot-dsi-progress.json` a partir de `dados/progresso.json`.

O recorte não é o documento interno copiado: cada entrega troca `resumo_publico` por
`resumo`, ganha `validacao` derivada do próprio estado, `limitacoes` vinda de
`estudo_engenharia.limitacao` e `evidencias` descritas uma a uma — o site publica só as
capturas listadas na raiz `evidencias_publicadas`, que é o único campo do recorte escrito
à mão. Nenhum texto é reescrito: o que muda é a estrutura, mais o vocabulário que o
projeto não publica.

A saída é determinística — a mesma árvore gera o mesmo arquivo — e é conferida contra
`docs/data/oot-dsi-progress.schema.json` antes de escrever.

Uso: python ferramentas/gerar_recorte_publico.py [--raiz DIR] [--dry-run]
Sai com código 1 se houver conteúdo não publicável ou violação de esquema.
"""

import argparse
import json
import os
import re
import sys

# O projeto não publica o nome do arquivo de referência nem extensões de ROM.
FORBIDDEN = [
    (re.compile(r"\bbaseroms?\b", re.I), "baserom"),
    (re.compile(r"\.(z64|n64|v64)\b", re.I), "extensão de ROM"),
    (re.compile(r"[A-Za-z]:[\\/]{1,2}(?:Users|users)"), "caminho de usuário"),
    (re.compile(r"%TEMP%", re.I), "%TEMP%"),
    (re.compile(r"[\w.+-]+@[\w-]+\.[A-Za-z]{2,}"), "e-mail"),
]

SUBSTITUICOES = [
    # dentro de um caminho basta não nomear o arquivo de referência
    (re.compile(r"game/oot/extracted/ntsc-1\.2/baserom"), "game/oot/extracted/ntsc-1.2/<referência>"),
    (re.compile(r"\bno baserom\b", re.I), "na cópia de referência"),
    (re.compile(r"\bdos baserom\b", re.I), "da cópia de referência"),
    (re.compile(r"\bdo baserom\b", re.I), "da cópia de referência"),
    (re.compile(r"\bbaserom\b", re.I), "cópia de referência"),
    # "ROM" nas entregas antigas é a imagem de N64; o texto aprovado do site é literal
    (re.compile(r"O ROM é OcarinaDsi\.nds"), "O build é OcarinaDsi.nds"),
    (re.compile(r"dentro e fora da ROM"), "dentro e fora do build"),
    (re.compile(r"o ROM de referência do usuário"), "a cópia de referência do usuário"),
    (re.compile(r"depende de ROM nem"), "depende de cópia de referência nem"),
    (re.compile(r"sem ter o ROM em mãos"), "sem ter a cópia de referência em mãos"),
    (re.compile(r"instruções do jogo no ROM"), "instruções do jogo no binário"),
    (re.compile(r"de versão do ROM era"), "de versão da cópia de referência era"),
    (re.compile(r"específica do ROM"), "específica da cópia de referência"),
    # concordância depois da troca de palavra
    (re.compile(r"\bdo cópia de referência\b"), "da cópia de referência"),
    (re.compile(r"\bo cópia de referência\b"), "a cópia de referência"),
    (re.compile(r"cópia de referência NTSC-U 1\.2 extraído"), "cópia de referência NTSC-U 1.2 extraída"),
]

ORDEM_ENTREGA = [
    "id", "tipo", "titulo", "data", "frente", "fase", "estado", "build", "resumo",
    "estudo_engenharia", "validacao", "limitacoes", "evidencias", "medicoes", "testes",
    "problemas", "retratacoes", "proximos_passos", "referencias_historicos", "subestado",
    "entrega_pai", "dependencias",
]

AMBIENTE_EMULADOR = {
    "ambiente": "melonDS 1.1, modo DS, 67 MHz",
    "modo": "DS mode",
    "emulador": "melonDS 1.1",
    "clock_mhz": 67,
    "hardware_fisico": False,
    "observacao": "Resultado de emulador; não é hardware físico.",
}
AMBIENTE_COMPUTADOR = {
    "ambiente": "computador (host), sem emulador",
    "modo": None,
    "emulador": None,
    "clock_mhz": None,
    "hardware_fisico": False,
    "observacao": "Validado por autoteste ou verificação fora do DS.",
}
PASTA_PUBLICADA = ("Pasta de evidência do projeto; apenas as capturas listadas em "
                   "evidencias_publicadas foram copiadas para o site.")
PASTA_INEDITA = ("Pasta de evidência mantida no projeto; nenhuma captura desta entrega "
                 "foi publicada nesta página.")


def sanitize(value):
    if isinstance(value, str):
        for pattern, replacement in SUBSTITUICOES:
            value = pattern.sub(replacement, value)
        return value
    if isinstance(value, list):
        return [sanitize(item) for item in value]
    if isinstance(value, dict):
        return {key: sanitize(item) for key, item in value.items()}
    return value


def published_by_folder(published):
    """`evidencias_publicadas` declara a entrega; a entrega aponta para pastas."""
    by_folder = {}
    for item in published:
        by_folder.setdefault("evidencias/" + item["entrega"], []).append(item["arquivo_site"])
    return by_folder


def as_list(value):
    """Uma evidência escrita em prosa vira item único: o recorte mantém array de caminhos."""
    if value is None:
        return []
    return value if isinstance(value, list) else [value]


def cut(source, by_folder):
    entry = dict(source)
    estudo = entry.get("estudo_engenharia") or {}
    entry["resumo"] = entry.pop("resumo_publico")
    entry["validacao"] = dict(
        AMBIENTE_COMPUTADOR if source.get("estado") == "testado_computador" else AMBIENTE_EMULADOR
    )
    limitacao = estudo.get("limitacao")
    entry["limitacoes"] = [limitacao] if limitacao else ["Sem limitação registrada nesta entrega."]
    entry["retratacoes"] = entry.get("retratacoes") or []
    entry["medicoes"] = entry.get("medicoes") or []
    for group in ("medicoes", "problemas", "testes"):
        for item in entry.get(group, []):
            if "evidencia" in item:
                item["evidencia"] = as_list(item["evidencia"])
    folders = entry.get("evidencias", [])
    entry["evidencias"] = []
    for folder in folders:
        files = by_folder.get(folder, [])
        entry["evidencias"].append({
            "referencia": folder,
            "publicada_no_site": bool(files),
            "arquivos_publicados": list(files),
            "observacao": PASTA_PUBLICADA if files else PASTA_INEDITA,
        })
    ordered = {key: entry[key] for key in ORDEM_ENTREGA if key in entry}
    for key, value in entry.items():
        ordered.setdefault(key, value)
    return sanitize(ordered)


def find_forbidden(text):
    hits = []
    for pattern, label in FORBIDDEN:
        for match in pattern.finditer(text):
            start = max(0, match.start() - 40)
            hits.append("%s: …%s…" % (label, text[start:match.end() + 40].replace("\n", " ")))
    return hits


def check_schema(node, schema, path, errors):
    if not schema:
        return
    if "$ref" in schema:
        check_schema(node, DEFS.get(schema["$ref"].rsplit("/", 1)[-1]), path, errors)
        return
    kind = schema.get("type")
    if kind:
        kinds = kind if isinstance(kind, list) else [kind]
        ok = any(
            (k == "object" and isinstance(node, dict)) or (k == "array" and isinstance(node, list))
            or (k == "string" and isinstance(node, str)) or (k == "boolean" and isinstance(node, bool))
            or (k == "number" and isinstance(node, (int, float)) and not isinstance(node, bool))
            or (k == "null" and node is None)
            for k in kinds
        )
        if not ok:
            errors.append("%s: é %s, o esquema pede %s" % (path, type(node).__name__, "/".join(kinds)))
            return
    if isinstance(node, dict):
        for key in schema.get("required", []):
            if key not in node:
                errors.append("%s: campo obrigatório ausente: %s" % (path, key))
        props = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            for key in node:
                if key not in props:
                    errors.append("%s: propriedade fora do esquema: %s" % (path, key))
        for key, value in node.items():
            if key in props:
                check_schema(value, props[key], "%s.%s" % (path, key), errors)
    elif isinstance(node, list):
        if len(node) < schema.get("minItems", 0):
            errors.append("%s: menos de %d itens" % (path, schema["minItems"]))
        if "items" in schema:
            for index, item in enumerate(node):
                check_schema(item, schema["items"], "%s[%d]" % (path, index), errors)
    elif isinstance(node, str):
        if len(node) < schema.get("minLength", 0):
            errors.append("%s: mais curto que minLength %d" % (path, schema["minLength"]))
        if "pattern" in schema and not re.search(schema["pattern"], node):
            errors.append("%s: não bate com o padrão %s" % (path, schema["pattern"]))
    if "enum" in schema and node not in schema["enum"]:
        errors.append("%s: %r fora do enum" % (path, node))


DEFS = {}


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    parser = argparse.ArgumentParser()
    parser.add_argument("--raiz", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    root = os.path.abspath(args.raiz)

    def load(*parts):
        with open(os.path.join(root, *parts), "r", encoding="utf-8") as fh:
            return json.load(fh)

    source = load("dados", "progresso.json")
    current = load("docs", "data", "oot-dsi-progress.json")
    schema = load("docs", "data", "oot-dsi-progress.schema.json")
    global DEFS
    DEFS = schema.get("$defs", {})

    by_folder = published_by_folder(current["evidencias_publicadas"])
    entregas = [cut(entry, by_folder) for entry in source["entregas"]]

    data = {
        "schema_version": source["schema_version"],
        "projeto": current["projeto"],
        "estados": source["estados"],
        "ambiente_padrao": current["ambiente_padrao"],
        "medicoes_definicoes": source["medicoes_definicoes"],
        "entregas": entregas,
        "evidencias_publicadas": current["evidencias_publicadas"],
        "nota_publicacao": (
            "Recorte público derivado de dados/progresso.json do projeto. As %d entregas OTD-001..OTD-%03d "
            "estão incluídas, na mesma ordem de registro; apenas %d capturas foram copiadas para o site, "
            "listadas em evidencias_publicadas. Nenhuma cópia de referência ou asset extraído acompanha este arquivo."
            % (len(entregas), len(entregas), len(current["evidencias_publicadas"]))
        ),
    }

    errors = []
    check_schema(data, schema, "raiz", errors)
    hits = find_forbidden(json.dumps(data, ensure_ascii=False))
    for hit in hits:
        print("ERRO:  conteúdo não publicável no recorte — " + hit)
    for message in errors[:40]:
        print("ERRO:  " + message)
    if len(errors) > 40:
        print("ERRO:  ... mais %d violação(ões) de esquema" % (len(errors) - 40))
    if hits or errors:
        print("FALHOU: %d problema(s)" % (len(hits) + len(errors)))
        return 1

    published = sum(1 for e in entregas for v in e["evidencias"] if v["publicada_no_site"])
    text = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    print("entregas: %d | pastas com captura no site: %d | capturas: %d | %d bytes"
          % (len(entregas), published, len(current["evidencias_publicadas"]), len(text.encode("utf-8"))))
    if args.dry_run:
        print("dry-run: nenhum arquivo escrito")
        return 0

    out_path = os.path.join(root, "docs", "data", "oot-dsi-progress.json")
    with open(out_path, "w", encoding="utf-8", newline="\r\n") as fh:
        fh.write(text)
    print("escrito: docs/data/oot-dsi-progress.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
