#!/usr/bin/env python3
"""Gera os documentos públicos em `docs/publico/` a partir do recorte público do site.

Diferente da cópia desta ferramenta no projeto de desenvolvimento, aqui a fonte é
`docs/data/oot-dsi-progress.json` — o recorte já sanitizado e conferido por esquema,
que é o mesmo arquivo que a página cita. Assim a camada pública não pode divergir
do que o site anuncia.

Saídas (todas sob docs/publico/):
  resumos/OTD-NNN.md   resumo legível por entrega
  linha-do-tempo.md    ordem de registro das entregas
  estado-e-limites.md  estado por entrega, pendências e o que ainda não existe
  galeria.md           as capturas realmente publicadas neste repositório

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

# Espelha o bloco "ainda não existe" e o "parcial do motor" da página, em docs/index.html.
NAO_EXISTE = (
    "sistema geral de atores e objetos em tempo de execução: IA, interações, diálogos, drops e eventos persistentes",
    "ações completas do jogador: combate, inimigos, itens, natação, escalada e estados de dano",
    "áudio do jogo: sequências, síntese, bancos e efeitos sonoros integrados ao tempo de execução",
    "HUD de jogo, textos, menus, mapa, ocarina, equipamentos e inventário utilizável",
    "salvar e carregar: três arquivos, formato persistente, integridade e seleção",
    "campanha, masmorras, chefes e cutscenes; carregar áreas pelo fluxo original do jogo, com cenas e objetos conectados em tempo de execução",
    "execução em DSi físico: nenhuma entrega tem validação em hardware real",
)
MOTOR_PARCIAL = (
    "ciclo 1 do combiner, fontes ENV/PRIMITIVE, `G_TT_IA16`, texturas retangulares, wrap/mask/shift e recorte em `x = ±w` e `y = ±w`",
    "20 808 triângulos (14,9 %) pedem iluminação do RSP e continuam fora do caminho",
    "4 063 triângulos (19,3 %) têm segunda imagem CI/IA/I, fora do caminho RGBA16 do lerp",
    "a cadeia de mip custa +20,6 ms de render na sala 0 — 17,3 % do render enviado — e compra 0,40 unidades de grão, preço medido em OTD-044",
    "o limite de 750 px por canto e o custo de 0,5 ms da cor de vértice seguem sem explicação fechada",
    "um ator desenhado custa 39,02 ms por quadro no caminho atual e sozinho estoura o orçamento de 33 ms da meta de 30 FPS",
    "a folha da porta ainda não se move entre f1.0 e f11.0, e o quadro da troca de sala pode mostrar a sala antiga contra a nova",
)


def fmt_number(value):
    if isinstance(value, bool):
        return str(value)
    if isinstance(value, int):
        return str(value)
    if float(value).is_integer():
        return str(int(value))
    return ("%g" % value).replace(".", ",")


def load(root):
    with open(os.path.join(root, "docs", "data", "oot-dsi-progress.json"), "r", encoding="utf-8") as fh:
        return json.load(fh)


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def resumo(data, entry):
    eid = entry["id"]
    defs = {d["chave"]: d["nome"] for d in data["medicoes_definicoes"]}
    lines = []
    lines.append("# %s — %s" % (eid, entry["titulo"]))
    lines.append("")
    lines.append("> Gerado de `docs/data/oot-dsi-progress.json` (esquema 1.0.0). Para alterar, edite `dados/progresso.json` do projeto, rode `ferramentas/gerar_recorte_publico.py` e depois este script; não edite este arquivo.")
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
    lines.append("| Validado em | %s |" % entry["validacao"]["ambiente"])
    lines.append("")
    lines.append("## Resumo")
    lines.append("")
    lines.append(entry["resumo"])
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
    lines.append("**Limitação.** " + " ".join(entry["limitacoes"]))
    lines.append("")

    if entry.get("medicoes"):
        lines.append("## Medições")
        lines.append("")
        lines.append("A condição de cada linha está declarada no arquivo. Salvo indicação contrária, o ambiente é melonDS 1.1, modo DS, 67 MHz — ou o computador, nas entregas testadas fora do emulador. Valor de emulador não é medição de hardware físico.")
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
    published = [v for v in entry.get("evidencias", []) if v["arquivos_publicados"]]
    if published:
        for evidence in published:
            for name in evidence["arquivos_publicados"]:
                lines.append("- ![](%s)" % ("../../" + name))
                lines.append("")
                lines.append("  [`%s`](../../%s) — captura desta entrega, com hash em [`evidencias/publicas/MANIFESTO.csv`](../../../evidencias/publicas/MANIFESTO.csv)." % (name, name))
            lines.append("- A pasta `%s` fica no projeto de desenvolvimento; o conjunto completo não é distribuído por este repositório." % evidence["referencia"])
    else:
        for evidence in entry.get("evidencias", []):
            lines.append("- `%s` — %s" % (evidence["referencia"], evidence["observacao"]))
        if not entry.get("evidencias"):
            lines.append("Sem pasta de evidência. Os valores desta entrega continuam transcritos no histórico técnico, com o mesmo `%s`." % eid)
    lines.append("")

    lines.append("## Onde ler o estudo completo")
    lines.append("")
    for ref in entry["referencias_historicos"]:
        label = ref.get("rotulo_publico") or ref["secao"]
        lines.append("- Histórico técnico do projeto: `%s`, seção \"%s\"." % (ref["arquivo"], label))
    lines.append("")
    return "\n".join(lines)


def timeline(data):
    dates = sorted({e["data"] for e in data["entregas"]})
    lines = []
    lines.append("# Linha do tempo")
    lines.append("")
    lines.append("> Gerado de `docs/data/oot-dsi-progress.json`. Não editar à mão.")
    lines.append("")
    lines.append(
        "O `OTD-NNN` é atribuído na ordem de registro do histórico, não pela ordem de execução. "
        "As %d entregas vão de %s a %s; a data identifica o dia do registro, e a coluna *Fase* identifica a área de trabalho. "
        "Uma correção (`OTD-025`) aparece sob a entrega que corrige e não conta como avanço independente."
        % (len(data["entregas"]), dates[0], dates[-1])
    )
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
    lines.append("> Gerado de `docs/data/oot-dsi-progress.json`. Não editar à mão.")
    lines.append("")
    lines.append("## Vocabulário de estado")
    lines.append("")
    lines.append("Cada entrega recebe um estado só. O vocabulário separa implementação de validação de propósito: *implementado* não significa testado, e *testado em emulador* não significa testado em hardware físico.")
    lines.append("")
    lines.append("| Estado | Significado |")
    lines.append("|---|---|")
    for key in data["estados"]:
        lines.append("| %s | %s |" % (data["estados"][key]["rotulo"], data["estados"][key]["descricao"]))
    lines.append("")
    lines.append("## Estado por entrega")
    lines.append("")
    lines.append("| ID | Entrega | Frente | Estado | Limite registrado |")
    lines.append("|---|---|---|---|---|")
    for e in data["entregas"]:
        lines.append("| [%s](resumos/%s.md) | %s | %s | %s | %s |" % (
            e["id"], e["id"], e["titulo"], FRONT_LABEL[e["frente"]], STATE_LABEL[e["estado"]],
            " ".join(e["limitacoes"]).replace("\n", " ").replace("|", "\\|")))
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
    lines.append("Uma sala renderizada, um Link que anda e uma porta que troca de sala não são o jogo. Cada sistema abaixo continua sem entrega registrada:")
    lines.append("")
    for item in NAO_EXISTE:
        lines.append("- " + item)
    lines.append("")
    lines.append("O estado do motor também é parcial:")
    lines.append("")
    for item in MOTOR_PARCIAL:
        lines.append("- " + item)
    lines.append("")
    lines.append("A lista detalhada fica em `HISTORICO_MOTOR_GRAFICO.md`, seção \"Onde o motor está\" — a cópia deste repositório cobre as entregas registradas até 2026-10-27; a numeração vigente está no recorte estruturado.")
    lines.append("")
    return "\n".join(lines)


def gallery(data):
    published = data["evidencias_publicadas"]
    lines = []
    lines.append("# Galeria")
    lines.append("")
    lines.append("> Gerado de `docs/data/oot-dsi-progress.json`, campo `evidencias_publicadas`. As imagens são capturas reais do melonDS 1.1 em modo DS, não renderizações ilustrativas e não hardware físico.")
    lines.append("")
    lines.append(
        "São %d capturas, e elas são tudo o que este repositório publica: cada uma mostra o painel 3D do DS "
        "(ou a janela completa) em um build e uma câmera declarados. Uma captura prova o que estava na tela "
        "naquele quadro, não que o jogo inteiro funciona. O conjunto de evidências de cada entrega fica no "
        "projeto de desenvolvimento, com hash em `evidencias/publicas/MANIFESTO.csv`." % len(published)
    )
    lines.append("")
    for item in published:
        lines.append("## %s — `%s`" % (item["entrega"], os.path.basename(item["arquivo_site"])))
        lines.append("")
        lines.append("![%s — %s](../%s)" % (item["entrega"], item["tag"], item["arquivo_site"]))
        lines.append("")
        lines.append(item["descricao"])
        lines.append("")
        lines.append("| Campo | Valor |")
        lines.append("|---|---|")
        lines.append("| Cena | %s |" % item["cena"])
        lines.append("| Câmera | %s |" % item["camera"])
        lines.append("| Build | %s |" % item["build"])
        lines.append("| Configuração | %s |" % item["configuracao"])
        lines.append("| Leitura | %s |" % item["leitura"])
        lines.append("")
        lines.append("Detalhes da entrega em [resumo de %s](resumos/%s.md)." % (item["entrega"], item["entrega"]))
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
        write(os.path.join(public, "resumos", entry["id"] + ".md"), resumo(data, entry))
    write(os.path.join(public, "linha-do-tempo.md"), timeline(data))
    write(os.path.join(public, "estado-e-limites.md"), estado_e_limites(data))
    write(os.path.join(public, "galeria.md"), gallery(data))

    print("gerados: %d resumos + linha-do-tempo, estado-e-limites e galeria com %d capturas publicadas"
          % (len(data["entregas"]), len(data["evidencias_publicadas"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
