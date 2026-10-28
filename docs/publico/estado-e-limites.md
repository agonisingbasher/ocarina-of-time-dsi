# Estado e limites

> Gerado de `dados/progresso.json`. Não editar à mão.

## Vocabulário de estado

Cada entrega recebe um estado só. O vocabulário separa implementação de validação de propósito: *implementado* não significa testado, e *testado em emulador* não significa testado em hardware físico.

| Estado | Significado |
|---|---|
| Planejado | Escopo definido; nenhum código da entrega foi escrito. |
| Em implementação | Código em andamento; nenhuma medição fechada. |
| Implementado | Compila e roda, mas não passou por teste registrado na condição real de uso. |
| Testado no computador | Validado por autoteste ou verificação fora do DS (por exemplo, no host). |
| Testado em emulador | Validado no melonDS 1.1 em modo DS, com evidência registrada. Não é hardware físico. |
| Testado em hardware físico | Validado em DSi real via TWiLight Menu++ / nds-bootstrap. Nenhuma entrega chegou aqui. |
| Parcial, bloqueado ou com regressão conhecida | Entrega incompleta, impedida por dependência externa ou com defeito conhecido registrado. |

## Estado por entrega

| ID | Entrega | Frente | Estado | Limite registrado |
|---|---|---|---|---|
| [OTD-001](resumos/OTD-001.md) | Sonda de hardware: o DSi emulado entrega DSi mode, RAM e duas telas | jogo | Testado em emulador | Tudo foi medido em modo DS. Confirmar DSi mode, heap de ~15,9 MB, bit de 134 MHz e SD real exige um DSi físico; sem isso o projeto para. |
| [OTD-002](resumos/OTD-002.md) | Base do loop: tempo, escalonador de 20 Hz, memória e telemetria | jogo | Testado em emulador | Nada foi lido do SD ainda: o adaptador de arquivo entra com nds/fs. A vazão de SD do emulador não existe e só pode ser medida em hardware. |
| [OTD-003](resumos/OTD-003.md) | Decodificador F3DEX2: ler display lists do decomp no computador | motor gráfico | Testado no computador | Ainda não havia lista desenhada na tela nem verificação no emulador; esta entrega é testada apenas no computador. |
| [OTD-004](resumos/OTD-004.md) | Primeira display list F3DEX2 desenhada pelo geometry engine | motor gráfico | Testado em emulador | A matriz ainda vinha do teste, não da lista; sem isso a geometria não tinha posição definida pelo jogo. |
| [OTD-005](resumos/OTD-005.md) | Matriz da display list aplicada no geometry engine | motor gráfico | Testado em emulador | A projeção ainda era fixa; faltava distinguir matriz de modelview e de projeção. |
| [OTD-006](resumos/OTD-006.md) | Projeção vinda da display list e pilha de matrizes por tipo | motor gráfico | Testado em emulador | Recorte no plano perto e correção de profundidade ainda não existiam. |
| [OTD-007](resumos/OTD-007.md) | Texturas CI4 e CI8 com paleta por slot de TMEM | motor gráfico | Testado em emulador | O alpha de paleta ainda não era tratado; o GE do DS tem paleta de 15 bits, sem alpha. |
| [OTD-008](resumos/OTD-008.md) | Cache de textura com substituição LRU | motor gráfico | Testado em emulador | O número de slots ainda era um palpite; a política de recusa em cache cheio ficava visível como triângulo sem textura. |
| [OTD-009](resumos/OTD-009.md) | Combiner do ciclo 0 e viewport do N64 | motor gráfico | Testado em emulador | Ciclo 1, fontes ENV/PRIMITIVE e blending ainda não cobertos. |
| [OTD-010](resumos/OTD-010.md) | Alpha do texel como recorte | motor gráfico | Testado em emulador | Blending translúcido de verdade ainda não existia, e o alpha de paleta CI não podia ser representado no GE. |
| [OTD-011](resumos/OTD-011.md) | OtherMode e blending translúcido por polígono | motor gráfico | Testado em emulador | Ciclo 1, fontes ENV/PRIMITIVE e recorte via paleta continuam não cobertos. |
| [OTD-012](resumos/OTD-012.md) | Recorte por alpha de paleta CI e disciplina de VRAM | motor gráfico | Testado em emulador | O caminho de recusa ainda não tinha sido substituído por capacidade; isso veio na entrega seguinte. |
| [OTD-013](resumos/OTD-013.md) | Cache de 64 slots: o despejo a meio do frame não existe | motor gráfico | Testado em emulador | O melonDS só zera a Polygon RAM na fronteira do frame, então o despejo a meio do frame não é verificável no emulador; a hipótese de que o hardware real libera slots no meio da lista fica para o DSi físico. E 26 texturas não são uma sala real. |
| [OTD-014](resumos/OTD-014.md) | Camada Ultra64: o primeiro arquivo do decomp compila para ARM9 | jogo | Testado em emulador | O fecho é pequeno: ainda não há display list montada pelo código do jogo nem pilha de matrizes do jogo. |
| [OTD-015](resumos/OTD-015.md) | Display list montada pelo código do jogo pinta pixels | jogo | Testado em emulador | A modelview ainda vinha de teste; a pilha de matrizes do jogo entra na entrega seguinte. |
| [OTD-016](resumos/OTD-016.md) | sys_matrix.c gera a modelview do jogo | jogo | Testado em emulador | A cena ainda é de teste; a sala real entra na fase 6. |
| [OTD-017](resumos/OTD-017.md) | Viewport de dois cantos e FORCE_BL | motor gráfico | Testado em emulador | O recorte contra x = ±w e y = ±w continua com o clipper do GE. |
| [OTD-018](resumos/OTD-018.md) | Profundidade pelo menor z/w e coluna de translação | motor gráfico | Testado em emulador | Overlay ortho nativo precisa de z negativo para ficar atrás do jogo; o recorte no plano perto ainda não existia. |
| [OTD-019](resumos/OTD-019.md) | Recorte de polígono no plano perto | motor gráfico | Testado em emulador | O recorte contra x = ±w e y = ±w não é feito pelo port; fica com o clipper do GE. |
| [OTD-020](resumos/OTD-020.md) | Entrada: teclas do DS e toque viram o contrato BTN_* | jogo | Testado em emulador | O analógico virtual não tem a mesma curva do controle do N64; a sensibilidade precisa ser ajustada com gameplay real. |
| [OTD-021](resumos/OTD-021.md) | fs: assets lidos do cartão SD em streaming | jogo | Testado em emulador | No emulador não existe latência de SD; a vazão de leitura real só pode ser medida em hardware. |
| [OTD-022](resumos/OTD-022.md) | A primeira sala real do jogo na tela de cima | motor gráfico + jogo | Testado em emulador | A sala é flat-shaded nesta entrega; textura e minificação vêm na seguinte, junto com a descoberta de que o problema visual não era paleta. |
| [OTD-023](resumos/OTD-023.md) | Sala texturizada: o confete era minificação, não paleta | motor gráfico + jogo | Testado em emulador | O GE não tem filtro de minificação; mitigar exige cadeia de mip própria, que ainda não existe nesta entrega. |
| [OTD-024](resumos/OTD-024.md) | Câmera presa como régua e seleção de mip por triângulo | motor gráfico + jogo | Testado em emulador | O custo é alto e ainda não estava atribuído; a atribuição errada publicada aqui foi corrigida em OTD-026. Faces mais fundas que 1024 unidades de mundo caem silenciosamente para o nível 0. |
| [OTD-025](resumos/OTD-025.md) | Correção de escala da sonda de mip (fator 4096) | motor gráfico | Testado em emulador | Esta é uma correção dentro de OTD-024 e não conta como avanço independente; compartilha as capturas e não substitui a necessidade de varredura de ângulo. |
| [OTD-026](resumos/OTD-026.md) | O custo do mip era aritmética por face; recíproco por tabela devolve 2,1 ms | motor gráfico | Testado em emulador | Ainda custa +17,5 ms; são cerca de 16 multiplicações de 64 bits por face. A próxima formulação precisa ser validada face a face antes de substituir. Faces mais fundas que 1024 unidades de mundo têm recíproco zero. |
| [OTD-027](resumos/OTD-027.md) | A área em espaço de olho empatou; o ganho estava no tamanho do inteiro | motor gráfico | Testado em emulador | O ganho é 1,2 ms, 7 % dos 17,5 ms da cadeia; o FPS não muda. O limite de 750 px é uma mudança de comportamento, verificada por contador em uma sala e três vistas apenas. A maior parte do custo restante está nas multiplicações de 64 bits da projeção por canto, não na área. |
| [OTD-028](resumos/OTD-028.md) | A sala estava branca porque o port a pintava de branco | motor gráfico | Testado em emulador | O ganho é visual e custa 0,5 ms (1,1 % do render), sem mudar FPS. A conferência é contra o que a lista pede, não contra uma imagem de referência do N64, que esta máquina não tem. Só uma sala foi vista no emulador; as outras 358 foram lidas offline. 20 808 triângulos (14,9 %) pedem iluminação do RSP e continuam brancos de propósito; a família de duas texturas (20 891 triângulos, 137 salas) continua não modelada. |
| [OTD-029](resumos/OTD-029.md) | O lerp de duas texturas virou segunda passada, e o alpha da segunda imagem quase o engoliu | motor gráfico | Testado em emulador | É aproximação de alpha: o RDP usa ENVA/255 e o GE usa POLY_ALPHA/31, com erro de meio nível (~1,6 %). O método depende de o T0 estar no destino; não há leitura de framebuffer. 42,1 % das segundas imagens estão no arquivo da cena, que o build não monta, e degradam para T0 só. 19,3 % não são RGBA16. Uma sala, um ângulo, em emulador; as outras 358 salas foram lidas offline. Correção (OTD-030): o build monta a cena da sala carregada (BMORI1.SCE como segmento 2; a paleta da sala vem de lá, linha 9 do HUD com pS:14288). As 35 faces da Bmori1 têm a segunda imagem no segmento 3; o 42,1 % descreve as salas que o demo não carrega. |
| [OTD-030](resumos/OTD-030.md) | A troca de textura custa 0,45 µs; quem cobra é a aritmética do mip | motor gráfico + jogo | Testado em emulador | Uma sala e um ângulo, em emulador. O preço da troca inclui o corte do lote; o custo por nível inclui a varredura do cache. O rasterizador não entra em s. A cadeia continua compromisso em avaliação: −31 % de grão por +16,4 ms (34 % do render), não fechado. Sem atores, segunda sala, colisão ou hardware. |

## Pendências e regressões conhecidas

| ID | Problema | Situação |
|---|---|---|
| OTD-001 | OTD-001-P1 — DSi mode e boost de clock não puderam ser confirmados sem hardware físico. | Pendente |
| OTD-001 | OTD-001-P2 — Benchmark de SD não executado no emulador por falta de cartão montado. | Pendente |
| OTD-002 | OTD-002-P2 — Build de referência do decomp não validado por exigir o ROM de referência do usuário. | Pendente |
| OTD-013 | OTD-013-P3 — Despejo a meio do frame só pode ser confirmado em DSi real. | Pendente |
| OTD-021 | OTD-021-P1 — Vazão real de SD não é mensurável no emulador. | Pendente |
| OTD-024 | OTD-024-P2 — Custo de +17,5 ms com a cadeia ligada; formulação atual exige ~16 multiplicações de 64 bits por face. | Pendente |
| OTD-024 | OTD-024-P3 — Faces com w > 65536 têm recíproco zero e caem silenciosamente para o nível 0. | Pendente |
| OTD-026 | OTD-026-P2 — Custo residual de +17,5 ms, dominado por ~16 multiplicações de 64 bits por face. | Pendente |
| OTD-026 | OTD-026-P3 — Recíproco 16.16 é zero para w > 65536 (face mais funda que 1024 unidades de mundo); nessas faces o nível cai para 0. | Pendente |
| OTD-026 | OTD-026-P4 — Varredura de ROOM_ANGLE com o instrumento de áudio continua não feita; custo medido em um ângulo só. | Pendente |
| OTD-027 | OTD-027-P2 — O limite de ±750 px por canto é mudança de comportamento verificada por contador em uma sala e três vistas; uma parede que encha a tela de muito perto pode atingi-lo. | Pendente |
| OTD-027 | OTD-027-P3 — A cadeia ainda custa +16,5 ms, dominada pelas multiplicações de 64 bits da projeção por canto; novos cortes exigem mudar a projeção, não a área. | Pendente |
| OTD-028 | OTD-028-P3 — G_LIGHTING segue sem estágio de iluminação: 20 808 triângulos (14,9 %) pedem SHADE calculado pelo RSP e continuam brancos. | Pendente |
| OTD-028 | OTD-028-P4 — A família de duas texturas (TEXEL1-TEXEL0)*ENVA+TEXEL0, 20 891 triângulos em 137 salas, continua não modelada; nesta sala são 35 faces. | Pendente |
| OTD-028 | OTD-028-P5 — A cor de vértice custa 0,5 ms reproduzíveis (46,6 → 47,1) sem que eu tenha identificado a origem, já que os dois lados chamam glColor3b uma vez por vértice. | Pendente |
| OTD-029 | OTD-029-P6 — 42,1 % das segundas imagens da família vivem no segmento 2 (arquivo da cena), que o build não monta; nesses materiais só o T0 é desenhado. Correção (OTD-030): o segmento 2 é montado para a sala carregada; o que falta é carregar a cena de cada sala nova. | Pendente |
| OTD-029 | OTD-029-P7 — 4 063 triângulos (19,3 %) têm segunda imagem CI/IA/I, fora do censo de alpha e do caminho RGBA16. | Pendente |
| OTD-030 | OTD-030-P4 — O build de teto 0 emite -Wtype-limits em mipLimit porque o laço é inalcançável; o build entregue compila sem avisos. | Pendente |
| OTD-030 | OTD-030-P5 — A cadeia de mip segue como compromisso em avaliação: −31 % de grão por +16,4 ms (34 % do render), medido numa sala e num ângulo. | Pendente |

## O que ainda não existe

Uma sala renderizada não é o jogo. Cada sistema abaixo continua com estado próprio, e nenhum deles está coberto por uma entrega registrada:

- atores e objetos de cena em tempo de execução
- colisão e física do jogador
- câmera de jogo (a câmera presa é instrumento de medida, não a câmera do OoT)
- animação e esqueleto
- áudio
- progressão, portas, transições de sala e salvamento
- segunda sala e carregamento de cena completo
- HUD e inventário 2D na tela de baixo
- execução em DSi físico

O estado do motor também é parcial: ciclo 1 do combiner, fontes ENV/PRIMITIVE, `G_TT_IA16`, texturas retangulares, wrap/mask/shift e recorte em `x = ±w` / `y = ±w` não estão cobertos. A lista detalhada fica em [`HISTORICO_MOTOR_GRAFICO.md`](../../HISTORICO_MOTOR_GRAFICO.md), seção "Onde o motor está".
