# Ocarina of Time DSi — visão geral

## O que é este projeto

Um port de *The Legend of Zelda: Ocarina of Time* para o Nintendo DSi. O jogo roda a partir do TWiLight Menu++ / nds-bootstrap em modo DSi, com o 3D na tela de cima desenhado pelo geometry engine do DS e a interface 2D na tela de baixo. O ARM9 do DSi não tem FPU, então a matemática de câmera, matrizes, projeção e seleção de textura é feita em ponto fixo.

O ponto de partida não é um emulador. O projeto usa a decompilação em C do jogo como base de código e traduz as display lists F3DEX2 do N64 para o pipeline 3D do DS. Cada instrução gráfica que o jogo emite é interpretada e reemitida no formato do DS.

## Duas frentes distintas

| Frente | O que resolve | Onde vive |
|---|---|---|
| Motor gráfico | decodificar F3DEX2, converter matrizes e projeção, texturas CI4/CI8/RGBA16, cache de textura, combiner, viewport, recorte no plano perto, seleção de nível de detalhe | `source/gfx/` |
| Integração com o jogo | tempo, escalonador, memória, entrada, leitura de assets, camada de compatibilidade Ultra64, código do decomp compilando para ARM9, carregamento de sala, telemetria | `source/nds/`, `source/ui/`, `source/main.c`, `game/oot` |

As duas frentes se encontram no momento em que uma display list real do jogo chega ao tradutor. Uma sala aparecer na tela prova a junção das duas; não prova nenhuma das duas isoladamente, e não prova o jogo.

## O que já foi demonstrado

Tudo abaixo foi medido no **melonDS 1.1, modo DS, 67 MHz** — emulador, não hardware físico:

- o ambiente do DSi foi sondado: modo, clock, heap, RAM, SD e duas telas;
- uma sala real do jogo, `market_day_room_0`, foi desenhada com 510 triângulos;
- uma sala texturizada, `Bmori1_room_0`, foi desenhada com 646 triângulos, 75 imagens e 2 paletas;
- o pontilhado colorido dessa sala foi diagnosticado como minificação, não como erro de paleta;
- uma cadeia de mip construída no bake reduziu o grão do painel de 13,28 para 9,09 (−32 %) ao custo de +17,5 ms de tempo de render;
- o custo da cadeia foi atribuído à aritmética por face, não à troca de textura, e uma tabela de recíprocos devolveu 2,1 ms sem alterar um pixel do painel.

## O que ainda não existe

Uma sala renderizada não é o jogo. Atores, colisão, câmera de jogo, animação, áudio, progressão, portas, transições de sala, salvamento, segunda sala, HUD/inventário na tela de baixo e execução em DSi físico continuam fora do escopo entregue. O estado de cada sistema está em [estado-e-limites.md](estado-e-limites.md).

## O que este projeto não afirma

- **Não há percentual global de conclusão.** Nenhuma metodologia verificável foi definida para transformar sistemas de naturezas diferentes em um número único, e um número assim seria uma promessa, não uma medida.
- **Resultado em emulador não é resultado em hardware.** O portão de DSi real continua aberto em todas as frentes.
- **Demonstração isolada não é promessa sobre o jogo inteiro.** Renderizar uma sala em um ângulo fixo não diz que o jogo inteiro rodará naquela taxa.
- **Medida de ruído não é medida de qualidade.** O grão do painel é um indicador próprio, com definição e limites declarados em [desempenho.md](desempenho.md).

## Como ler esta documentação

- [Linha do tempo](linha-do-tempo.md) — as entregas na ordem de registro, com identificador estável.
- [Estado e limites](estado-e-limites.md) — o que está implementado, testado onde, e o que falta.
- [Desempenho](desempenho.md) — o que cada número significa, como foi coletado e o que ele não mede.
- [Galeria](galeria.md) — capturas reais do port em execução.
- [Resumos por entrega](resumos/) — um arquivo por `OTD-NNN`, com problema, decisão, resultado e limitação.
- [Créditos e dependências](creditos.md) — origem do código, papel da decompilação e licenças.

O estudo técnico completo, com investigações que falharam, correções de conclusões anteriores e detalhes de reprodução, está nos dois históricos na raiz do projeto: [`HISTORICO_MOTOR_GRAFICO.md`](../../HISTORICO_MOTOR_GRAFICO.md) e [`HISTORICO_JOGO.md`](../../HISTORICO_JOGO.md).
