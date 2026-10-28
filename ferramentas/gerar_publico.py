#!/usr/bin/env python3
"""Gera os documentos públicos derivados de dados/progresso.json.

Saídas (todas sob docs/publico/):
  resumos/OTD-NNN.md   resumo legível por entrega
  linha-do-tempo.md    ordem de registro das entregas
  estado-e-limites.md  estado por entrega, pendências e o que ainda não existe
  galeria.md           capturas reais selecionadas de evidencias/

Os documentos editoriais (visao-geral, desempenho, creditos, README) são
escritos à mão. Este script não publica nada: só grava arquivos locais.

Uso: python ferramentas/gerar_publico.py [--raiz DIR]
"""

import argparse
import json
import os
import sys

STATE_LABEL = {
    "planejado": "Planejado",
    "em_implementacao": "Em implementação",
    "implementado": "Implementado",
    "testado_computador": "Testado no computador",
    "testado_emulador": "Testado em emulador",
    "testado_hardware": "Testado em hardware físico",
    "parcial_bloqueado_regressao": "Parcial, bloqueado ou com regressão conhecida",
}
FRONT_LABEL = {"motor": "motor gráfico", "jogo": "jogo", "motor+jogo": "motor gráfico + jogo"}


def fmt_number(value):
    if isinstance(value, bool):
        return str(value)
    if isinstance(value, int):
        return str(value)
    if float(value).is_integer():
        return str(int(value))
    return ("%g" % value).replace(".", ",")


def load(root):
    with open(os.path.join(root, "dados", "progresso.json"), "r", encoding="utf-8") as fh:
        return json.load(fh)


def evidence_counts(root, folder):
    path = os.path.join(root, folder)
    if not os.path.isdir(path):
        return 0
    return len([n for n in os.listdir(path) if os.path.isfile(os.path.join(path, n))])


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def resumo(data, root, entry):
    eid = entry["id"]
    defs = {d["chave"]: d["nome"] for d in data["medicoes_definicoes"]}
    lines = []
    lines.append("# %s — %s" % (eid, entry["titulo"]))
    lines.append("")
    lines.append("> Gerado de `dados/progresso.json` (esquema 1.0.0). Para alterar, edite o JSON e rode `ferramentas/gerar_publico.py`; não edite este arquivo.")
    lines.append("")
    if entry["tipo"] == "correcao":
        lines.append("**Correção dentro da entrega %s.** Não conta como avanço independente." % entry.get("entrega_pai"))
        lines.append("")
    lines.append("| Campo | Valor |")
    lines.append("|---|---|")
    lines.append("| Data | %s |" % entry["data"])
    lines.append("| Frente | %s |" % FRONT_LABEL[entry["frente"]])
    lines.append("| Fase | %s |" % entry["fase"])
    lines.append("| Estado | %s |" % STATE_LABEL[entry["estado"]])
    if entry.get("subestado"):
        lines.append("| Detalhe de estado | %s |" % entry["subestado"])
    lines.append("| Build | %s |" % entry["build"]["identificador"])
    lines.append("| Commit | %s |" % (entry["build"]["commit"] or "não disponível (árvore sem histórico git)"))
    lines.append("")
    lines.append("## Resumo")
    lines.append("")
    lines.append(entry["resumo_publico"])
    lines.append("")
    lines.append("## Estudo de engenharia")
    lines.append("")
    estudo = entry["estudo_engenharia"]
    lines.append("**Problema.** " + estudo["problema"])
    lines.append("")
    lines.append("**Como foi investigado.** " + estudo["investigacao"])
    lines.append("")
    lines.append("**Decisão.** " + estudo["decisao"])
    lines.append("")
    lines.append("**Resultado.** " + estudo["resultado"])
    lines.append("")
    lines.append("**Limitação.** " + estudo["limitacao"])
    lines.append("")

    if entry.get("medicoes"):
        lines.append("## Medições")
        lines.append("")
        lines.append("Ambiente padrão: melonDS 1.1, modo DS, 67 MHz. A condição de cada linha está declarada; valores de emulador não são medição de hardware físico.")
        lines.append("")
        lines.append("| Indicador | Valor | Condição | Método |")
        lines.append("|---|---|---|---|")
        for med in entry["medicoes"]:
            cond = med["condicao"]
            cond_txt = "%s; câmera %s; build %s" % (cond["cena"], cond["camera"], cond["build"])
            if cond.get("configuracao"):
                cond_txt += "; " + cond["configuracao"]
            if cond.get("angulo"):
                cond_txt += "; ângulo " + cond["angulo"]
            if med.get("observacao"):
                cond_txt += ". " + med["observacao"]
            lines.append("| %s | %s %s | %s | %s |" % (
                defs.get(med["indicador"], med["indicador"]), fmt_number(med["valor"]), med["unidade"], cond_txt, med["metodo"]))
        lines.append("")

    if entry.get("testes"):
        lines.append("## Testes registrados")
        lines.append("")
        for test in entry["testes"]:
            lines.append("- **%s** — %s. Resultado: %s" % (test["tipo"], test["ambiente"], test["resultado"]))
        lines.append("")

    pend = [p for p in entry.get("problemas", []) if p["estado"] != "corrigido"]
    fix = [p for p in entry.get("problemas", []) if p["estado"] == "corrigido"]
    if pend or fix:
        lines.append("## Problemas e correções")
        lines.append("")
        for p in fix:
            lines.append("- **Corrigido** (%s): %s" % (p["id"], p["descricao"]))
        for p in pend:
            label = "Regressão conhecida" if p["estado"] == "regressao_conhecida" else "Pendente"
            lines.append("- **%s** (%s): %s" % (label, p["id"], p["descricao"]))
        lines.append("")

    if entry.get("retratacoes"):
        lines.append("## Conclusões anteriores corrigidas")
        lines.append("")
        for r in entry["retratacoes"]:
            lines.append("- **Afirmação anterior:** %s" % r["afirmacao_anterior"])
            lines.append("  **Evidência nova:** %s" % r["evidencia_nova"])
            lines.append("  **Conclusão vigente:** %s" % r["conclusao_vigente"])
            lines.append("  **Registrada em:** %s." % r["registrada_em"])
        lines.append("")

    lines.append("## Evidências")
    lines.append("")
    folders = entry.get("evidencias", [])
    if folders:
        for folder in folders:
            count = evidence_counts(root, folder)
            lines.append("- [`%s/`](../../../%s/) — %d arquivo(s), com SHA-256 em [`evidencias/MANIFESTO.csv`](../../../evidencias/MANIFESTO.csv)." % (folder, folder, count))
    else:
        lines.append("Sem captura recuperada. Os valores desta entrega continuam transcritos no histórico técnico, com o mesmo `%s`." % eid)
    lines.append("")

    lines.append("## Onde ler o estudo completo")
    lines.append("")
    for ref in entry["referencias_historicos"]:
        label = ref.get("rotulo_publico") or ref["secao"]
        lines.append("- [`%s`](../../../%s), seção \"%s\"." % (ref["arquivo"], ref["arquivo"], label))
    lines.append("")
    return "\n".join(lines)


def timeline(data):
    lines = []
    lines.append("# Linha do tempo")
    lines.append("")
    lines.append("> Gerado de `dados/progresso.json`. Não editar à mão.")
    lines.append("")
    lines.append("O `OTD-NNN` é atribuído na ordem de registro do histórico, não pela ordem de execução. Quase todas as entregas foram registradas em 2026-10-27; a data identifica o dia do registro, e a coluna *Fase* identifica a área de trabalho. Uma correção (`OTD-025`) aparece sob a entrega que corrige e não conta como avanço independente.")
    lines.append("")
    lines.append("| Data | ID | Frente | Fase | Entrega | Estado |")
    lines.append("|---|---|---|---|---|---|")
    ordered = sorted(data["entregas"], key=lambda e: (e["data"], e["id"]))
    for e in ordered:
        titulo = e["titulo"]
        if e["tipo"] == "correcao":
            titulo = "*(correção de %s)* %s" % (e.get("entrega_pai"), titulo)
        lines.append("| %s | [%s](resumos/%s.md) | %s | %s | %s | %s |" % (
            e["data"], e["id"], e["id"], FRONT_LABEL[e["frente"]], e["fase"], titulo, STATE_LABEL[e["estado"]]))
    lines.append("")
    return "\n".join(lines)


def estado_e_limites(data):
    lines = []
    lines.append("# Estado e limites")
    lines.append("")
    lines.append("> Gerado de `dados/progresso.json`. Não editar à mão.")
    lines.append("")
    lines.append("## Vocabulário de estado")
    lines.append("")
    lines.append("Cada entrega recebe um estado só. O vocabulário separa implementação de validação de propósito: *implementado* não significa testado, e *testado em emulador* não significa testado em hardware físico.")
    lines.append("")
    lines.append("| Estado | Significado |")
    lines.append("|---|---|")
    for key in ("planejado", "em_implementacao", "implementado", "testado_computador", "testado_emulador", "testado_hardware", "parcial_bloqueado_regressao"):
        lines.append("| %s | %s |" % (data["estados"][key]["rotulo"], data["estados"][key]["descricao"]))
    lines.append("")
    lines.append("## Estado por entrega")
    lines.append("")
    lines.append("| ID | Entrega | Frente | Estado | Limite registrado |")
    lines.append("|---|---|---|---|---|")
    for e in data["entregas"]:
        limit = e["estudo_engenharia"]["limitacao"]
        lines.append("| [%s](resumos/%s.md) | %s | %s | %s | %s |" % (
            e["id"], e["id"], e["titulo"], FRONT_LABEL[e["frente"]], STATE_LABEL[e["estado"]], limit))
    lines.append("")
    lines.append("## Pendências e regressões conhecidas")
    lines.append("")
    lines.append("| ID | Problema | Situação |")
    lines.append("|---|---|---|")
    for e in data["entregas"]:
        for p in e.get("problemas", []):
            if p["estado"] != "corrigido":
                situacao = "Regressão conhecida" if p["estado"] == "regressao_conhecida" else "Pendente"
                lines.append("| %s | %s — %s | %s |" % (e["id"], p["id"], p["descricao"], situacao))
    lines.append("")
    lines.append("## O que ainda não existe")
    lines.append("")
    lines.append("Uma sala renderizada não é o jogo. Cada sistema abaixo continua com estado próprio, e nenhum deles está coberto por uma entrega registrada:")
    lines.append("")
    for item in (
        "atores e objetos de cena em tempo de execução",
        "colisão e física do jogador",
        "câmera de jogo (a câmera presa é instrumento de medida, não a câmera do OoT)",
        "animação e esqueleto",
        "áudio",
        "progressão, portas, transições de sala e salvamento",
        "segunda sala e carregamento de cena completo",
        "HUD e inventário 2D na tela de baixo",
        "execução em DSi físico",
    ):
        lines.append("- " + item)
    lines.append("")
    lines.append("O estado do motor também é parcial: ciclo 1 do combiner, fontes ENV/PRIMITIVE, `G_TT_IA16`, texturas retangulares, wrap/mask/shift e recorte em `x = ±w` / `y = ±w` não estão cobertos. A lista detalhada fica em [`HISTORICO_MOTOR_GRAFICO.md`](../../HISTORICO_MOTOR_GRAFICO.md), seção \"Onde o motor está\".")
    lines.append("")
    return "\n".join(lines)


def pick_captures(root, folder, limit=3):
    path = os.path.join(root, folder)
    if not os.path.isdir(path):
        return []
    names = sorted(n for n in os.listdir(path) if n.lower().endswith(".png"))
    chosen = []

    def take(predicate, count):
        for name in names:
            if len(chosen) >= limit or count <= 0:
                return
            if name in chosen:
                continue
            if predicate(name):
                chosen.append(name)
                count -= 1

    take(lambda n: n.endswith("_topbig.png"), 2)
    take(lambda n: n.endswith("_top.png"), 1)
    take(lambda n: not n.endswith(("_hud.png", "_fps.png", "_tail.png", "_top.png", "_topbig.png")), 1)
    return chosen


def gallery(data, root):
    lines = []
    lines.append("# Galeria")
    lines.append("")
    lines.append("> Gerado de `dados/progresso.json` e `evidencias/MANIFESTO.csv`. As imagens são capturas reais do melonDS 1.1 em modo DS, não renderizações ilustrativas e não hardware físico.")
    lines.append("")
    lines.append("Cada imagem mostra o painel 3D do DS (ou a janela completa) em um estado de teste. Uma captura prova o que estava na tela naquele build e naquela câmera; não prova que o jogo inteiro funciona. O conjunto completo, com hash de cada arquivo, está em [`evidencias/`](../../evidencias/).")
    lines.append("")
    for e in data["entregas"]:
        folders = e.get("evidencias", [])
        if not folders:
            continue
        lines.append("## %s — %s" % (e["id"], e["titulo"]))
        lines.append("")
        for folder in folders:
            chosen = pick_captures(root, folder)
            if not chosen:
                continue
            for name in chosen:
                rel = "../../" + folder + "/" + name
                lines.append("![%s — %s](%s)" % (e["id"], name, rel))
                lines.append("")
                lines.append("*`%s` — captura de emulador; condição e leitura estão no [resumo da entrega](resumos/%s.md).*" % (name, e["id"]))
                lines.append("")
            count = evidence_counts(root, folder)
            lines.append("Pasta completa: [`%s/`](../../%s/) (%d arquivo(s))." % (folder, folder, count))
            lines.append("")
    return "\n".join(lines)


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except AttributeError:
        pass
    parser = argparse.ArgumentParser()
    parser.add_argument("--raiz", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    args = parser.parse_args()
    root = os.path.abspath(args.raiz)
    data = load(root)
    public = os.path.join(root, "docs", "publico")

    for entry in data["entregas"]:
        write(os.path.join(public, "resumos", entry["id"] + ".md"), resumo(data, root, entry))
    write(os.path.join(public, "linha-do-tempo.md"), timeline(data))
    write(os.path.join(public, "estado-e-limites.md"), estado_e_limites(data))
    write(os.path.join(public, "galeria.md"), gallery(data, root))

    print("gerados: %d resumos + linha-do-tempo, estado-e-limites, galeria" % len(data["entregas"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
