#!/usr/bin/env python3
"""Valida dados/progresso.json contra o esquema 1.0.0 e contra a árvore.

Confere, sem dependências externas:
  1. estrutura e enums do esquema (subconjunto declarado em progresso.schema.json);
  2. identidade das evidências: pasta existe, tem arquivo, e está no MANIFESTO;
  3. consistência com os históricos: a seção citada existe e a linha de ID
     daquela seção concorda com data, frente e estado do JSON;
  4. que nenhum documento público carrega caminho local, %TEMP% ou link de ROM.

Uso: python ferramentas/validar_progresso.py [--raiz DIR]
Sai com código 1 se houver erro.
"""

import argparse
import csv
import json
import os
import re
import sys

ID_RE = re.compile(r"^OTD-\d{3}$")
ENTRY_STATES = {
    "planejado", "em_implementacao", "implementado", "testado_computador",
    "testado_emulador", "testado_hardware", "parcial_bloqueado_regressao",
}
FRONTS = {"motor", "jogo", "motor+jogo"}
STATE_LABEL = {
    "planejado": "planejado",
    "em_implementacao": "em implementação",
    "implementado": "implementado",
    "testado_computador": "testado no computador",
    "testado_emulador": "testado em emulador",
    "testado_hardware": "testado em hardware físico",
    "parcial_bloqueado_regressao": "parcial",
}
INDICATORS = {
    "fps", "taxa_simulacao_hz", "tempo_quadro_ms", "tempo_render_ms",
    "tempo_submissao_ms", "grao", "contagem", "percentual", "vazao_mb_s",
}
ID_LINE_RE = re.compile(
    r"\*\*ID\*\*\s+`(OTD-\d{3})`\s+·\s+\*\*Data\*\*\s+([0-9-]{10})\s+·\s+"
    r"\*\*Frente\*\*\s+([^·]+?)\s+·\s+\*\*Fase\*\*\s+([^·]+?)\s+·\s+"
    r"\*\*Estado\*\*\s+([^·]+?)\s+·"
)
FORBIDDEN_PUBLIC = [
    (re.compile(r"%TEMP%", re.I), "%TEMP%"),
    (re.compile(r"[A-Za-z]:\\\\?Users", re.I), "caminho absoluto de usuário"),
    (re.compile(r"\bbaseroms?\b", re.I), "baserom"),
    (re.compile(r"\.(z64|n64)\b", re.I), "arquivo de ROM"),
]

errors = []
warnings = []


def err(msg):
    errors.append(msg)


def warn(msg):
    warnings.append(msg)


def load_json(path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def parse_histories(root):
    sections = {}
    for name in ("HISTORICO_MOTOR_GRAFICO.md", "HISTORICO_JOGO.md"):
        path = os.path.join(root, name)
        with open(path, "r", encoding="utf-8") as fh:
            lines = fh.read().splitlines()
        current = None
        block = []
        for line in lines:
            if line.startswith("## "):
                if current is not None:
                    sections[(name, current)] = "\n".join(block)
                current = line[3:].strip()
                block = []
            elif current is not None:
                block.append(line)
        if current is not None:
            sections[(name, current)] = "\n".join(block)
    return sections


def check_structure(data, root):
    if data.get("schema_version") != "1.0.0":
        err("schema_version deve ser 1.0.0")

    for key in ("projeto", "estados", "medicoes_definicoes", "entregas"):
        if key not in data:
            err("campo raiz ausente: " + key)
    if errors:
        return

    stored_states = set(data["estados"].keys())
    if stored_states != ENTRY_STATES:
        err("estados deve declarar exatamente os sete estados; faltam: %s; sobram: %s"
            % (sorted(ENTRY_STATES - stored_states), sorted(stored_states - ENTRY_STATES)))

    declared = {d["chave"] for d in data["medicoes_definicoes"]}
    if declared != INDICATORS:
        err("medicoes_definicoes não cobre exatamente os indicadores do esquema; "
            "faltam: %s; sobram: %s" % (sorted(INDICATORS - declared), sorted(declared - INDICATORS)))

    ids = []
    for entry in data["entregas"]:
        eid = entry.get("id", "?")
        if not ID_RE.match(eid):
            err("%s: id fora do padrão OTD-NNN" % eid)
        ids.append(eid)
        required = [
            "id", "tipo", "titulo", "data", "frente", "fase", "estado",
            "build", "resumo_publico", "estudo_engenharia", "problemas",
            "evidencias", "referencias_historicos",
        ]
        for key in required:
            if key not in entry:
                err("%s: campo obrigatório ausente: %s" % (eid, key))
        if entry.get("tipo") not in ("entrega", "correcao"):
            err("%s: tipo inválido" % eid)
        if entry.get("tipo") == "correcao":
            if not entry.get("entrega_pai"):
                err("%s: correção sem entrega_pai" % eid)
        elif entry.get("entrega_pai") not in (None, ""):
            err("%s: entrega normal não deve ter entrega_pai" % eid)
        if entry.get("frente") not in FRONTS:
            err("%s: frente inválida: %r" % (eid, entry.get("frente")))
        if entry.get("estado") not in ENTRY_STATES:
            err("%s: estado inválido: %r" % (eid, entry.get("estado")))
        if len(entry.get("resumo_publico", "")) < 40:
            err("%s: resumo_publico curto demais" % eid)
        build = entry.get("build", {})
        if "identificador" not in build or "commit" not in build:
            err("%s: build precisa de identificador e commit" % eid)
        estudo = entry.get("estudo_engenharia", {})
        for key in ("problema", "investigacao", "decisao", "resultado", "limitacao"):
            if not estudo.get(key):
                err("%s: estudo_engenharia.%s ausente" % (eid, key))
        for med in entry.get("medicoes", []):
            if med.get("indicador") not in INDICATORS:
                err("%s: indicador desconhecido: %r" % (eid, med.get("indicador")))
            cond = med.get("condicao", {})
            for key in ("cena", "camera", "build"):
                if not cond.get(key):
                    err("%s: medição sem condicao.%s" % (eid, key))
            for folder in med.get("evidencia", []):
                if not os.path.isdir(os.path.join(root, folder)):
                    err("%s: evidência citada não existe: %s" % (eid, folder))
        for prob in entry.get("problemas", []):
            if prob.get("estado") not in ("corrigido", "pendente", "regressao_conhecida"):
                err("%s: problema %s com estado inválido" % (eid, prob.get("id")))
        for folder in entry.get("evidencias", []):
            path = os.path.join(root, folder)
            if not os.path.isdir(path):
                err("%s: pasta de evidência não existe: %s" % (eid, folder))
            elif not any(os.scandir(path)):
                err("%s: pasta de evidência vazia: %s" % (eid, folder))
        if not entry.get("referencias_historicos"):
            err("%s: sem referência a histórico" % eid)

    if len(ids) != len(set(ids)):
        err("ids duplicados: %s" % sorted({i for i in ids if ids.count(i) > 1}))
    expected = ["OTD-%03d" % n for n in range(1, len(ids) + 1)]
    if ids != expected:
        err("ids fora de sequência ou com lacuna: %s" % ids)

    known = set(ids)
    for entry in data["entregas"]:
        if entry.get("entrega_pai") and entry["entrega_pai"] not in known:
            err("%s: entrega_pai inexistente: %s" % (entry["id"], entry["entrega_pai"]))

    for item in data.get("evidencias_ausentes", []):
        if not item.get("tag") or "arquivo não recuperado" not in item.get("situacao", ""):
            err("evidencias_ausentes: item incompleto: %r" % item)


def check_manifest(data, root):
    manifest = os.path.join(root, "evidencias", "MANIFESTO.csv")
    if not os.path.isfile(manifest):
        err("evidencias/MANIFESTO.csv ausente")
        return
    seen = set()
    with open(manifest, "r", encoding="utf-8", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            folder = (row.get("id") or "").strip()
            arquivo = (row.get("arquivo") or "").strip()
            if folder and arquivo:
                seen.add(folder + "/" + arquivo)
    for entry in data["entregas"]:
        for folder in entry.get("evidencias", []):
            path = os.path.join(root, folder)
            if not os.path.isdir(path):
                continue
            for name in os.listdir(path):
                rel = folder.rsplit("/", 1)[-1] + "/" + name
                if rel not in seen:
                    warn("%s: arquivo fora do MANIFESTO: %s" % (entry["id"], name))


def check_histories(data, root):
    sections = parse_histories(root)
    for entry in data["entregas"]:
        for ref in entry.get("referencias_historicos", []):
            key = (ref.get("arquivo"), ref.get("secao"))
            if key not in sections:
                err("%s: seção não encontrada em %s: %s" % (entry["id"], key[0], key[1]))
                continue
            block = sections[key]
            match = ID_LINE_RE.search(block)
            if not match:
                err("%s: seção %s sem linha de metadados de ID" % (entry["id"], key[1]))
                continue
            found_id, date, front, _phase, state = match.groups()
            state_text = block[match.end():]
            cut = state_text.find("**Evidência**")
            if cut != -1:
                state_text = state_text[:cut]
            state_text = " ".join(state_text.split()).lower()
            if entry["tipo"] == "correcao":
                if found_id != entry.get("entrega_pai"):
                    err("%s: correção deve estar sob a entrega-pai %s, mas a seção registra %s"
                        % (entry["id"], entry.get("entrega_pai"), found_id))
                if entry["id"] not in block:
                    err("%s: seção %s não cita a correção" % (entry["id"], key[1]))
                if date != entry["data"]:
                    err("%s: data %s no JSON, %s na seção %s" % (entry["id"], entry["data"], date, key[1]))
                continue
            if found_id != entry["id"]:
                err("%s: seção %s registra %s" % (entry["id"], key[1], found_id))
            if date != entry["data"]:
                err("%s: data %s no JSON, %s na seção %s" % (entry["id"], entry["data"], date, key[1]))
            if front.replace(" ", "") != entry["frente"].replace(" ", ""):
                err("%s: frente %s no JSON, %s na seção %s" % (entry["id"], entry["frente"], front.strip(), key[1]))
            label = STATE_LABEL[entry["estado"]]
            if label not in state_text:
                err("%s: estado %s no JSON, %r na seção %s"
                    % (entry["id"], entry["estado"], state.strip(), key[1]))

    # toda linha de ID nos históricos precisa existir no JSON
    json_ids = {e["id"] for e in data["entregas"]}
    for (name, secao), block in sections.items():
        for match in ID_LINE_RE.finditer(block):
            if match.group(1) not in json_ids:
                err("%s: histórico cita %s em %r, ausente do JSON" % (name, match.group(1), secao))


LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")


def check_public_links(root):
    public = os.path.join(root, "docs", "publico")
    if not os.path.isdir(public):
        return
    for base, _dirs, files in os.walk(public):
        for name in files:
            if not name.endswith(".md"):
                continue
            path = os.path.join(base, name)
            with open(path, "r", encoding="utf-8") as fh:
                text = fh.read()
            for target in LINK_RE.findall(text):
                target = target.split("#", 1)[0].strip()
                if not target or "://" in target or target.startswith("mailto:"):
                    continue
                resolved = os.path.normpath(os.path.join(base, target))
                if not os.path.exists(resolved):
                    err("link quebrado em %s: %s" % (os.path.relpath(path, root), target))


def check_public_docs(root):
    public = os.path.join(root, "docs", "publico")
    if not os.path.isdir(public):
        return
    for base, _dirs, files in os.walk(public):
        for name in files:
            if not name.endswith((".md", ".html", ".json")):
                continue
            path = os.path.join(base, name)
            with open(path, "r", encoding="utf-8") as fh:
                text = fh.read()
            for pattern, label in FORBIDDEN_PUBLIC:
                if pattern.search(text):
                    err("documento público com %s proibido: %s" % (label, os.path.relpath(path, root)))


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    parser = argparse.ArgumentParser()
    parser.add_argument("--raiz", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    args = parser.parse_args()
    root = os.path.abspath(args.raiz)

    data = load_json(os.path.join(root, "dados", "progresso.json"))
    schema = load_json(os.path.join(root, "dados", "progresso.schema.json"))
    if "medicao" not in schema.get("$defs", {}):
        err("progresso.schema.json sem $defs/medicao")

    check_structure(data, root)
    check_manifest(data, root)
    check_histories(data, root)
    check_public_docs(root)
    check_public_links(root)

    counts = {}
    for entry in data["entregas"]:
        counts[entry["estado"]] = counts.get(entry["estado"], 0) + 1
    evidence_files = set()
    for entry in data["entregas"]:
        for folder in entry.get("evidencias", []):
            path = os.path.join(root, folder)
            if os.path.isdir(path):
                for name in os.listdir(path):
                    if os.path.isfile(os.path.join(path, name)):
                        evidence_files.add(folder + "/" + name)
    total_files = len(evidence_files)
    total_meas = sum(len(e.get("medicoes", [])) for e in data["entregas"])

    for message in warnings:
        print("AVISO: " + message)
    for message in errors:
        print("ERRO:  " + message)
    print("entregas: %d | medições: %d | arquivos de evidência: %d"
          % (len(data["entregas"]), total_meas, total_files))
    print("estados: " + ", ".join("%s=%d" % (k, counts[k]) for k in sorted(counts)))
    if errors:
        print("FALHOU: %d erro(s)" % len(errors))
        return 1
    print("OK: progresso.json consistente com o esquema, os históricos e as evidências")
    return 0


if __name__ == "__main__":
    sys.exit(main())
