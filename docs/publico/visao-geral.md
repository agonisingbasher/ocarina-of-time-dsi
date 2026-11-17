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

59 das 72 entregas registradas terminaram com captura no **melonDS 1.1, modo DS, 67 MHz** — emulador, não hardware físico — e 13 fecharam apenas com validação no computador:

- o ambiente do DSi foi sondado: modo, clock, heap, RAM, SD e duas telas;
- uma sala real do jogo, `market_day_room_0`, foi desenhada com 510 triângulos, e uma sala texturizada, `Bmori1_room_0`, com 646 triângulos, 75 imagens e 2 paletas;
- o pontilhado colorido dessa sala foi diagnosticado como minificação, não como erro de paleta;
- uma cadeia de mip por triângulo foi construída no bake, com cor de vértice no combiner e lerp de duas texturas em segunda passada;
- o Link sai dos arquivos do próprio cartucho: esqueleto de 21 juntas, 89 *frames* de espera e 29 de caminhada lidos do arquivo, não de constantes inventadas;
- há fatia caminhável: o corpo para antes da parede, desliza nela, sobe degraus e a câmera de jogo não atravessa o muro — a colisão estática da cena é lida e usada por colisão, câmera e lógica de 20 Hz;
- duas salas de `Bmori1` coexistem, com porta `En_Door` de seis estados, porta de madeira animada com as duas faces originais e travessia exercitada de ida e volta no emulador com o cartão DLDI montado;
- atores de levantamento entram na tela: Talon com 422 faces batendo com o arquivo e Saria desenhando 459 triângulos sobre o fundo da sala;
- luz ambiente, duas luzes direcionais da cena, descarte de faces pelas listas originais, `G_CullDL` e câmeras originais alternando no visualizador;
- 101 cenas e 388 salas conferidas no computador por um comando que prepara salas, fundos e profundidades, recusando imagens corrompidas ou desatualizadas.

O preço dessas coisas também está medido. Na vista comparada, o quadro caiu de 154,8 ms para 50,1 ms (19–20 FPS na sala inicial de `Bmori1`, 65 ms de média na travessia com as duas salas desenhadas, 53 ms depois da pré-carga da sala vizinha). Um ator desenhado custa 39,02 ms por quadro no caminho atual. A cadeia de mip, no binário atual, custa +20,6 ms de render na sala 0 — 17,3 % do render enviado — e compra 0,40 unidades de grão (7,70 → 7,27, ou −5,6 %); o número de outubro (+16,4 ms, 34 % do render, −31 % de grão) está retratado em OTD-044.

## O que ainda não existe

Uma sala renderizada, um Link que anda e uma porta que troca de sala não são o jogo. Continua sem entrega registrada: o sistema geral de atores e objetos em tempo de execução (IA, interações, diálogos, drops, eventos persistentes), as ações completas do jogador (combate, inimigos, itens, natação, escalada, estados de dano), o áudio, o HUD de jogo com textos, menus, mapa, ocarina e inventário, salvar e carregar, e a campanha com masmorras, chefes e cutscenes carregadas pelo fluxo original. Nenhum DSi físico foi testado. O estado de cada sistema está em [estado-e-limites.md](estado-e-limites.md), e a meta de 30 FPS (45 onde a cadência de 60 Hz permitir, com a lógica a 20 Hz) é critério de aceite do plano — nenhum build mediu essa meta no alvo.

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
