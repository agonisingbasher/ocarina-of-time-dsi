# Histórico do Port — Ocarina of Time DSi

O histórico foi dividido em duas séries, uma para cada frente de trabalho. Esta
página é só o índice; as entradas recentes ficam no topo de cada arquivo.

| Série | Arquivo | Escopo | Última entrada |
|---|---|---|---|
| Motor gráfico | [`HISTORICO_MOTOR_GRAFICO.md`](HISTORICO_MOTOR_GRAFICO.md) | `source/gfx/` — decodificador de display lists F3DEX2, tradução para o geometry engine, texturas, cache, combiner, viewport, recorte | Fase 12 — a troca de textura custa 0,45 µs; quem cobra é a aritmética do mip, 2026-10-27 |
| Jogo | [`HISTORICO_JOGO.md`](HISTORICO_JOGO.md) | `source/nds/`, `source/ui/`, `source/main.c`, `game/oot` — plataforma, loop, memória, streaming, entrada, UI das duas telas, áudio, integração | Fase 7 — a régua de medida foi consertada, e ela refutou a fase 6 por engano antes de se refutar a si mesma, 2026-10-27 |

Regras comuns às duas séries:

- Nenhuma etapa é marcada como concluída sem medição no hardware indicado
  (hoje melonDS 1.1 em **DS mode**, 67 MHz; o portão de DSi real continua aberto
  nas duas frentes).
- Cada entrada traz objetivo, entregue, descobertas medidas, validação, erros
  meus e limites.
- Uma mudança que afete as duas frentes é registrada nos dois arquivos, com
  referência cruzada e o **mesmo identificador**.
- Cada entrada carrega uma linha de metadados com ID, data, frente, fase, estado,
  pasta de evidência e commit. O commit é declarado indisponível porque a árvore
  não tem histórico git; nenhum hash de commit é inventado.
- As capturas citadas vivem em [`evidencias/`](evidencias/), com SHA-256 em
  `evidencias/MANIFESTO.csv`. Evidência perdida não é
  recriada: fica registrada como ausente.
- Os dados estruturados ficam em [`dados/progresso.json`](dados/progresso.json),
  validados por `ferramentas/validar_progresso.py` contra o esquema
  [`dados/progresso.schema.json`](dados/progresso.schema.json).

## Registro de identificadores

O `OTD-NNN` identifica uma **entrega registrada**, não uma fase. A ordem é a de
registro no histórico, que não é necessariamente a ordem de execução nem a
ordem de importância; a data identifica o dia, e a fase identifica a área de
trabalho. Uma correção que não constitui avanço independente (`OTD-025`) fica
sob a entrega que corrige e não é contada duas vezes. Não se renumera uma
entrega publicada: se uma conclusão muda, entra uma retratação com o mesmo ID ou
uma correção com ID novo e referência à entrega-pai.

| ID | Entrega | Frente | Histórico |
|---|---|---|---|
| `OTD-001` | sonda de hardware | jogo | `HISTORICO_JOGO.md` |
| `OTD-002` | base do loop | jogo | `HISTORICO_JOGO.md` |
| `OTD-003` | decodificador F3DEX2 (preparação) | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-004` | primeira display list na tela | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-005` | matriz da display list no GE | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-006` | projeção e pilha por tipo | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-007` | texturas CI4/CI8 | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-008` | cache de textura (LRU) | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-009` | combiner e viewport | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-010` | alpha do texel como recorte | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-011` | OtherMode e blending | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-012` | recorte paletado e VRAM | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-013` | cache de 64 slots | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-014` | camada Ultra64 | jogo | `HISTORICO_JOGO.md` |
| `OTD-015` | display list do jogo no painel | jogo | `HISTORICO_JOGO.md` |
| `OTD-016` | `sys_matrix.c` como modelview | jogo | `HISTORICO_JOGO.md` |
| `OTD-017` | viewport de dois cantos | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-018` | profundidade e translação | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-019` | recorte no plano perto | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-020` | entrada do DS → `BTN_*` | jogo | `HISTORICO_JOGO.md` |
| `OTD-021` | `nds/fs` e streaming | jogo | `HISTORICO_JOGO.md` |
| `OTD-022` | primeira sala real | motor + jogo | os dois |
| `OTD-023` | sala texturizada e minificação | motor + jogo | os dois |
| `OTD-024` | régua de câmera e mip por triângulo | motor + jogo | os dois |
| `OTD-025` | correção 4096× da sonda (dentro de `OTD-024`) | motor | os dois |
| `OTD-026` | custo do mip e tabela de recíprocos | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-027` | espaço de olho e área em 32 bits | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-028` | cor de vértice no combiner e censo do jogo inteiro | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-029` | lerp de duas texturas em segunda passada | motor | `HISTORICO_MOTOR_GRAFICO.md` |
| `OTD-030` | preço da troca de textura e do mip; semântica dos contadores | motor + jogo | `HISTORICO_MOTOR_GRAFICO.md` |

Plano de trabalho completo: `coral-hill-wren.md`, mantido fora do repositório no
diretório de planos do editor.
