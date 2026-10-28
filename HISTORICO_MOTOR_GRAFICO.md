# Histórico do Motor Gráfico — Ocarina of Time DSi

Registro por etapa do tradutor de gráficos: o decodificador de display lists
F3DEX2 e a ponte até o geometry engine do DS. A entrada mais recente fica no
topo. Nenhuma etapa é marcada como concluída sem medição no hardware indicado.

**Escopo deste arquivo:** `source/gfx/` — `f3dex2.c` (decodificador),
`ge_draw.c` (tradutor para o GE), `dl_demo.c` (display list de teste) e o
comportamento do geometry engine. Plataforma, memória, streaming, UI das duas
telas, áudio e integração do jogo ficam em `HISTORICO_JOGO.md`; o índice das
duas séries está em `HISTORICO_FASES.md`.

Plano completo: `coral-hill-wren.md`, fora do repositório.

---

## Onde o motor está

| Caminho | Estado |
|---|---|
| Geometria (VTX/TRI1/TRI2, pilha de matrizes, projeção por tipo) | coberta e medida |
| Matriz 16.16 → 1.19.12 sem transposição, `G_MTX_PROJECTION` na pilha certa | coberta e medida |
| Ordem de profundidade do GE — **fica com o menor `clip z/w`**; `glFrustumf32` ≡ `guPerspective`, `glOrthof32` tem o `z` invertido | coberta e medida em quatro pontos; overlay ortho nativo precisa de `z` negativo para ficar atrás do jogo |
| Texturas RGBA16, CI4 e CI8 com paleta por slot de TMEM | coberta e medida |
| Cache de texturas com despejo LRU **pinado por frame**, 64 slots e espera limitada por relógio quando a lista transborda | coberto e medido — reciclar dentro do frame corrompia a geometria já enfileirada; o despejo a meio do frame **não é alcançável** no emulador e custa um frame no hardware |
| Combiner do ciclo 0 (MODULATE / DECAL / SHADE) no estágio fixo do GE | coberto e medido — em MODULATE com `G_LIGHTING` limpo o `SHADE` é a cor do vértice (`cn`), que é o que a lista pede; `l` no HUD conta as faces em que a lista pede luz e o port não a tem |
| Combiner do ciclo 0: lerp de duas texturas `(TEXEL1−TEXEL0)·ENVA+TEXEL0` | **coberto e medido em 560 px** — segunda passada sobre o T0 já desenhado, com `POLY_ALPHA(ENVA)`; +1,3 ms na Bmori1 (+2,7 % do render) para 35 faces. 21 041 triângulos no jogo, 42 % deles com a segunda imagem no segmento 2 (arquivo da cena), que este build ainda não monta e por isso degrada para T0 só. **Correção OTD-030:** o build monta a cena da sala carregada (`BMORI1.SCE`, paleta `pS:14288` na linha 9); o 42 % é das outras salas, que o demo não carrega |
| `G_LIGHTING` (luz do RSP: normais, `G_SETLIGHT`/`G_DIRLIGHT`) | **não coberto** — o port lê o bit de geometria para não usar o `cn` como cor, mas não calcula iluminação; 20,8 mil triângulos (14,9 %) pedem `SHADE` iluminado |
| Viewport N64 → painel do DS: **dois cantos inclusivos, y contando de baixo** | coberto e medido — o formato anterior (deslocamento + tamanho, y do topo) desenhava de cabeça para baixo e sumia com a metade das listas |
| Alpha do texel como recorte (`G_ACMUX_TEXEL0` → `GL_ALPHA_TEST`), em RGBA5551 | coberto e medido |
| Alpha de paleta CI no geometry engine | **não existe no hardware** — contorno medido em pixels: bake A1B5G5R5 (CI4) e flag `GL_TEXTURE_COLOR0_TRANSPARENT` (CI8) |
| OtherMode (`G_SETOTHERMODE_H/L`) com sombra de flags — **opaco só quando `ZMODE_OPA` e, com `FORCE_BL`, a cor de blend do ciclo 1 ≠ `CLR_MEM`** | coberto e medido |
| Blending translúcido por polígono (`POLY_ALPHA` + `POLY_ID` + lote reaberto) | **coberto e medido em pixels**: α16/31 → 53 % do fonte, α8/31 → 28 % |
| Ciclo 1, fontes ENV/PRIMITIVE, recorte via paleta | **não coberto** |
| IA4/IA8, texturas retangulares, wrap/mask/shift, `G_LOADTILE` com origem ≠ 0 | **não coberto** |
| Recorte de polígono contra o plano perto, em espaço de olho (equivalente ao RSP) | **coberto e medido** — modelview aplicada em software, corte Sutherland–Hodgman em `z = nearZ`; sem ele, um vértice 20000 unidades atrás da câmera é clampado pela entrada `v16` e o motor corta no plano errado (14 colunas de painel) |
| Recorte contra `x = ±w` e `y = ±w` | **não coberto pelo port** — tratado pelo clipper do próprio GE, que mede `posição` contra `w` e por isso inverte os seis planos quando `w` é negativo |
| Display list real do jogo (asset extraído do ROM) | **coberto e medido** — `market_day_room_0` cru do baserom ntsc-1.2, 510 triângulos, `DL t510 c0 k0 x0 ok`; o caminho big-endian do decodificador (`f3SetSegmentBe`) existe por causa disso |
| Texturas de sala real | **coberto e medido** — `Bmori1_room_0`: 646 triângulos, 75 `G_SETTIMG`, 8 `G_LOADTLUT` em 2 paletas da cena, `Tex b0 h65 x0 e0 t646/532 on`; parede, madeira e painel reconhecíveis no quadro |
| Alpha de paleta CI só quando o OtherMode pede (`G_TT_RGBA16`/`IA16`) | **coberto e medido em contagem** — com `G_TT_NONE` o bit 15 da paleta é lixo e o bake o força a 1; 25 % a 81 % dos texéis das imagens desta sala cairiam em entrada de paleta alpha-0 contra o `GL_ALPHA_TEST` global |
| Escala de texel do vértice (N64 10.5 → GE 12.4) | **coberto e medido** — `GE_TEX_SHIFT 1`; `texMaxS/texMaxT` = 448 × 347 texéis de span de UV numa parede de ~150 px |
| Filtro de textura (mip / LOD / antialias de minificação) | **não existe no hardware** — o GE amostra um texel por pixel. Mitigação **construída e medida**: cadeia box-filter no bake (nível = lado/2ⁿ, piso 8×8) + nível escolhido por triângulo pela razão entre área de textura e área de tela. No ângulo preso a sala **minifica muito** (`Lo 2/25/256/363`, pior face `R ≈ 79` texéis/px) e a cadeia tira o grão do painel de **13,28 para 9,09** (−32 %) por **+16,5 ms** (FPS 20 → 19; era +19,6 antes da tabela de recíprocos e +17,5 antes da área em 32 bits). Teto de profundidade **2**, medido: o nível 3 compra 0,03 de grão e custa 4 FPS. O custo é **aritmética por face**, não troca de textura: o build que calcula o nível e o descarta custa o mesmo que a cadeia inteira. Re-medido em 2026-10-27 com sombreamento e lerp no build: grão **4,22 → 2,91 (−31 %)** por **+16,4 ms** (34 % do render), dos quais **15,7 ms são o seletor**, 0,7 ms nível/cache e **0,45 µs por troca de textura** (662 trocas forçadas, painel idêntico). O ganho continua **compromisso em avaliação** (OTD-030) |
| Régua de medida do painel | **coberta e medida** — a câmera do demo girava sozinha e o quadro capturado dependia do FPS, então as três medidas comparadas eram três vistas diferentes; `ROOM_SPIN`/`ROOM_ANGLE` em `source/main.c` prendem a câmera, e duas capturas do mesmo ângulo dão md5 igual |
| `G_TT_IA16` (paleta em intensidade+alpha) | **não coberto** — ainda lida como RGBA16 |

---

## Fase 12 — a troca de textura custa 0,45 µs; quem cobra é a aritmética do mip (2026-10-27)

> **ID** `OTD-030` · **Data** 2026-10-27 · **Frente** motor+jogo · **Fase** 12 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-030/` · **Commit** árvore sem histórico git

**Objetivo:** antes de ampliar efeitos, separar três custos que a fase 8 tinha
junto num número só — a troca de textura, o upload de nível e a aritmética do
seletor de mip — com a mesma câmera e os mesmos pixels dos dois lados. Ao mesmo
tempo, dizer o que cada contador da telemetria mede de verdade, sem forçar FPS,
simulação e tempos a coincidir.

### Entregue

- **`GE_TEX_REBIND_PROBE`**, sonda de medição, 0 no build entregue. Ela tem
  dois pontos: `bindTex` deixa de devolver cedo quando o id já ligado é o
  mesmo, e `putFan` refaz a ligação uma vez por triângulo **com o id já
  ligado** (`d->boundId`). Religar a imagem-base em vez disso derruba o
  triângulo ao nível 0 de mip e muda o painel — foi o primeiro erro desta fase.
- **Build entregue sem mudança de comportamento:** `GE_TEX_MIP_MAX 2`,
  `GE_TEX_MIP_NULL 0`, `GE_TEX_REBIND_PROBE 0`. O painel bate com o da fase 11
  (`fa1c5bc0…`) e `s48,8` em duas capturas depois de restaurar.
- **`evidencias/OTD-030/`** com seis capturas (`.png`, `_hud`, `_top`) e
  `medicoes.txt`.

### Descobertas medidas

- **A/B da cadeia de mip, mesma linhagem, mesma câmera, duas capturas por
  lado.** `MAX 0`: grão **4,22**, `s32,4`, `f33,4`, FPS 20, `r136`, painel
  `92d399e9…`. `MAX 2`: grão **2,91**, `s48,8`, `f50,1`, FPS 19, `r247`,
  painel `fa1c5bc0…`. A cadeia compra **−31 % de grão por +16,4 ms**
  (34 % do render). É o mesmo compromisso da fase 7, agora medido com o
  sombreamento por vértice e o lerp no build.
- **O custo da cadeia é 96 % aritmética.** O build que calcula o seletor e
  descarta o nível (`GE_TEX_MIP_NULL`) custa `s48,1` (grão 4,22, painel
  `92d399e9…`): **+15,7 ms** contra o `MAX 0`. O que sobra — escolher o nível,
  buscar no cache e religar — vale **0,7 ms**.
- **Uma troca de textura custa 0,45 µs.** Forçar 662 trocas a mais (uma por
  triângulo, religando o id corrente) move `s` de 48,8 para **49,1** e `r` de
  247 para **909**, com o painel **byte a byte igual**. O preço inclui fechar e
  reabrir o lote, então é limite superior.
- **Não há upload por quadro nesta cena.** `Tex b0` nos quatro builds: depois
  do aquecimento o cache acerta tudo (`h` 101 → 207). O número que a cadeia
  move são acertos de cache, não transferências para a VRAM.
- **Rectificação da fase 8.** Ficou publicado que "a otimização que paga é
  agrupar primitivos por nível e cortar os re-binds". Com o preço medido, 111
  trocas de nível valem no máximo **0,7 ms** — agrupar por nível não paga o que
  a fase 8 sugeriu. O custo está na aritmética por face, como a fase 9 já
  dizia.
- **O compromisso continua em avaliação.** O build entregue mantém a cadeia
  ligada porque 20 → 19 FPS é um quadro e o grão cai 31 %, mas **34 % do render
  para um ganho que só foi medido numa sala e num ângulo** não é conclusão
  fechada; o build `MAX 0` fica como alternativa declarada, não como código
  morto.

### O que cada contador mede

| Mostrador | Mede | Não mede |
|---|---|---|
| `FPS` | quadros terminados dentro da última janela de 1 s do relógio de barramento (`perf.c`: `framesInSec`, zerado quando `now − secondStart ≥ BUS_CLOCK`) | não é média histórica; é inteiro, atualizado no ecrã a 4 Hz, e não passa de 60 porque cada iteração termina em `swiWaitForVBlank()` |
| `f` | intervalo entre duas chamadas a `perfEnd`, ou seja a **última** iteração inteira: entrada, render, texto do HUD, lógica e espera de VBlank | não é média, não é tempo de GPU e não é o tempo de um quadro "do jogo" |
| `s` | de `renderStart` até depois de `glFlush(0)`: montar a cena, decodificar a display list, atualizar o cache de textura e enfileirar os comandos do GE | **não inclui rasterização** nem a espera de VBlank; é trabalho de CPU |
| `w` | maior `f` desde o boot (marca d'água, nunca zerada) | não é o custo atual: 349 ms inclui os quadros de carregamento da sala |
| `Logic 20 Hz` | `sched.tickRate`: ticks de lógica por segundo, derivados do contador de IRQ de VBlank (`schedFrame`: `elapsed/3`) | não é taxa de desenho: um quadro pode devolver 0, 1 ou mais ticks, e a lógica continua a 20 Hz com o render a 19 FPS |
| `ticks` | total acumulado de ticks de lógica desde o boot | não reinicia a cada segundo, ao contrário de `FPS` |
| `Tex b/h/x/e` | uploads, acertos, recusas e despejos do cache de textura no quadro | `b0` não quer dizer "sem textura": quer dizer que tudo acertou o cache |
| `r` | trocas de textura (mudanças de id ligado), incluindo as do lerp | não conta uploads nem custo de rasterização |
| `Bk d/m` | grão do quadrado de textura que subiu no bake e níveis de mip usados (máx/médio/triângulos) | não é o grão do painel, que é medido offline |

Os números **não coincidem por construção**: `FPS 19` com `f50,1` é coerente
(1000/50,1 = 19,96, truncado para 19 na janela), `s48,8` deixa 1,3 ms de loop
fora do render (entrada, texto, lógica e o resto da espera de VBlank), e `w349`
é um recorde de carregamento, não um custo de repouso. Ajustar qualquer um para
"bater" com outro apagaria justamente a informação que ele carrega.

### Validação

melonDS 1.1, modo DS, 67 MHz, `Bmori1_room_0` (646 triângulos), `ROOM_ANGLE 0`,
`ROOM_SPIN 0`, `GE_LERP 2`, `GE_VTX_SHADE 1`, `GE_MIP_EYE 0`,
`GE_MIP_AREA32 1`, `GE_TEX_LOD_BIAS 2048`, estado estacionário. Capturas
`f12a1`/`f12a3` (entregue), `f12b1`/`f12b2` (`MAX 0`), `f12e1`/`f12e2` (sonda de
troca), `f12f1`/`f12f2` (`MIP_NULL`), cada par com md5 idêntico. A sonda errada
fica em `f12d1`. Builds A, C e D compilam sem avisos; B emite um
`-Wtype-limits` porque o laço de `mipLimit` é inalcançável com teto 0. Nada em
DSi físico.

### Erros meus

1. **A primeira sonda de troca só tirou o retorno cedo do `bindTex`.** Nesta
   cena quase toda ligação já é um id diferente, então ela forçou apenas **16**
   trocas (`r247 → r263`), `s48,3` contra 48,8 e nenhuma conclusão. O preço de
   uma troca só aparece forçando uma por triângulo.
2. **A sonda por triângulo religava a imagem-base, não o id corrente.** Isso
   devolveu cada triângulo ao nível 0 enquanto `texMip` continuava no nível
   escolhido; o painel mudou **60 576 px** (caixa x 124..389, y 62..383). O md5
   foi o que pegou, e a captura errada fica no registro.
3. **Comparei grão absoluto com o da fase 7.** 9,09 era o painel pintado de
   branco; o build atual, com cor de vértice, mede 2,91 no mesmo recorte. O
   número comparável entre fases é a razão do A/B (−31 %), não o absoluto.
4. **O build de teto 0 não compila limpo.** Um aviso `-Wtype-limits` no laço de
   `mipLimit`; o build entregue não tem aviso, mas a sonda deveria ter sido
   guardada por `#if` em vez de deixar o compilador avisar.

### Limites

- Um ângulo e uma sala; o preço da troca é limite superior (inclui o corte do
  lote) e o custo por nível inclui a varredura do cache.
- O rasterizador não entra em `s`; o custo de preencher pixel e o
  comportamento de DMA/VRAM do DS real continuam fora.
- A cadeia de mip continua sendo **compromisso em avaliação**: −31 % de grão
  por +16,4 ms, medida numa vista só.
- Sem atores, sem segunda sala, sem colisão; nada em hardware.

### Próximo

- A fatia jogável pedida pelo usuário: uma sala, Link controlável, câmera e
  colisão básica, com critérios próprios; depois a transição para uma segunda
  sala. As dependências que faltam (objeto do Link, animação, colisão da cena)
  serão registradas antes de qualquer substituto.

## Fase 11 — o lerp de duas texturas virou segunda passada, e o alpha da segunda imagem quase o engoliu (2026-10-27)

> **ID** `OTD-029` · **Data** 2026-10-27 · **Frente** motor · **Fase** 11 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-029/` · **Commit** árvore sem histórico git

**Objetivo:** a fase 10 fechou com a maior lacuna medida e uma direção: a
família `(TEXEL1−TEXEL0)·ENVA+TEXEL0`, 20 891 triângulos em 137 salas, devia
virar duas passadas porque o blend do GE lê o destino. A tarefa era construir
isso, provar que desenha o que a lista manda, medir o preço e dizer quantos
triângulos do jogo ela realmente alcança — sem presumir que a família inteira
está no arquivo da sala.

### Entregue

- **`F3_CC_LERP` no decodificador.** Modo próprio para o ciclo 0
  `a0=2, b0=1, c0=12, d0=1`, com `cc.env`/`cc.envValid` vindos do
  `G_SETENVCOLOR` e a letra `L` no HUD. O `o` da linha 20 deixa de engolir a
  família: na `Bmori1_room_0` ele cai de 35 para 0 e os 35 triângulos passam a
  ser contados como lerp.
- **`lerpPass`, a segunda passada.** O T0 já foi desenhado como MODULATE; o
  segundo passo liga T1 e redesenha o mesmo triângulo com
  `POLY_ALPHA(ENVA) | POLY_CULL_NONE | POLY_ID(...)`, lote reaberto. O GE
  calcula `src·ENVA + dst·(1−ENVA)`, e com T0 no destino isso é a aritmética do
  RDP.
- **`GE_LERP` 0/1/2** como interruptor de A/B na mesma linhagem de build: 0 é o
  T0 só (o build anterior), 1 é T1 só e opaco (sonda de diagnóstico, não é
  caminho de entrega), 2 são as duas passadas.
- **ENVA → `POLY_ALPHA` arredondado**, `(env·31 + 127)/255`, para o erro ficar
  em meio nível de 31 (1,6 %) em vez de um nível inteiro por truncamento.
- **`GE_LERP_ENV 128` como constante declarada.** Nenhuma sala escreve
  `G_SETENVCOLOR`: o OoT o define uma vez por cena, em
  `Scene_DrawConfigForestTemple()` (`src/code/z_scene_table.c`), que este build
  não executa. `lerpEnvDefault` conta os triângulos que caíram no valor
  declarado para que a escolha fique visível no HUD.
- **A segunda imagem assada opaca.** O mux de alpha do lerp é `(0−0)·0+1`, então
  o RDP escreve alpha opaco qualquer que seja o alpha do TEXEL1. O DS tira o
  alpha do pixel do texel e o `topInit()` roda o recorte no limiar mais baixo do
  quadro inteiro; um `F3Tex.opaque` força o bit 15 no bake da segunda imagem.
  Sem isso a segunda passada era descartada em silêncio (medido: 0 de 1024
  texéis de T1 com alpha contra 35 de 1024 de T0 na mesma entrada).
- **`loadWords` copiado no `G_SETTILESIZE` do tile 1.** O bake recusa a imagem
  quando `loadWords != w·h` (razão 5), e o contador de palavras do `G_LOADBLOCK`
  só era guardado no tile 0. A ordem real da lista (`SETTIMG → SETTILE(7) →
  LOADBLOCK(1024) → SETTILE(1) → SETTILESIZE(1) → SETCOMBINE`) garante que o
  dado está lá quando o `SETTILESIZE` chega.
- **Ferramentas arquivadas:** `ferramentas/analisar_lerp_alpha.py` (censo do
  alpha da segunda imagem, seguindo `G_DL`), `ferramentas/dump_entrada_sala.py`
  (ordem dos comandos de uma entrada), `ferramentas/comparar_paineis.py` (md5 +
  pixels diferentes + caixa) e a opção `--lerp`/`--luz` do
  `analisar_combiner.py`.

### Descobertas medidas

- **A entrada 2 da sala pede exatamente isso, e não pede ENV.** 35 triângulos,
  `(TEXEL1−TEXEL0)*ENVA+TEXEL0  alpha (0-0)*0+1`, `geomode 0x00010400` (com
  fog, sem luz), dois binds de textura por triângulo (`texOn=70/35`), **nenhum
  `G_SETENVCOLOR`** na lista; o PRIM é branco em 31 dos 35 e `0xF5F4F1FF` nos
  outros 4, que este estágio não usa porque o lerp não multiplica por PRIM.
- **O A/B muda só a laje.** `GE_LERP 0` dá o painel md5
  `8b5420d1…`, idêntico byte a byte ao painel arquivado da fase 10; com
  `GE_LERP 2` o md5 é `fa1c5bc0…` e a diferença é de **560 pixels numa caixa
  x 232..283, y 362..383** — exatamente a laje de chão de 35 faces, em duas
  capturas de cada lado. A luminância média do painel vai de 13,6 para 13,8 e a
  fração de pixels de dominância vermelha de 8,5 % para 8,6 %: o efeito é
  localizado, não muda o quadro.
- **Os contadores confirmam a contagem.** `Bl 20/646 → 55/646`: o `xluTris`
  conta as 20 faces translúcidas da sala mais as 35 segundas passadas, uma por
  triângulo da família; `r152 → r247` são os rebinds de textura por quadro
  (mais 95, ~2,7 por triângulo do lerp); `s611 l0 o0` no lugar de
  `s611 l0 o35`.
- **Custa 1,3 ms, e o teste é intercalado.** `s47,5` com `GE_LERP 0` em duas
  capturas (`f11k`, `f11m`) e `s48,8` com `GE_LERP 2` em duas (`f11j`, `f11n`);
  `FPS 19 f50,1 w349` nos quatro. O delta é +1,3 ms (2,7 % do render), não o
  "dentro do ruído" que eu tinha registrado antes de intercalar o A/B.
- **O censo do alpha, agora sobre a família inteira.** 21 041 triângulos com o
  ciclo 0 do lerp: **14 588 (69,3 %)** têm ao menos um texel com alpha;
  **2 390 (11,4 %)** amostram uma segunda imagem RGBA16 com o bit de alpha
  limpo em todo texel e perderiam a passada inteira sem o bake opaco;
  **4 063 (19,3 %)** não são RGBA16 (CI/IA/I, outra pergunta). Por segmento da
  imagem: **8 858 (42,1 %)** no segmento 2 (arquivo da cena) e 8 120 (38,6 %)
  no segmento 3 (a própria sala) — o número do segmento 2 é a limitação nova.
- **A família é 82,2 % sem luz.** Dos 20 214 triângulos da forma base
  `d0=1`, 16 608 (82,2 %) estão em grupos com `G_LIGHTING` limpo e 3 606 em
  salas que pedem luz; a variante de alpha soma 827 em 23 salas, e o ciclo 0
  inteiro fecha em 21 041 triângulos e 151 salas na forma base.

### Validação

melonDS 1.1, modo DS, 67 MHz, `Bmori1_room_0`, 646 triângulos, `ROOM_ANGLE 0`,
`ROOM_SPIN 0`, `GE_TEX_MIP_MAX 2`, `GE_TEX_LOD_BIAS 2048`, `GE_MIP_EYE 0`,
câmera presa, estado estacionário. Mesma linhagem de build, só o `GE_LERP`
muda: `f11k` e `f11m` (0) painel `8b5420d1…`, `s47,5`, `Bl 20/646 r152`;
`f11j` e `f11n` (2) painel `fa1c5bc0…`, `s48,8`, `Bl 55/646 r247`; diffs
`560 px` em `x 232..283, y 362..383` nos dois pares. As sondas de diagnóstico
ficam no registro: `f11f` (bind de textura branca no lugar de T1) muda os
mesmos 560 px; `f11g` (lerp sem o bake opaco) muda **0 px**; `f11h` (T1 só,
opaco) muda os mesmos 560 px. Censos e ordem dos comandos:
`censo_lerp.txt`,
`censo_alfa.txt`,
`sala_bmori1_lerp.txt`,
`entrada2_comandos.txt`,
`painel.txt`. Compila sem avisos.

### Erros meus

1. **O segundo tile nunca era assado, e em silêncio.** A primeira build com o
   decodificador do lerp desenhava 0 px de diferença. O `G_SETTILESIZE` do tile
   1 gravava a extensão mas não `loadWords`, o bake recusava a imagem pela
   razão 5 e a passada caía sem tocar no painel. O traço da entrada
   (`LOADBLOCK` antes de `SETTILESIZE`) mostrou que o dado existia; faltava
   copiá-lo do slot do bloco para o slot do tile.
2. **A hipótese de "mesma profundidade é rejeitada" estava errada.** Como a
   segunda passada redesenha o mesmo triângulo, supus que o GE recusaria o
   segundo desenho por `clip z/w` igual e adicionei um viés de −8 no `z`. O viés
   não mudou nada, e depois do conserto do alpha o lerp desenhou com viés
   **zero**: profundidade igual passa. O viés foi removido em vez de mantido.
3. **A causa real era o alpha do texel.** Diagnóstico em cadeia: (a) um bind de
   textura branca nos mesmos cantos mudou 560 px, então a rasterização
   funcionava; (b) os ids das duas texturas eram distintos (`a6 b60`); (c) os
   bakes em Python diferiam em todos os níveis; (d) a contagem de bit 15 deu
   **T0 35/1024 e T1 0/1024**. O `GL_ALPHA_TEST` global descartava tudo. O
   conserto é o `opaque` no bake, não uma mudança no blend.
4. **Uma captura inválida foi lida como resultado.** A captura `f11e` saiu
   160×28, com o painel todo branco: era um grab de janela ruim, não a tela. Eu
   cheguei a tratá-la como "a tela ficou branca" por um instante antes de
   conferir as dimensões. Refiz como `f11e2` e passei a checar o tamanho de toda
   captura antes de confiar no md5; a captura inválida fica registrada em
   `f11e_grab_invalido.png`.
5. **O primeiro censo do alpha não seguia `G_DL`.** Ele andava só a lista linear
   de cada entrada e via **12 221 de 21 041** triângulos da família (58 %),
   reportando 2 294 alpha-0 e 6 322 imagens no segmento 2. Seguindo os ramos de
   segmento 3, o total sobe para os 21 041 e os números viram 2 390 e 8 858. Os
   valores antigos ficam aqui como correção; a ferramenta arquivada é a que
   segue `G_DL`.
6. **O custo que eu tinha registrado estava subestimado.** Eu havia anotado
   "+1,3 ms dentro do ruído" com uma captura por lado. Intercalando duas
   capturas de cada lado na mesma sessão, 47,5 contra 48,8 se repete nos dois
   pares: é custo, não ruído.

### Limites

- É uma **aproximação de alpha**: o RDP usa ENVA/255 (8 bits) e o GE usa
  `POLY_ALPHA`/31 (5 bits). O arredondamento limita o erro a meio nível
  (~1,6 %), mas os dois não são iguais.
- O método depende de o T0 estar no destino: não há leitura de framebuffer como
  fonte. Se o T0 exigisse ordenação de profundidade ou fosse translúcido, a
  segunda passada leria o que estivesse ali.
- **42,1 % das segundas imagens estão no segmento 2**, o arquivo da cena, que
  este build não monta: nelas o material degrada para T0 só, sem erro visível.
  Montar a cena é o próximo passo óbvio da família.
  **Correção (fase 12, OTD-030):** o build **monta** a cena da sala carregada —
  `roomLoad` lê `BMORI1.SCE`, registra como segmento 2 e a paleta da sala vem de
  lá (linha 9 do HUD: `pS:14288`). As 35 faces da `Bmori1_room_0` têm a segunda
  imagem no **segmento 3**, como o censo da sala mostra (`seg3_alfa0: 35`). O
  `seg2_` descreve as outras salas, que o demo não carrega: o limite é carregar a
  cena de cada sala, não resolver o segmento.
- 4 063 triângulos (19,3 %) têm segunda imagem que não é RGBA16 (CI/IA/I) e
  ficam fora deste censo.
- O bake opaco muda o alpha da segunda imagem por desenho: é o certo para este
  combiner (o alpha de saída vem do mux), mas **não** seria certo generalizar
  esse flag para uma segunda imagem usada como recorte.
- Uma sala, um ângulo, em emulador. As outras 358 salas foram lidas, não
  desenhadas; a comparação com imagem de referência do N64 continua em aberto,
  e o portão de DSi físico não foi tocado.

### Próximo

Medir, com builds intercalados, **quanto custam as trocas de textura e a cadeia
de mip** — a fase 10 e a 11 empurraram os rebinds por quadro de 152 para 247, e
o número de mip que existe hoje (+16,5 ms) foi medido antes deste trabalho. A
medida tem de separar o custo de rebind do custo de aritmética e registrar
qualidade visual e desempenho juntos, para que uma melhora cara fique
identificada como compromisso em avaliação. Depois: montar o arquivo da cena
(segmento 2) para a família inteira, `G_TT_IA16`, a segunda sala com os ids de
objeto montados e a varredura de `ROOM_ANGLE`.

---

## Fase 10 — a sala estava branca porque o port a pintava de branco (2026-10-27)

> **ID** `OTD-028` · **Data** 2026-10-27 · **Frente** motor · **Fase** 10 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-028/` · **Commit** árvore sem histórico git

**Objetivo:** a fase 9 fechou dizendo que a próxima pergunta era se a paleta da
`Bmori1_room_0` bate com a referência do N64. Não há emulador de N64 nesta
máquina, então a pergunta virou outra, que dá para responder só com os dados: o
que as 646 faces da sala **pedem** ao combiner, e o que o port **faz** com esse
pedido? A hipótese a testar era que o branco do painel não é falta de textura nem
de luz — é o port desenhando branco por cima do que a lista manda.

### Entregue

- **Censo de combiners do jogo inteiro, fora do emulador**
  (`ferramentas/analisar_combiner.py`). Percorre o `RoomShape` de cada sala,
  segue `G_DL` dentro do segmento 3, atribui cada `G_TRI1`/`G_TRI2` ao
  `G_SETCOMBINE` em vigor e classifica o ciclo 0 com a mesma regra do
  `decodeCC` em C. Modos: uma sala com detalhe (`--prim` mostra PRIM/ENV/texOn
  por grupo), ou `--todas` com `--luz` para o agregado.
- **Censo de cores de vértice** (`ferramentas/analisar_vertices.py`). Mantém o
  buffer de 80 slots do decodificador, guarda **todo** vértice carregado por
  `G_VTX` do segmento 3 (não só o que sobrou no buffer no fim) e conta canal
  `cn`, branco puro e `G_MOVEWORD`/`G_MW_LIGHTCOL`.
- **Métrica de painel arquivada** (`ferramentas/medir_painel.py`). Luminância
  Rec.601 com duas leituras explícitas: sobre todos os pixels (preto = 0) e só
  sobre a geometria (`L ≥ 8`). Existe porque o número que eu tinha medido antes
  vinha de um script de sessão que não ficou; sem a definição no repositório ele
  não seria reproduzível.
- **`onGeom` no decodificador.** `F3_GEOM_*` e `F3GeomHook` em `f3dex2.h`; o
  `G_SETGEOMETRYMODE` do F3DEX2 carrega um set e um clear na mesma palavra, e o
  hook entrega o estado já resolvido. `GeDraw` guarda `geomMode` e conta
  `shadeTris`/`litTris`/`otherTris`, que a linha 20 do HUD mostra como
  `s`/`l`/`o` (os números de viewport saíram, eram constantes).
- **A cor do vértice onde a lista a pede.** `putEye` pinta com `v->r/g/b` os
  triângulos texturados MODULATE com `G_LIGHTING` limpo; DECAL, combiner não
  modelado e superfície iluminada continuam brancos. `GE_VTX_SHADE` em 1 é o
  build entregue; em 0 reproduz o painel anterior sem mexer em mais nada.

### Descobertas medidas

- **A sala pede cor de vértice, e o port desenhava branco.** `Bmori1_room_0`:
  646 triângulos, **611 MODULATE** (`(TEXEL0-K5)*SHADE+0`) e **35 OTHER**
  (`(TEXEL1-TEXEL0)*ENVA+TEXEL0`); `geomode` `0x00010400` (FOG|CULL_BACK, sem
  luz). Em 644 dos 646 triângulos o PRIM é `0xFFFFFFFF`, então multiplicar por
  PRIM nesta sala não mudaria nada.
- **O `cn` desta sala é cor, não normal.** 979 vértices carregados, **média
  (96, 81, 59)**, luminância média **82,7 de 255 (32,4 %)**, **0** com azul
  acima do vermelho, 5 branco puro; as mais comuns são (132, 88, 43),
  (52, 49, 38) e (100, 89, 67). E a lista **não emite um único
  `G_MOVEWORD`**, logo não há `G_SETLIGHT`/`G_DIRLIGHT`: neste espaço o único
  `SHADE` possível é o `cn`.
- **O A/B é de um build só.** Com `GE_VTX_SHADE 0`, painel md5
  `05992cb58c8229ba3b60a28f949e2e93`, L 47,5, 33,4 % dos pixels acima de 20;
  com 1, md5 `8b5420d114b4746703b78e3ad4828f2a`, L **13,6** (−71 %), 28,2 %
  acima de 20, e a fração de pixels de dominância vermelha sobe de 1,1 % para
  8,5 % — o cinza claro vira pedra quente com gradiente vertical. Contadores
  idênticos nos dois (`s611 l0 o35`), e o censo offline previa exatamente
  611/0/35: o instrumento e o dado concordam.
- **Custa 0,5 ms.** `s46,6` com branco e `s47,1` com a cor do vértice, duas
  capturas de cada, `FPS 19` nos dois, `f50,1 w333`. As duas builds chamam
  `glColor3b` uma vez por vértice; não identifiquei de onde vem o 1,1 %, e não
  vou inventar uma explicação.
- **O jogo inteiro**: 359 salas, **140 000 triângulos**, 82,8 % MODULATE e
  17,2 % OTHER. Por par de textura: T0 em 83,1 %, **T0+T1 em 15,0 %**, nenhuma
  textura em 1,9 %. Por luz (por grupo de combiner): MODULATE sem luz 71,7 %,
  OTHER sem luz 13,4 %, MODULATE com luz 11,1 %, OTHER com luz 3,8 %.
- **A próxima lacuna está medida, e não é o PRIM.** Das assinaturas OTHER, a
  família `(TEXEL1-TEXEL0)*ENVA+TEXEL0` soma **20 891 triângulos (14,9 %)**, em
  137 salas na forma base; `(SHADE-K5)*PRIM+0` soma 2 456 (1,8 %) e
  `(TEXEL0-K5)*PRIM+0` 374 (0,3 %). O PRIM vale **2,0 %** dos triângulos, e nas
  salas que mais o usam — as três `market_*`, 510 triângulos cada, 100 %
  `(SHADE-K5)*PRIM+0` — o `geomode` é `0x00030400`, **com `G_LIGHTING`**: o
  `SHADE` ali é resultado de iluminação, que o port não calcula. Implementar
  PRIM sem luz seria multiplicar um `SHADE` que já está errado. **Medido e
  adiado**, não esquecido.

### Validação

melonDS 1.1, modo DS, 67 MHz, `Bmori1_room_0`, 646 triângulos, `ROOM_ANGLE 0`,
`ROOM_SPIN 0`, `GE_TEX_MIP_MAX 2`, `GE_TEX_LOD_BIAS 2048`, câmera presa, estado
estacionário. Mesma linhagem de build, só o `GE_VTX_SHADE` muda: `f10c` e
`f10c2` (0) painel `05992cb5…`, `s46,6` nas duas; `f10d` e `f10d2` (1) painel
`8b5420d1…`, `s47,1` nas duas; `f10e` é o build entregue, `s47,1`, `FPS 19`,
`f50,1 w333`, `Bk d4,4 m644/2/0 r152 e0`, mesmo md5 e mesmos contadores. As
capturas antigas `f10a`/`f10b` batem byte a byte com `f10c`/`f10d`. Censo e
sala: `censo_combiners.txt`,
`censo_luz.txt`,
`sala_bmori1_combiner.txt`,
`sala_bmori1_vertices.txt`,
`sala_market_day_combiner.txt`,
`painel.txt`. Compila sem avisos.

### Erros meus

1. **O comentário que justificava o código era falso.** O `ge_draw.c` afirmava
   que "605 de 979 vértices têm azul acima do vermelho, as mais comuns (4,0,52)
   e (0,0,69)" e que o `cn` "não é uma cor". Medido com o censo: **0 de 979**
   têm azul acima do vermelho, a média é (96, 81, 59) e as mais comuns são
   (132, 88, 43) e (52, 49, 38) — e a sala não emite `G_MOVEWORD`, então
   também não há luz para o `cn` ser material difuso. O comentário foi trocado
   pelo que a medição diz; a conclusão anterior estava errada e fica registrada.
2. **O censo lia as entradas cullable no offset errado.** `RoomShape` tipo 2
   tem a esfera de cull nos primeiros 8 bytes; `opa` está em 0x08 e `xlu` em
   0x0C, não em 0 e 4. A primeira execução reportou 0 triângulos em toda sala
   do tipo 2 — e teria "provado" que a categoria não existe.
3. **O script rápido do `market_day_room_0` começou a andar na lista errada** e
   imprimiu "12 vértices" para 510 triângulos e um PRIM que não era o da sala.
   Não usei o número: refiz com o analisador arquivado, que dá 510 triângulos,
   100 % `(SHADE-K5)*PRIM+0`, `geomode 0x00030400` e PRIMs `0x00A100FF` (272),
   `0xFFFF7FFF` (136), `0xFF0000FF` (80) e `0x7F00FFFF` (16).
4. **O censo de vértices nasceu com três opcodes errados** (`G_VTX` como 0x04,
   `G_MOVEWORD` como 0xBC e o índice do moveword em bits 8..15) e reportou 0
   vértices referenciados em todas as entradas. Quem pegou foi a comparação com
   os constantes do próprio decodificador (`f3dex2.h`), não o olho: os valores
   certos são 0x01, 0xDB e bits 16..23.
5. **O número de luminância que eu já tinha citado não tinha definição no
   repositório.** Ele era a média sobre **todos** os pixels, com o preto
   contando zero — 47,5 e 13,6. Reescrevi a métrica em `medir_painel.py` com as
   duas leituras e as duas dão os mesmos números de antes, agora reproduzíveis.

### Limites

A correção é de **cor**, não de geometria nem de custo: o ganho é visual e o
preço é 0,5 ms, com FPS inalterado. A verificação de "certo" é contra o que a
própria lista pede, não contra uma imagem de referência do N64 — esta máquina
não tem emulador de N64, e a comparação com referência continua em aberto desde
a fase 6. Só uma sala foi vista no emulador; as outras 358 foram lidas offline,
não desenhadas. `G_LIGHTING` continua sem estágio de iluminação: 20 808
triângulos (14,9 %) pedem `SHADE` que o port não sabe calcular, e neles o
`GE_VTX_SHADE` **não** age — seguem brancos de propósito. A família de duas
texturas (20 891 triângulos, 14,9 %, 137 salas) continua não modelada; nesta
sala ela são 35 triângulos.

### Próximo

Agora que a lacuna está classificada por tamanho, a próxima é a que o censo
aponta como maior: **`(TEXEL1-TEXEL0)*ENVA+TEXEL0`**, o lerp entre duas texturas
por `ENVA`. O GE do DS não lê o framebuffer como fonte do combiner, mas o blend
lê o destino: uma segunda passada com `SRC = T1·ENVA` e `DST = fb·(1−ENVA)` é
exatamente o lerp, ao preço de desenhar esses triângulos duas vezes. Antes de
construir, medir quantos dos 20 891 estão em salas que também precisam de luz.
Depois `G_TT_IA16`, a segunda sala com os ids de objeto montados como segmentos,
a varredura de `ROOM_ANGLE` e o resto dos 16,5 ms da cadeia de mip.

---

## Fase 9 — a área em espaço de olho empatou; o ganho estava no tamanho do inteiro (2026-10-27)

> **ID** `OTD-027` · **Data** 2026-10-27 · **Frente** motor · **Fase** 9 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-027/` · **Commit** árvore sem histórico git

**Objetivo:** a fase 8 fechou com uma frase de direção, não de medida — "o
caminho é a formulação por área em espaço de olho com uma profundidade média por
face, validada face a face". A tarefa era construir essa formulação, validá-la
face a face contra o shoelace e só então decidir se ela substituía o seletor que
está no build. O alvo era parte dos +17,5 ms que a cadeia de mip ainda cobra.

### Entregue

- **`mipOfTri` com seletor parametrizado.** A área de textura e o laço de nível
  passaram a ser comuns; o que varia é de onde vem a área projetada (parâmetro
  `eye`), e um segundo parâmetro (`record`) separa a chamada que desenha da
  chamada de sonda, para que medir não conte duas vezes.
- **Identidade em espaço de olho.** Com `u = Sx·x/w` e `v = Sy·y/w`, o shoelace
  sobre três cantos se reduz a `2A = Sx·Sy·|det[x y w]| / (w0·w1·w2)`. A versão
  exata (`GE_MIP_EYE 1`) troca as três projeções por canto e os quatro produtos
  do shoelace por um determinante de seis produtos e **uma divisão de 64 bits**.
- **Variante com profundidade média** (`GE_MIP_EYE 2`): o produto dos três
  `w` é substituído pelo cubo da média, com o mesmo recíproco por tabela da fase
  8 usado três vezes arrancadas de `n << (48 - bits(n))`, para que áreas pequenas
  não sejam truncadas a zero. Nenhuma divisão.
- **Área em 32 bits** (`GE_MIP_AREA32`, no build entregue): os cantos projetados
  são limitados a ±12000 dezesseis-avos de pixel (±750 px, três larguras de
  painel) e os quatro produtos do shoelace passam a ser de 32 bits.
- **Sondas novas.** `lodEyeDiv`/`lodEyeMax` contam as faces em que os dois
  seletores discordam do nível e a maior discordância; `lodClamp` conta as faces
  em que o limite de 32 bits realmente mexeu num canto. A linha 22 do HUD foi
  encurtada para `gp gt c e/máx` porque o texto antigo corria por baixo dos
  quadrados de cor.

### Descobertas medidas

- **A formulação exata não paga.** `f9b`/`f9b2`: `s48,9` contra `s47,7` do
  shoelace — **1,2 ms mais cara**, com o mesmo painel (md5
  `05992cb58c8229ba3b60a28f949e2e93`). A divisão de 64 bits custa mais do que as
  três projeções e os quatro produtos que ela elimina.
- **E ela decide igual.** Com os dois seletores calculados e o desenho feito
  pelo shoelace (`f9p`), a discordância de nível é **0 em 646 faces**
  (`e0/0`). O determinante não é uma aproximação do shoelace: é a mesma conta
  escrita de outro jeito.
- **A profundidade média também empata.** `f9c2`: `s47,8`, mesmos níveis
  (`Lo 2/25/619/0`), mesmo painel, mesmos contadores (`m644/2/0 r152`). Sem a
  divisão, mas com um determinante e três recíprocos, o total volta ao mesmo
  lugar. A hipótese da fase 8 — de que a formulação por área seria o próximo
  corte — está **refutada**.
- **O ganho estava no tamanho dos inteiros.** `f9e`/`f9e2`: **`s46,5`**, 1,2 ms
  menos que o shoelace de 64 bits, painel md5-idêntico. O limite de ±12000
  dezesseis-avos não foi atingido por **nenhuma face** em três ângulos medidos
  (`c0` a 0°, 22,5° e 45°), e os pares de md5 por ângulo são iguais
  (`d1e029869e5e8180b346081672ec036b` a 22,5°, `d9c832699b57acd90488372b5f95224f`
  a 45°). Com o limite, as quatro multiplicações da área são de 32 bits, que é
  onde o tempo estava.
- **Custo residual:** a cadeia de mip ainda cobra **+16,5 ms** sobre os 30,0 ms
  sem seletor, e o resto está nas multiplicações de 64 bits da projeção por
  canto — não na área.

### Validação

melonDS 1.1, modo DS, 67 MHz, `Bmori1_room_0`, 646 triângulos, `ROOM_ANGLE 0`,
`GE_TEX_MIP_MAX 2`, `GE_TEX_LOD_BIAS 2048`, câmera presa, estado estacionário.
Builds limpos: `f9a` e `f9c` (shoelace 64 bits) `s47,7` nas duas; `f9b` e `f9b2`
(espaço de olho exato) `s48,9` nas duas; `f9c2` (profundidade média) `s47,8`;
`f9e` e `f9e2` (área de 32 bits) `s46,5` nas duas; `f9f` é o build entregue,
`s46,5`, `FPS 19`, `f50,1 w333`, `Bk d4,4 m644/2/0 r152 e0`, mesmo md5. Sondas:
`f9p` (`e0/0`), `f9m` (a primeira versão quebrada da profundidade média, `gp229`)
e `f9m2` (`e0/0` depois do conserto), `g5`/`g2`/`g4` (`c0` nos três ângulos).
Compila sem avisos.

### Erros meus

1. **A primeira versão da profundidade média zerou 229 faces.** A cadeia de três
   `>> 16` truncava a área pequena a zero, e as 229 faces caíram na guarda
   `pxA == 0` e voltaram ao nível 0. Quem pegou foi o contador `gp`, não o olho:
   o histograma mudou de `2/25/619` para `231/22/393` e a primeira leitura que eu
   fiz do campo de divergência (`e23`) não fechava com isso — eu tinha lido o
   número errado. Conserto: deslocar `n` para 48 bits antes dos recíprocos.
2. **Li um contador que estava por baixo dos quadrados de cor.** Duas capturas
   seguidas me deram "c6" onde o certo era `c0`; o "6" era um quadrado de cor. A
   linha foi encurtada e os três ângulos refeitos. É o mesmo erro da fase 8 em
   miniatura: o instrumento não foi conferido antes de virar número.
3. **A direção publicada na fase 8 estava errada** e eu a segui por uma fase
   inteira antes de medi-la. O custo de uma fase para descobrir que a hipótese
   valia zero é aceitável; o que não seria aceitável é ela virar entrega.

### Limites

O ganho é **1,2 ms**, 7 % dos 17,5 ms da cadeia e 2,5 % do render — pequeno, e
não muda FPS. O limite de 750 px é uma mudança de comportamento, mesmo que
nenhuma face o tenha atingido nas três vistas medidas: uma sala com uma parede
que encha a tela de muito perto pode chegar lá, e o contador `c` existe para
ser lido na primeira sala assim. As duas variantes em espaço de olho ficaram no
código como interruptor medido, não como caminho. A varredura de `ROOM_ANGLE`
com grão e custo por vista continua não feita; a desta fase só compara md5 em
três vistas de uma sala.

### Próximo

A pergunta que a fase 6 abriu e que ninguém respondeu: a paleta decorada da
`Bmori1_room_0` bate com a referência do N64 no mesmo ângulo? Depois, `G_TT_IA16`,
a segunda sala com os ids de objeto montados como segmentos, e só então atacar o
resto dos 16,5 ms — que agora se sabe estar na projeção por canto, não na área.

---

## Fase 8 — o custo da cadeia não eram os binds, era a aritmética por face (2026-10-27)

> **ID** `OTD-026` · **Data** 2026-10-27 · **Frente** motor · **Fase** 8 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-026/` · **Commit** árvore sem histórico git

**Objetivo:** pagar o `+19,6 ms` que a fase 7 deixou. A fase 7 escreveu, como
crença e não como medida, que o custo estava na troca de textura (`Tex h65 →
h226`). A tarefa era descobrir onde o tempo estava de fato e devolvê-lo sem perder
o grão de 9,09.

### Entregue

- **Divisão por tabela no seletor.** `recip65536(w)` substitui `65536u / w`:
  `w` é normalizado por `clz`, os oito bits de mantissa indexam uma tabela de 1 KB
  com `65536/(1+u)`, e o resultado é `tabela[j] >> e`. Sem divisor de hardware no
  ARM9, `__aeabi_uidiv` é um laço de 32 etapas e o seletor chamava isso três vezes
  por face.
- **Interruptor de medição `GE_TEX_MIP_NULL`.** Com ele em 1 o nível é calculado e
  descartado, de modo que o quadro paga o seletor e nunca troca de textura. É o
  instrumento que separou aritmética de binds; fica no fonte em 0.
- Nada mais mudou: `GE_TEX_MIP_MAX 2u`, `GE_TEX_LOD_BIAS 2048u`, sonda desligada.

### Descobertas medidas

- **A crença da fase 7 estava errada.** Com o seletor calculando e o resultado
  jogado fora (`mi1`), o render é **49,9 ms** — o mesmo que a cadeia inteira
  (49,6). Ou seja: os **152 re-binds** e as 80 buscas extras de textura custam
  dentro do ruído, e os ~19,9 ms são aritmética pura por face.
- **Onde os 19,9 ms estão**, por subtração de builds (mesma cena, mesmo ângulo,
  `s` em ms): sem seletor **30,0** → seletor com multiplicações de 32 bits no lugar
  da projeção **40,1** (as duas multiplicações 64-bit por canto valem **9,8**) →
  com uma multiplicação 64-bit por canto **43,5** → com a fórmula inteira **49,6**.
  As três divisões valem ~4 ms, das quais a tabela devolveu **2,1**.
- **Recíproco aproximado é indistinguível do exato neste uso.** 8 bits de mantissa
  dão erro abaixo de 0,2 %, e o teste de nível tem passos de fator 4 em área: o
  histograma continua `m644/2/0`, os contadores `Tex b0 h145 x0 e0` / `Bk r152`
  não se movem e o **painel sai md5-idêntico** ao build com a divisão exata
  (`05992cb58c8229ba3b60a28f949e2e93`).
- **A economia de precisão não existe onde eu fui buscá-la.** Duas tentativas de
  baixar de duas para uma multiplicação de 64 bits por canto, ambas refutadas pela
  própria sonda: tirar quatro bits do canto e deslocar por 34 derrubou o
  histograma a `m9/2/0` (o mesmo 4096 da fase 7b, forma nova); tirar dois bits do
  recíproco (`inv >> 2`) derrubou a 418 de 644 faces, porque para uma face a
  duzentas unidades o recíproco **é 1** — e 1 deslocado dois bits é zero. O
  recíproco carrega a informação justamente na parte inteira nas faces distantes.

### Validação

melonDS 1.1, DS mode, 67 MHz, `Bmori1_room_0`, 646 triângulos, `ROOM_SPIN 0` /
`ROOM_ANGLE 0`. Capturas em `%TEMP%\ordsi`: `mi0` (cadeia entregue, divisão exata),
`mi1` (seletor sem efeito, `GE_TEX_MIP_NULL`), `mi2` (sem divisão), `mi3` (32 bits
por canto), `mi4` (uma multiplicação 64-bit por canto), `mj0`/`mj3` (tabela de
recíprocos, **build entregue**), `mj1`/`mj2` (as duas economias de precisão
refutadas). Entregue: **`s47,5` contra `s49,6`** — 2,1 ms devolvidos — FPS 19, grão
9,09, `DL t646 c0 k0 p1 m2/0 x0`, `Tex b0 h145 x0 e0 t646/532 on`,
`Bk d4.4 m644/2/0 r152 e0`, painel md5 `05992cb5…` igual ao build anterior. Build
sem warnings.

### Erros meus

1. **Publiquei a fase 7 com "o custo é a troca de textura"** a partir de dois
   contadores que se moveram juntos, sem nunca ter rodado um build que separasse as
   duas coisas. Custou esta fase inteira para desfazer.
2. **Repeti o erro de escala da fase 7b em miniatura**: escrevi `>> 34` onde a
   conta pedia `>> 22` na primeira variante de uma multiplicação só, e a sonda
   pegou pelo histograma (`m9/2/0`) e não pela aritmética.
3. **Escolhi cortar precisão no lugar errado** (`inv >> 2`) porque a média dos
   recíprocos parece folgada; é a cauda das faces distantes que manda.

### Limites

- **Ainda custa +17,5 ms**, e o que sobra é caro de tirar: são ~16
  multiplicações de 64 bits por face que a formulação atual exige (seis
  coordenadas de tela, mais as duas áreas). O caminho é trocar a formulação por
  área em espaço de olho com uma profundidade média por face — menos
  multiplicações e **nenhuma divisão** —, e isso precisa de validação contra a
  atual face a face antes de valer.
- `inv` é 16.16 inteiro: uma face mais funda que **1024 unidades** de mundo (w >
  65536) tem recíproco zero e cai silenciosamente para o nível 0. Nesta sala
  nenhuma chega lá; numa sala grande, sim.
- A varredura de ângulo com o instrumento são continua não feita, e o custo foi
  medido num ângulo só.
- Continua sem `G_TT_IA16`, sem segunda sala, sem portas nem atores.

### Próximo

1. Formulação por área em espaço de olho, validada contra a atual (comparar nível
   por face numa sonda) antes de substituir.
2. Refazer a varredura de `ROOM_ANGLE` e reportar grão e custo por vista.
3. Paleta contra referência do N64 no ângulo preso; depois `G_TT_IA16` e a segunda
   sala.

---

## Fase 7 — a cadeia de mip existe, e ela primeiro produziu um número saturado que derrubou a fase 6 por engano (2026-10-27)

> **ID** `OTD-024` · **Data** 2026-10-27 · **Frente** motor + jogo · **Fase** 7 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-024/` · **Commit** árvore sem histórico git

> **Correção dentro desta entrega:** `OTD-025` — a posição projetada da sonda
> estava 4096× fora de escala e 467 de 646 faces caíam pela guarda `pxA == 0`;
> a correção e as capturas posteriores estão em
> `evidencias/OTD-025/`. Não é um avanço independente.

> **O custo desta fase foi diagnosticado errado e a fase 8 corrigiu.** O que fica da
> fase 7 é a cadeia, o seletor por área, a régua presa e a retirada da refutação da
> fase 6. O que não fica é "o custo é a troca de textura": medido com um build que
> calcula o nível e o descarta, os binds custam zero e os ~20 ms são aritmética por
> face — a fase 8 devolveu 2,1 ms dele com uma tabela de recíprocos.

**Objetivo:** tarefa #37 do plano — seleção de nível de mip por triângulo no bake.
A fase 6 tinha deixado uma meta explícita: levar o grão do painel de 17,3 para perto
de 2,4 (o grão dos texéis enviados) sem borrar o que está perto. Escopo aqui é
construir o mecanismo em `source/gfx/`, medir o que ele custa e o que ele remove, e
não declarar nada consertado sem os dois números.

**O que a fase entregou veio com um erro seu no meio.** Duas vezes, na verdade: a
primeira fórmula (por aresta) media errado, e a segunda (por área) media certo mas
com a escala de projeção errada — e foi essa segunda que produziu a "refutação" da
fase 6 que este arquivo registrou mais cedo hoje. A refutação está retirada abaixo,
com o número que a matou. A hipótese da fase 6 — minificação — está de pé, e agora
com a cadeia removendo medida.

### Entregue

- **Cadeia de mip no bake.** `texBox` (`source/gfx/ge_draw.c`) filtra cada quadrado
  por bloco: média por canal com arredondamento (`(v+half)>>sh`) e **alpha pela OR do
  bloco**, não pela média — média transformaria um furo de recorte em moeda ao ar
  contra o `GL_ALPHA_TEST` de limiar 1. Nível *n* é lado/2ⁿ com piso em 8×8, que é o
  menor código de textura que o GE aceita.
- **`ensureTex(d, t, mip)`.** A chave do cache de texturas ganhou o nível
  (`e->mip == mip`), o lado efetivo é `base >> mip`, e o lado base é rebaixado
  enquanto `base >> mip` cairia abaixo de 8. `GeTexSlot` ganhou `mip`; `GeDraw`
  ganhou `texCur` e `texMip`, e `syncTex` devolve `texMip` a 0 em cada troca de
  material.
- **Coordenadas acompanham o nível.** `putEye` divide `s`/`t` por
  `GE_TEX_SHIFT + d->texMip` em vez de só `GE_TEX_SHIFT`: a mesma fração da imagem,
  coberta por menos texéis. Omitir isso era a diferença entre uma mip chain e uma
  textura menor com o UV velho.
- **Seletor de nível por triângulo: razão de áreas.** `mipOfTri` projeta os três
  cantos para a tela em dezesseis-avos de pixel e compara **duas áreas**: `texA`, o
  dobro da área do triângulo UV em unidades 10.5 ao quadrado, e `pxA`, o dobro da
  área projetada. A razão de minificação é `R² = texA / (4·pxA)`, e o teste de nível
  é escrito sem formar a razão:
  `while (lvl < max && texA * bs * bs >= (pxA << (2*lvl + 12u))) lvl++;`.
  A projeção de cada canto é
  `sixteenths = vp * m * x / (512 * w)`, com `w = −z_olho` exatamente (as linhas
  `m[8..11] = (dx,dy,C,−1)` e `m[12..15] = (0,0,f,0)` da `glFrustumf32` da libnds),
  e sai `((s64)512 * vp * (m>>6) * inv * (x>>6)) >> 28` com `inv = 65536/(w>>6)` —
  uma divisão 32/32 por canto, o resto produto.
- **`max != 0` antes do teste.** `putFan` só chama `mipOfTri` quando o material tem
  cadeia.
- **Duas formulações mortas no caminho.** (1) Por aresta, com a profundidade *média
  dos três cantos*: a mesma aresta medida por dois caminhos independentes deu 2816
  contra 357, 7,9× de divergência com a profundidade quase constante ao longo dela;
  usar a profundidade por aresta própria super-selecionou (`Lo 0/6/236/404`) e
  borrô o painel (grão 8,73). (2) Por área com a escala errada — ver a primeira
  descoberta abaixo.
- **Constantes nomeadas.** `GE_TEX_LOD_BIAS` (`bs = BIAS >> 6`; 2048 ⇒ sobe de nível
  quando `R ≥ 1`, 1024 ⇒ `R ≥ 2` que é a regra do RDP, 4096 ⇒ `R ≥ 0,5`),
  `GE_TEX_MIP_MAX` (**2**, escolhido por medição, ver a curva de profundidade) e
  `GE_TEX_MIP_FORCE` (0; outro valor força a sala inteira num nível).
- **Instrumento que ficou.** `GE_LOD_PROBE` em `source/gfx/ge_draw.h`, desligado por
  padrão porque divide e porque ocupa as duas últimas linhas do HUD. Registra o
  histograma de níveis, `lodRworst = 4·texA/pxA` = **16·R²** (16 ⇒ um texel por
  pixel, 64 ⇒ dois, 256 ⇒ quatro) com as duas áreas da face recordista, e quatro
  contadores de guarda — `lodGm`, `lodGw`, `lodGp` (área projetada zero), `lodGt`.
  Linhas 21 e 22 do HUD: `Lo n0/n1/n2/n3 r…` e `Lo gp… gt… a… b…`. A linha 23
  **não renderiza**, então em build de sonda a linha 22 deixa de mostrar `Bk`.
  **Foi essa sonda que pegou o erro de escala**: ela imprimia `gp467` e
  `b4294967295`, e `u32` saturado não é uma área medida, é uma área perdida.
- **Contadores.** `mipTris`, `mipBinds`, `mipMax`, `mipFail`, `texRebinds`,
  `texEvict` na linha 22 (`Bk … m…/…/… r… e…`).

### Descobertas medidas

- **O bug: a posição projetada saía 4096× grande demais, e isso era o
  "nenhuma face minifica".** O deslocamento da projeção estava em 16 onde a
  derivação pede 28. Consequência em cadeia: os cantos estouravam o clamp de
  ±1048576 e eram achatados na mesma parede, o laço de área sobre três pontos
  iguais devolvia zero, e **467 de 646 faces saíam pela guarda `pxA == 0`**. As
  que sobravam tinham `pxA` inflado por 4096² e nunca passavam do teste. Resultado
  aparente: `Lo 646/0/0/0`, pior face `r0`, painel md5-idêntico ao da cadeia
  desligada. Eu li isso como "a sala inteira está ampliada neste ângulo" e usei o
  número para refutar a fase 6. Era saturação. Achado ao varrer `ROOM_ANGLE` com a
  sonda ligada — a varredura procurava uma vista que minificasse e achou, no
  primeiro ângulo testado, a própria medição quebrada.
- **Corrigido, o veredito inverte: a sala minifica muito, e a fase 6 estava
  certa.** No mesmo ângulo preso (`ROOM_ANGLE 0`), com a sonda ligada:
  **`Lo 2/25/256/363`** — duas faces no nível 0, 25 no 1, 256 no 2, 363 no 3 — com
  **`gp0`**: nenhuma guarda, nenhuma área perdida. Pior face `r100824`, ou seja
  `R² = 100824/16 = 6301`, **R ≈ 79 texéis por pixel**: uma face de 320 texéis² de
  textura cobrindo 0,05 px² de painel. Os ingredientes batem à mão
  (`4·327680/13 = 100824`), e o `a327680 b13` da linha 22 é a conta, não a
  impressão.
- **A cadeia remove grão de verdade, medido A/B repetido.** No ângulo preso, sonda
  desligada, dois builds de cada lado, dois frames cada:

  | configuração | grão do painel | pares | FPS | `f` | `s` |
  |---|---|---|---|---|---|
  | `GE_TEX_MIP_MAX 0` (cadeia desligada) | **13,28** | 32740 | 20 | 33,4 | 30,0 |
  | `GE_TEX_MIP_MAX 2` (entregue) | **9,09** | 33012 | 19 | 50,1 | 49,6 |
  | `GE_TEX_MIP_MAX 3` | 9,06 | 33019 | 15 | 66,8 | 50,7 |

  md5 repetível em cada linha (`mu0a` = `mu0b`, `mv2` = `mf0`). **−32 % de grão por
  +19,6 ms de render.** Visualmente: os tijolos da parede passam a ler-se como
  tijolos; antes eram aresta de ruído.
- **O custo não é a matemática do LOD, é a troca de textura.** Uma cadeia de
  profundidade 1 já custa `s49,6`, exatamente o que custa a de profundidade 2 — o
  seletor por face (três divisões 32/32 em 646 faces) não é o que some 20 ms; é o
  cache de textura passar a carregar mais de um nível por material e o número de
  binds do frame subir (`Tex … h65` → `h226`). Isso muda o alvo da otimização:
  agrupar primitivos por nível, não afinar `mipOfTri`.
- **Curva de profundidade medida, e por isso o teto é 2.** 13,28 (sem cadeia) →
  11,01 (`MAX 1`) → **9,09 (`MAX 2`)** → 9,06 (`MAX 3`). O terceiro nível compra
  0,03 de grão e custa 4 FPS. `GE_TEX_MIP_MAX 2` é medição, não escolha.
- **Escada de viés medida com a fórmula sã.** 1024 (regra do RDP, `R ≥ 2`) →
  **11,76**; 2048 (`R ≥ 1`) → **9,09**; 4096 (`R ≥ 0,5`) → **6,15**. O 6,15 é o
  mais liso e está errado: a parede vira pasta, os tijolos somem. 2048 ficou
  porque é o ponto em que um texel por pixel encerra o nível, que é a condição em
  que o GE sem filtro começa a descartar amostra. A escada que eu publiquei mais
  cedo hoje (1024 → sala inteira no nível 0, 1448 → 644/2, 2048 → qualquer face
  acima de 1:1 sobe) foi medida com a escala errada e não é evidência de nada.
- **A régua, consertada na fase anterior, é o que tornou isto possível.** Sem
  `ROOM_SPIN`/`ROOM_ANGLE` não haveria A/B repetível, e os dois md5 iguais por
  linha da tabela acima são o que permite afirmar 13,28 → 9,09 como efeito da
  cadeia e não do ângulo.

### Validação

melonDS 1.1, DS mode, 67 MHz, câmera presa em `ROOM_ANGLE 0`, sala `Bmori1_room_0`,
646 triângulos, build sem warnings.

| captura | configuração | resultado |
|---|---|---|
| `mq5`–`mq9` | seletor por área **com a escala errada** | `Lo 646/0/0/0`, `gp478`, `b4294967295`, painel 13,28 = cadeia desligada. **Registrado como veredito; era saturação. Retirado.** |
| `sw8192` | varredura de ângulo, sonda | `Lo 646/0/0/0 gp467` — o mesmo artefato num ângulo diferente, o que denunciou a escala |
| `mr0` | escala corrigida, sonda | **`Lo 2/25/256/363 r100824`**, `gp0 gt2`, `a327680 b13`, painel **9,06** |
| `ms0` | escala corrigida, sonda desligada, `MAX 3` | painel 9,06, FPS 15, `f66,8 s50,7 w366` |
| `mu0a`/`mu0b` | `MAX 0` | painel 13,28, FPS 20, `f33,4 s30,0 w249`, md5 igual entre as duas |
| `mu3a`/`mu3b` | `MAX 3` | painel 9,06, FPS 15, `s50,7`, md5 igual entre as duas |
| `mt0` | `BIAS 1024` | painel 11,76 |
| `mt1` | `BIAS 4096` | painel 6,15, `Lo 2/0/204/440` — liso demais, tijolo perdido |
| `mv1` | `MAX 1` | painel 11,01, FPS 19, `s49,6` |
| `mv2` = `mf0` | **`MAX 2`, entregue** | painel **9,09**, FPS 19, `f50,1 s49,6 w333`, md5 igual entre as duas capturas |

### Erros meus

1. **Usei uma área saturada como se fosse uma área medida.** `b4294967295` é
   `u32` cheio. O número estava gritando que tinha estourado, na linha que eu mesmo
   tinha escrito para denunciar exatamente isso, e eu li o histograma (`646/0/0/0`)
   em vez de ler o `gp478` logo abaixo. A regra que faltou: um contador de guardas
   alto **invalida** a média, não é um detalhe dela.
2. **Refutei uma fase inteira com esse número.** A fase 6 ("o pontilhado é
   minificação") estava certa. Escrevi a refutação no histórico do motor, no do
   jogo e no plano, e ela foi retirada no mesmo dia — é o que o bloco de abertura
   desta seção e a faixa na fase 6 registram.
3. **Publiquei "2 de 646" e "a pior aresta é 7,7 texéis por pixel, verificada à
   mão"** antes disso, com a fórmula por aresta; a verificação era circular (conferia
   o registro da sonda contra os ingredientes que o próprio registro gravara).
4. **Aceitei "17,25 → 7,00" como resultado de fase** sem controlar o ângulo da
   câmera — o erro que a régua presa matou, e que me deixou confiante demais na
   régua: ela garantia repetibilidade, não escala correta.
5. **Tentei consertar o fator 256 na folha com um `>> 2u` adivinhado** e desfiz antes
   de compilar. Certo, mas a mesma pressa me deu o 16 que devia ser 28.
6. **Deixei uma varredura de ângulo sem checar primeiro se o instrumento media
   certo.** O trabalho de varrer era procurar uma vista que minificasse; o que a
   varredura entregou foi a medição quebrada, e isso só aconteceu porque a sonda
   imprimia as guardas. Ela custou ~10 % do render e pagou a fase inteira.

### Limites

- **O custo ainda não foi pago.** A cadeia vale −32 % de grão e custa +19,6 ms
  (FPS 20 → 19). A 20 fps de teto do plano, isso come a margem de tudo que ainda
  vai entrar no frame (atores, HUD, segunda sala). O alvo agora é o número de binds
  por frame, não o seletor. *(Errado, e a fase 8 mediu: os binds custam zero e o
  custo é a aritmética por face; a fase 8 devolveu 2,1 ms com uma tabela de
  recíprocos e deixou +17,5.)*
- A cadeia para no piso do hardware: o menor código de textura do GE é 8×8, então
  uma imagem 16×16 oferece um nível só e 32×32 oferece dois; `GE_TEX_MIP_MAX 2` é
  o teto *escolhido por medição*, e a sala pedia mais (363 faces queriam nível 3).
- O `r100824` da pior face é `R ≈ 79`: sub-amostragem de 79× num hardware que só
  tem três níveis. Mip não resolve face sub-pixel; o que resolve é não desenhá-la.
- Nada foi medido num segundo ângulo desde a correção. A varredura que encontrou o
  bug ainda não foi refeita com o instrumento são, então não se sabe se 9,09 é o
  pior ou o melhor caso da sala.
- `r68` re-binds por frame permanecem, e agora com mais um motivo: níveis.
- Continua sem `G_TT_IA16` (paleta IA16 ainda lida como RGBA16), sem segunda sala,
  sem portas nem atores.

### Próximo

1. Reduzir o custo: agrupar primitivos por nível dentro do mesmo material (ou subir
   o nível só quando o material inteiro pede) para devolver os ~20 ms sem perder o
   9,09. Medida de saída: `s` e o `h` do `Tex`.
2. Refazer a varredura de `ROOM_ANGLE` com a escala corrigida, e reportar o grão por
   ângulo — é a primeira curva real de minificação desta sala.
3. Voltar à pergunta que a fase 6 adiou: a paleta decorada bate com a referência do
   N64 no ângulo preso?
4. `G_TT_IA16` e a segunda sala.

---

## Fase 6 — O pontilhado da sala texturizada não era cor, era minificação (2026-10-27)

> **ID** `OTD-023` · **Data** 2026-10-27 · **Frente** motor + jogo · **Fase** 6 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-023/` · **Commit** árvore sem histórico git

> **Refutada pela fase 7 e restaurada no mesmo dia.** Mais cedo hoje a fase 7
> derrubou esta hipótese com `Lo 646/0/0/0` — e o número era saturação: a escala da
> projeção estava 4096× errada, 467 faces perdidas pela guarda `pxA == 0`, e a face
> recordista imprimindo `u32` cheio. Corrigida a escala, a sala **minifica muito**
> neste mesmo ângulo preso (`Lo 2/25/256/363`, pior face `R ≈ 79` texéis por pixel) e
> a cadeia de mip tira o grão do painel de **13,28 para 9,09**. "Não era cor" nunca
> esteve em dúvida: as correções de endianness, padding e I/IA desta fase são reais e
> continuam valendo. O que cai é a aritmética da prova, não a prova: `448 × 347
> texéis` abaixo é soma sobre a sala toda, não span por face, e o `17,25 → 7,00`
> comparava três vistas diferentes — os dois números que a fase 7 usou para confiar
> demais em si mesma.


**Objetivo:** `Bmori1_room_0` foi a primeira sala com textura a chegar ao painel, e
chegou coberta de confete cor-de-arco-íris. A hipótese herdada era "a paleta CI
está errada". Escopo aqui é fechar a causa com medição — dos dois lados, no bake
offline e no texel já enviado — e consertar o que for real no `source/gfx/`.

### Entregue

- **Instrumento de grão no bake.** `GeTexSlot` ganhou `tlutCount` e `ciAlpha`, e
  `GeDraw` ganhou `bakePix`, `bakeDl`, `bakePairs`, `texMaxS`, `texMaxT`
  (`source/gfx/ge_draw.h`). `ensureTex` soma `|Δluma|` sobre todo par vizinho
  — horizontal e vertical — de cada quadrado **antes** do `glTexImage2D`, com a
  mesma luminância 30/59/11 e a mesma escala 0..31 com que o painel é medido, para
  que os dois números sejam comparáveis em vez de interpretáveis. A medição é
  acumulada entre `geDrawInit` resets: uma sala envia suas texturas no primeiro
  frame e depois só acerta o cache, então contadores por frame leriam zero.
- **Escala de texel nomeada e medida.** `putEye` divide o `s`/`t` do vértice por
  `GE_TEX_SHIFT` (`source/gfx/ge_draw.c`) em vez de um `>> 1` solto. O valor
  correto é **1**: um texel são 32 unidades no 10.5 do N64 e 16 no 12.4 do GE. A
  constante existe porque o divisor errado é exatamente o tipo de erro que some a
  textura e sobra o ruído.
- **Gate `G_TT_NONE` no desbobamento de paleta.** `palConvert` recebeu
  `keepAlpha`; `ensureTex` deriva `ciAlpha` de `d->omFlags & F3_OM_TTALPHA` e,
  quando ele é 0, força o bit 15 de cada texel a 1. `ciAlpha` entrou na **chave**
  do cache de texturas, porque a mesma imagem pode viver nas duas formas dentro de
  um mesmo frame (lista opaca e lista translúcida).
- **Sondas removidas.** O `GE_TEX_SMOOTH` ficou em 1 e o probe que zerava o bit 15
  de cada texel saliu do código; o que sobrou é só o `Bk` do HUD, que não altera a
  imagem.

### Descobertas medidas

- **A paleta estava certa, dos dois lados.** Offline, com os bytes que o alvo vê:
  as 6 imagens CI8 únicas (`R:0xB128 32x32`, `R:0xB528`, `R:0xB928 64x32`,
  `R:0xC128 16x32`, `R:0xC328`, `R:0x8528 16x64`, `S:0x15B90`) resolvidas por
  `scene+0x14288` em **big-endian** A1R5G5B5 dão pedra, madeira e painel coerentes
  — contraste vizinho **5,7 a 8,4**, onde ruído aleatório dá 85, e 64–108 cores
  distintas por imagem. No alvo, a linha `B1` do HUD lê
  `iR:B128 pS:14288 f2 s1 b11`: mesma imagem, mesma paleta, mesmo fmt/siz,
  endianidade 1 nos dois lados.
- **Os texels enviados são cinza e lisos.** `Bk d2.4` sobre 45952 texels enviados,
  canais médios `r13 g12 b12` (meias de 5 bits) e **0 de 45952** texels
  azul-dominantes. O painel mede **17,25**. Os bytes explicam 2,4 de um grão de
  17,25: a textura chega à tela **7× mais rugosa do que ela é**.
- **O controle que fecha o argumento (bm34).** Substituir cada quadrado por uma
  cor chata de mesma luminância média e alpha 1 levou o painel de **17,25 → 0,68**
  e deixou um único trapézio cinza liso, sem estrutura interna nenhuma. Dois
  fatos de uma vez: sombreamento, combiner e alpha test do GE não produzem grão;
  e **toda** a estrutura visível na sala vem da textura.
- **A causa é minificação.** `texMaxS/texMaxT` do frame = **14336 / 11109**, isto
  é **448 × 347 texéis** de span de UV contra uma parede de ~150 pixels de painel:
  ~**3×** de minificação. O geometry engine do DS não tem filtro, mip nem LOD —
  amostra um texel por pixel, e uma texel de 1/9 da área de tela vira confete.
- **Desbastar o quad inteiro não resolve.** `GE_TEX_SMOOTH` 1 / 2 / 3passes dá
  painel **17,25 / 14,04 / 12,21** — um platô, porque desbasta junto as superfícies
  que estão perto e não precisam. Coarsear a escala (`GE_TEX_SHIFT 3`) dá **9,03**,
  mas trapaceia o mapeamento; revertido para 1.
- **O alpha de paleta estava sendo copiado sem o OtherMode pedir.** 116 das 256
  entradas da paleta `0x14288` têm bit 15 = 0, e a fração de texéis que cai nelas
  por imagem é 9,5 / 23,2 / 25,3 / 50,9 / 51,3 / 72,2 / 81,2 %. Com
  `GL_ALPHA_TEST` ligado globalmente em limiar 1 (`source/nds/video_top.c:31`), a
  primeira lista translúcida abriria buracos. `Bmori1_room_0` tem `omFlags = f1`
  (só XLU), `F3_OM_TTALPHA` limpo ⇒ `G_TT_NONE` ⇒ o gate fica desligado e **esta**
  sala não muda — a correção é para as outras.

### Validação

melonDS 1.1, DS mode, 67 MHz, captura `bm33` (estado honesto: shift 1, smooth 1,
gate ligado) e `bm37` (mesma cena depois de limpar as sondas de canal):

| Leitura | bm33 | bm37 |
|---|---|---|
| grão do painel (150 px, 256 amostras) | 17,25 | 17,31 |
| `Bk` | `d2.4 s14336 t11109` | idem |
| `Tex` | `b0 h65 x0 e0 t646/532 on` | idem |
| `DL` | `t646 c0 k0 p1 m2/0 x0` | idem |
| FPS | 30 | 30 |

Repetível entre builds. Nenhum probe que altere a imagem sobrou no tree.

### Erros meus

1. Persegui "paleta errada" por uma fase inteira por causa de uma estatística
   minha medida errada: usei a máscara `0x7C1F` em vez de `0x83FF` e depois
   comparei um predicado `b > r+6` em 8 bits com o mesmo predicado em 5 bits. Uma
   máscara que também limpa o verde não é um teste de azul.
2. Chamei de "não é paleta" uma janela que **é** paleta: despejei `scene+0x14288`
   na ordem de bytes errada e ela pareceu ruído. A verificação correta
   (`b128_variants.py`, big-endian) já estava escrita na pasta de captura e não foi
   relida antes da conclusão.
3. Concluí, a partir do build `bm27`, que o bit 15 do texel governava o alpha test.
   A máscara daquele build (`0x7C1F`) zerava também os 5 bits do **verde** e
   mantinha vermelho e azul — o painel preto medido era a perda do verde.
4. Duas capturas (`bm25`, `bm26`) rodaram binário velho: `make` não recompila
   `source/main.c` nem `source/gfx/ge_draw.c` sozinho nesta árvore OneDrive. Agora
   `touch` nos dois antes de todo build.
5. Contadores de bake resetados por frame, o que dava `Bk 0` no regime estacionário
   em que nenhuma textura é enviada — um zero lido como "não há upload".

### Limites

- O grão **continua na tela**: é o preço de um hardware sem filtro de textura, e a
  resposta é a próxima fase (mip por triângulo), não um conserto desta.
- Nada de `G_TT_IA16`: paleta em intensidade+alpha ainda é lida como RGBA16.
- Bug latente conhecido, não corrigido aqui: `st->tluts[slot].be` e `pend.tlutBe`
  herdam a endianidade do **segmento da imagem**, não a da TLUT
  (`source/gfx/f3dex2.c:509`, `:522`).
- O `GL_ALPHA_TEST` global com limiar 1 continua sendo uma política do port, não
  uma réplica do pipeline do N64.

### Próximo

Tarefa #37 — seleção de **nível de mip por triângulo** no bake: o nível vem do
span de texéis da triângulo contra seu span projetado em pixels de painel, a chave
do cache ganha o nível, e a saída é medida pelo mesmo `Bk d` desta fase (meta:
painel de 17,3 para perto de 2,4 sem borrar o que está perto). Depois `G_TT_IA16` e
a segunda sala.

---

## Fase 6 — O decodificador aprendeu a ler big-endian, e o plano perto foi confirmado numa sala real (2026-10-27)

> **ID** `OTD-022` · **Data** 2026-10-27 · **Frente** motor + jogo · **Fase** 6 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-022/` · **Commit** árvore sem histórico git

**Objetivo:** o motor até aqui só tinha visto listas que ele mesmo construiu na
RAM do DS, em little-endian. Uma display list vinda do cartucho é o oposto:
`Vtx`, `Mtx` e `Vp` são estruturados para o R4300, e o arquivo cru da sala chega
com os campos de 16 bits deitados ao contrário. Sem isso a sala não entra, e sem
a sala nada do resto do capítulo vale medida. Escopo aqui é `source/gfx/`; o
carregador da sala e a câmera estão em `HISTORICO_JOGO.md`.

### Entregue

- **Segmento marcado por endianidade.** `F3State` ganhou `u32 segBe`, um bit por
  segmento, e `f3SetSegmentBe(st, seg, be)` (`source/gfx/f3dex2.c:622`). Toda
  origem de cópia — `G_VTX`, `G_MTX`, `G_MOVEMEM` de viewport, e o alvo de
  `G_DL`/`G_DISPLAYLIST` — consulta `segBe(st, addr)` e decide na hora. Uma lista
  alcançada por `G_DL` herda a endianidade do segmento em que o próprio ponteiro
  mora, não a da lista que a chamou.
- **Três formatos de cópia, três regras.** `copyBe16` troca **meia-palavra por
  meia-palavra**, não palavra por palavra — num `Vtx` os campos `y` e `flag`
  cabem dentro da mesma palavra de 32 bits, e virar a palavra inteira põe `y` no
  lugar de `x`. `copyVtx` faz 12 bytes assim e copia os 4 de cor **in natura**
  (`r,g,b,a` são bytes). `copyMtx` — dezesseis palavras 16.16 — pode virar
  palavra inteira, e vira com `__builtin_bswap32`. `copyVp` é oito `s16`, então
  volta para a regra do `Vtx`. Os três tamanhos estão fechados por
  `_Static_assert` (`sizeof(F3Vtx) == 16`).
- **O prologue do carregador é little-endian de propósito.** O segmento 3 (o
  arquivo cru da sala) é marcado BE; o segmento 5, de onde sai a lista que o
  próprio port construiu na RAM, **não** é. Marcar os dois foi o erro que
  apareceu como viewport deitado: `gSPViewport` lido com swap de 16 bits
  devolve um `Vp` com `vscale` no lugar de `yoffset`.

### Descobertas medidas

- **`projNearZ` conferido ao vivo, e não mais só no autoteste.** A sala
  `market_day_room_0` entrou com `near 667,5` e `shift 9`. O motor, lendo a
  projeção que o prologue carregou, publicou `nearZ = -5339`. A fórmula da fase
  anterior é `-m[14]·4096 / (m[10] + m[11])`, que para `guPerspective` é
  `-near · 2^(12-shift)` = `-667,5 · 64` = **−42720** em 1.19.12 antes do
  truncamento por palavra... e −5339 é exatamente isso dividido por 8, o
  `>> 4 + shift` de `mtxToDs` aplicado à translação. Os dois números são o mesmo
  plano perto visto de dois estágios, e a sonda do HUD confirmou a paridade no
  alvo, não no papel.
- **A escala mundo→GE (tarefa #27) é um `shift` por sala, não um `shift` por
  contexto.** `geDrawInit(8)` continua o default das listas de demonstração; a
  sala pede o seu. O critério medido: o menor `shift` em `6,14` tal que
  `(dist + raio) · 2^(12-shift) ≤ 32000`, porque `putEye` clampa em `v16` e um
  vértice clampado não é avisado pelo hardware — é só um vértice no lugar
  errado. Com `sh9` a sala deu `c0`; com o `shift` herdado de 8 teria dado
  clamp em vários cantos.
- **510 triângulos, `c0 k0 x0`, `ok`.** Primeira vez que uma lista de verdade,
  vinda de um arquivo de verdade, sai do decodificador sem nenhum contador de
  erro ligado. `Bl 0/510` diz que nenhum deles é translúcido nesta sala com
  estes objetos montados (nenhum), e `p1 m2/0` é o prologue: uma projeção, duas
  modelviews, zero níveis pendurados no fim.

### Validação

Builds `room5` (câmera consertada) e `room7` (HUD arrumado), melonDS, capturas
`room5_top.png`, `room6_top.png`, `room7_top.png` em `%TEMP%\ordsi`. Linhas do
motor: `F3 t4 mtx 1/1 1.5ms MtxOK`, `DL t510 c0 k0 p1 m2/0 x0 ok`,
`Tex b0 h0 x0 e0 p0 t0/0 off`, `CC e16 t128/128/128/128 p4/12 ok`,
`Cut 0 a6 n17 v1 c0-255,0-199`, `Bl 0/510 a31 n34 f0 p0 on`. FPS 30, render
22,9 ms.

### Erros meus

- Escrevi o `copyVtx` com `copyBe16` nos 16 bytes inteiros, o que embaralha a
  cor; os 4 bytes de `r,g,b,a` têm de ser copiados sem tocá-los.
- Marquei o segmento do prologue como big-endian "porque a sala é", e passei uma
  iteração olhando para um viewport torto antes de desconfiar da marca e não da
  conta.
- Aceitei "tela preta" como sintoma de textura faltando. Tela preta com
  `k510` é recorte, e recorte com `k` igual ao total de triângulos é câmera — o
  HUD já dizia as duas coisas e eu não li o `k`.

### Limites

- **Ordem de bytes de texel** ainda não existe: um `G_SETTIMG` que resolvesse
  hoje leria RGBA16 big-endian como little-endian. Está adiado de propósito até
  os segmentos de objeto entrarem, porque sem eles não há texel a medir.
- `G_LOADBLOCK`/`G_SETTILE` de sala real nunca rodaram no alvo; `Tex b0` não é
  "o caminho de textura quebrou", é "nenhuma textura foi pedida a um segmento
  montado".
- O `clipDrop` continua contador sem política: um triângulo descartado pelo
  plano perto não é reemitido com wedge, igual ao RSP, e isso ainda não foi
  medido numa lista que atravesse o plano perto de verdade.

### Próximo

Segmentos de objeto da sala (tarefa #31) para os `G_SETTIMG` resolverem, e só
então a endianidade de texel. Depois, `G_MTX` vindo da própria lista da sala,
que hoje é sobrescrito pelo prologue.

---

## Fase 2 — O recorte de polígono entrou, e a lei do geometry engine que o justificava estava errada (2026-10-27)

> **ID** `OTD-019` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** [`evidencias/OTD-019/` (evidencias/OTD-019/)` · **Commit** árvore sem histórico git

**Objetivo:** fechar a tarefa #26, o estágio de recorte equivalente ao RSP. A
fase nasceu de uma afirmação minha — a de que o clipper do geometry engine
descarta o triângulo inteiro no plano longe e pintura no lugar errado quando
`w ≤ 0`, medida na sessão anterior. Para escrever o recorte foi preciso aplicar
a modelview em software, e isso expôs a afirmação a um par A/B de builds. Ela
não sobreviveu.

### Entregue

- **Modelview em software.** `onMtx` de carga de modelview agora grava só a
  sombra `d->mtx` e não toca no hardware; `geDrawList` abre com
  `glMatrixMode(GL_MODELVIEW); glLoadIdentity()`. Os vértices entram em espaço
  de olho por `geEyeOf` (`source/gfx/ge_draw.c:64`), que é o mesmo
  `(m·v + T·4096) >> 12` que o motor faria.
- **Pilha de níveis separada por alvo.** `shStack[F3_MTX_STACK]` +
  `mtxType[]` guardam o que cada `G_MTX_PUSH` precisa restaurar; nível de
  projeção ainda tem `glPushMatrix`/`glPopMatrix` no hardware, nível de
  modelview não. `popLevels` fecha em `setMtxMode(false); syncNear(d)`.
- **Recorte Sutherland–Hodgman contra um plano.** `clipNear` (`:906`) corta em
  `z = nearZ`, emitindo o cruzamento em toda troca de lado — a forma canônica,
  não a variante "só na entrada", que perde um vértice quando dois cantos ficam
  atrás. Interpola posição, texel 10.5 e cor 0..255; devolve 3 ou 4 vértices e
  `putFan` emite leque de 1 ou 2 triângulos.
- **`nearZ` lido da projeção carregada:** `projNearZ` (`:182`) resolve
  `-m[14]·4096 / (m[10] + m[11])`, falso quando `m[11] == 0` — ortho não tem
  plano perto. Para o `guPerspective(60, 4/3, 200, 1000, 1)` do jogo isso dá
  olho −3200, que com 16 unidades de olho por unidade de mundo é mundo −200
  exato. `syncNear` é chamado em toda carga/multiplicação/pop.
- **Contadores novos** `clipTris` / `clipDrop` na `GeDraw`, com `k` na linha 18
  do HUD ao lado de `c` (vértices clampados) — é o par que fecha a história.
- **`geMtxSelfTest` consertado:** as duas expectativas de translação pediam
  `>> (8 + shift)` (128 e −1200) enquanto `mtxToDs` divide por `>> (4 + shift)`,
  que é o valor **medido** na fase anterior. Estava `MtxBAD` no HUD desde
  então. Corrigido para 2048 e −19200, comentário reescrito, e asserção nova
  fechando `geProjNearZ` do `guPerspective(1, 2, 100, 300)` sintético em −6400
  (= mundo −100).

### Descobertas medidas

- **O plano longe não descarta — corta.** Sonda `vtx[3].z = -2000` com a lista
  do jogo parada (`angle += 0`, captura `cl4`): a borda esquerda do losango
  vermelho, que no limpo é 182,164,146,130,112,110,126,144,160,176,192,210,226
  (um canto), passa a 188,194,196,200,204,208,210,214,218,220,224,228,230 —
  **uma reta**. Triângulo presente, borda cortada. A afirmação da sessão
  anterior estava errada.
- **O plano perto com `w` pequeno já é cortado certo pelo motor.** Par A/B com
  `vtx[7].z = +300`: `cl2` (recorte ligado) contra `cl3` (mesmo código com
  `GE_NO_CLIP`) diferiram em **6 linhas de diff**, isto é, uma coluna de scan em
  uma faixa de duas linhas. `cl2` mostra `DL t5 c0 k1` — o corte aconteceu, duas
  triângulos no lugar de um — e mesmo assim o desenho é o do motor.
- **Onde o recorte próprio vale, e é grande.** Mesma sonda em `vtx[7].z =
  +20000`: o `z` de olho é 320000, dez vezes o que a entrada `v16` comporta.
  Sem cortar (`cl6`): `DL t4 c1 k0` — **um vértice clampado** — e a borda do
  corte fica 14 colunas de painel por dentro de onde o RSP a põe (`cl5`,
  `DL t5 c0 k1`, nada clampado). As duas varreduras coincidem na borda esquerda
  e divergem na direita, linha 0: 349 cortado contra 321 sem corte.
- **Por que o motor erra sozinho:** ele testa `posição` contra `w`
  (`ClipSegment`, `GPU3D.cpp:856`), então `w` negativo inverte os seis planos de
  uma vez — um vértice atrás da câmera é tratado como se estivesse além do plano
  longe. O corte em espaço de olho garante `w > 0` antes de qualquer
  submissão, e mantém as coordenadas dentro da entrada.
- **Causa do desvio de 1 px no caminho rápido, lida no emulador:**
  `UpdateClipMatrix` (`GPU3D.cpp:746`) pré-compõe `ClipMatrix = ProjMatrix ·
  PosMatrix` e transforma o vértice uma vez só. O caminho novo trunca em cada
  estágio, `>> 12` na modelview e `>> 12` na projeção — que é o que o RSP faz.

### Validação

- Regressão com `angle += 0`, `cl1` (novo, limpo) contra `sZ.txt` (baseline da
  sessão anterior): **30 de 324 linhas** diferem, todas por ±1 coluna de painel
  numa borda; nenhum run aparece, some ou se desloca. Aceite ajustado — o
  byte-idêntico não é mais alcançável, e o motivo está medido acima.
- Estado final (`cl7`, tudo restaurado): `MtxOK`, `F3 t4 mtx 1/1 1.0ms`,
  `DL t4 c0 k0 p1 m2/0 x0 ok`, cena completa (grade, faixa do jogo, inventário
  das duas telas).
- Capturas e varreduras em `%TEMP%\ordsi`: `cl1`…`cl7` com `_top` de 512×384,
  `s1f` `s2f` `s3f` `s4f` `s5f` `s6f`, diffs `d1`…`d4`.

### Erros meus

- Escrevi uma "lei do hardware" a partir de leitura de fonte e a usei para
  justificar uma fase inteira, sem par A/B. A afirmação falsa estava no
  comentário de `onMtx`, na tabela deste arquivo e no histórico anterior.
- Chamei de lixo a silhueta que era o corte correto. Um quad com um canto atrás
  da câmera **deve** vazar do próprio contorno na tela: o ponto que cai no plano
  perto é amplificado 2,5× em relação aos vizinhos a `z = -500`.
- Deixei `MtxBAD` estampado no HUD por uma fase e não olhei o HUD.
- Quase encerrei a tarefa como "não necessária" com base no primeiro par A/B
  (`z = +300`). O segundo valor (+20000) é que mostrou o efeito; o caso pequeno
  simplesmente não exercita o clamp.

### Limites

- Nenhum recorte contra `x = ±w` / `y = ±w` no port — o motor trata, e a lista
  de demonstração nunca sai da tela.
- O corte interpola em espaço de olho, como o RSP, mas o GE reinterpola texel em
  perspectiva a partir dos vértices cortados: textura de triângulo cortado não é
  bit-a-bit a do N64.
- Tudo medido no melonDS 1.1; o próprio emulador comenta "clipping exhibits
  oddities on the real thing", e o portão de DSi real continua aberto.
- `clamped` ainda é só um contador. A escala mundo→GE (tarefa #27) é o que
  transforma isso em política, e agora tem onde pendurar a translação da câmera.

### Próximo

Política de escala mundo→geometry engine com a translação da câmera aplicada em
software; `shift` por lista em vez de um só no `geDrawInit`.

---

## Fase 3 — Quem manda na profundidade é o **menor** `z/w`, e a coluna de translação divide por 2^shift (2026-10-27)

> **ID** `OTD-018` · **Data** 2026-10-27 · **Frente** motor · **Fase** 3 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-018/` · **Commit** árvore sem histórico git

**Objetivo:** a série do jogo passou a montar a modelview com a pilha de
matrizes do próprio `sys_matrix.c`. Isso trocou a matriz que entra em
`mtxToDs`, e a regra de conversão dos slots 12..14 — que a sessão anterior
havia "corrigido" por derivação — passou a decidir se a lista do jogo aparece
ou não. A regra foi fechada em pixels, e o teste fechou junto uma lacuna que o
autoteste do motor não cobria: a grade de benchmark vinha escondendo a faixa do
jogo havia três execuções, e a entrada anterior registrou isso como sintoma sem
consertar.

**Entregue:**

- **`source/gfx/ge_draw.c` — `mtxToDs` confirmado e documentado**
  ([:32](source/gfx/ge_draw.c#L32)):
  `v >>= (j >= 12 && j < 15) ? 4 + shift : 4`. O comentário do topo agora carrega
  a medição em vez da derivação, porque a derivação errou duas vezes nesta sessão.
- **`geMtxSelfTest`** ([:101](source/gfx/ge_draw.c#L101))
  volta a fixar os dois números que a regra produz: `T = 32` em shift 6 →
  `m[14] == 2048`, e o termo `A = -300` de uma projeção → `m[14] == -19200`.
- Nenhuma outra mudança no motor. A correção do fundo está em
  `HISTORICO_JOGO.md`, série do jogo, porque é `source/nds/video_top.c`.

### Descobertas medidas

- **Translação de matriz vale o mesmo que translação de vértice.** Com a lista do
  jogo sozinha na tela, rotação e emenda em zero: a costura vermelha/verde fica na
  coluna **127,5** — o centro do painel — e o par inteiro ocupa colunas
  **64..190** para os 384 `x` de mundo entre os cantos, ou **0,331 coluna por
  unidade de mundo**. Pondo só a translação `x` da matriz em 60, a mesma costura
  vai à coluna 146,5: **19 colunas por 60 unidades, 0,317 cada**, 4 % do valor de
  um deslocamento de vértice. Os dois slots compartilham o divisor, e o fator 16 a
  mais que "1.19.12 contra um 16.16" previa não existe.
- **A "correção 16×" da sessão anterior estava errada e foi revertida.** Com
  `>> 8 + shift` os quadrados do jogo somem do painel: `flat 14` é também o termo
  `A = 2nf/(n-f)` de `guPerspective`, e dividi-lo de novo por 16 tira a geometria
  da faixa de recorte do DS.
- **O GE fica com o MENOR `clip z/w`.** Quatro pontos, mesma cena, só a
  profundade da grade mudando — e a grade `glOrthof32` envia `z_out =
  -(2z+far+near)/(far-near)`, ou seja `-z/4096` com o par simétrico:

  | `GRID_Z` | `z/w` da grade | `z/w` do jogo | faixa do jogo |
  |---|---|---|---|
  | 0 | 0,00 | +0,5 | some |
  | +3072 | −0,75 | +0,5 | some |
  | +4095 | −0,9998 | +0,5 | some |
  | −3072 | **+0,75** | +0,5 | **aparece inteira** |

  Os três primeiros são menores que +0,5 e ganham; o último é maior e perde. A
  entrada anterior tinha visto o mesmo efeito e contornado desligando a grade.
- **`glOrthof32` e `glFrustumf32` têm sinais de `z` opostos, e o do frustum é o
  mesmo do N64.** Lidas as duas construções no cabeçalho do libnds
  (`videoGL.h:1024` e `:1111`): o frustum do libnds produz exatamente os dezesseis
  valores de `guPerspective` — `-(f+n)/(f-n)` em `m[2][2]`, `-2fn/(f-n)` em
  `m[3][2]`, `-1` em `m[2][3]` —, então **uma projeção do jogo não precisa de
  remapeamento de profundidade nenhum**. O ortho, sim: ele manda o plano perto
  para `+1` e o longe para `-1`, o inverso do teste que o próprio GE aplica. É
  por isso que um overlay ortho nativo do DS some atrás de geometria em
  perspectiva do jogo, e por que o fundo tinha que ir a `z` negativo.
- **Sem transposição, confirmado por construção e não por sorte:** a ordem em que
  `glOrthof32` empilha os 16 valores é a ordem row-major do `Mtx` da libultra, que
  é a ordem column-major que o GE lê. Bate elemento por elemento.

### Validação

melonDS 1.1, DS mode, 67 MHz. `ootiso3` / `ootiso4` (lista do jogo isolada,
translação 60 e 0 → costura em 146,5 e 127,5), `ootmA` (grade ligada, lista de
demonstração desligada → faixa some), `ootmB` (grade desligada de verdade → faixa
inteira), `ootmtx4` / `ootmtx5` (`GRID_Z` 4095 e 3072 → ainda some) e
**`ootmC` / `ootmC2`** (quadro completo normal: faixa do jogo em painel
linhas 2..19 **e** cena de demonstração inalterada, `R55..70 W71..97 R98..116`
na linha 24 idêntico ao baseline `ootvp8`). Autoteste `OoT 49/49`.

### Erros meus

- **Derivei duas vezes e errei duas vezes.** Primeiro "provei" que `>> 4 + shift`
  estava certo assumindo que o vértice chega como `X·2^(16-shift)` — conclusão
  certa, argumento errado; depois "provei" o `>> 8 + shift` depois de ler o
  `12 - shift` de `putVertex` — conclusão errada, porque o slot dividido é
  compartilhado com o termo `A` da projeção. A regra inteira desta função só é
  confiável medida, não deduzida.
- **Aceitei um número de medição anterior sem remedi-lo.** A nota da sessão
  passada dizia "costura na coluna 247,5, 0,664 coluna por unidade, 2× o previsto".
  Reconstruí o comentário em cima disso. Re-medido hoje: 127,5 e 0,331 — o previsto
  de primeira mão. O número velho era de uma varredura com a grade de
  demonstração no meio.
- Gastei três ciclos de build/teste numa hipótese (sinal do ortho) que dois
  minutos lendo `videoGL.h` teriam fechado.

### Limites

- A regra "menor `z/w` ganha" foi medida para ortho contra perspectiva do jogo.
  Não sei ainda o que ela implica para a cena real do OoT inteira (o jogo ordena
  translúcidos por distância de câmera, não por `z/w`), e é aí que a tarefa #27
  entra de verdade.
- Nada disto foi visto em DSi real; o portão da tarefa #5 continua aberto.

### Próximo

Tarefa #27: política de profundidade e escala mundo→GE para uma cena com
múltiplas projeções ortho e perspectiva no mesmo frame. Tarefa #26: recorte de
polígono contra `x = ±w`, `y = ±w` e a faixa de `z`, que hoje descarta o
triângulo inteiro.

---

## Fase 3 — O viewport é dois cantos contados de baixo, e `FORCE_BL` não basta para ser translúcido (2026-10-27)

> **ID** `OTD-017` · **Data** 2026-10-27 · **Frente** motor · **Fase** 3 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-017/` · **Commit** árvore sem histórico git

**Objetivo:** a série do jogo ligou o `Gfx_Alloc` do decomp ao bump pointer que
o decodificador consome, e a lista que ele produz entrou no F3DEX2. Faltava o
contrato de verdade: **essa lista tem de pintar pixels no lugar onde o N64 a
pintaria**. Isso expõe dois defeitos do backend que nenhuma contagem de
triângulo no HUD mostrava — o eixo do viewport e a classificação de opacidade.

**Entregue:**

- **`source/gfx/ge_draw.c` — `onVp` reescrito** ([:677](source/gfx/ge_draw.c#L677)):
  a faixa `[y1,y2)` em linhas do painel (topo = 0) vira os cantos
  `192 - y2` (baixo) e `191 - y1` (alto), porque o `GFX_VIEWPORT` conta y a
  partir da **base** do painel e o segundo par são **cantos**, não tamanho.
- **`source/gfx/f3dex2.c` — `omFlags` corrigido** ([:106](source/gfx/f3dex2.c#L106)):
  um primitivo é translúcido quando `OM_L_ZMODE ≠ 0` **ou** quando `FORCE_BL`
  está ligado **e** a cor de blend do ciclo 1 é `G_BL_CLR_MEM`. As flags novas
  `OM_L_BLEND` / `OM_L_BLEND_MEM` (`:100`–`:104`).
- **`source/gfx/ge_draw.h`** — o comentário de `F3_OM_XLU` e o dos campos
  `vpX1..vpY2` agora dizem o que foi medido (cantos, y de baixo).

### Descobertas medidas

- **`GFX_VIEWPORT` = `(x1,y1)`–`(x2,y2)`, inclusivo, y da base.** Três cópias de
  uma mesma lista num frame, cada uma com seu viewport e sua cor. Duas faixas
  submetidas como `(11,50)` e `(71,110)` pintaram **39 linhas cada** —
  `|y2-y1|+1`, não o tamanho que o modelo de deslocamento previa (50 e 110).
  Uma terceira, com `y1 > y2` (`(167,24)`), somou 4 triângulos no contador
  (`DL t4 c0`) e **zero pixels** no painel: retângulo invertido não desenha.
- **Eixo invertido custava silêncio, não espelhamento.** Antes da correção, a
  faixa do topo do painel (`y1=0`) era enviada como `(0,22)` — válida, mas
  espelhada na base. Depois, `(169,191)` → painel linhas 0..22.
- **Opacidade:** `RM_OPA_SURF` já liga `FORCE_BL`, então testar só esse bit
  classificava como translúcida toda superfície opaca sem Z-buffer do jogo — e a
  lista do jogo, com `Bl 4/4`, não pintava. O que separa `RM_OPA_SURF` de
  `RM_XLU_SURF` é a segunda cor do blender ser `CLR_MEM`. Medido: `Bl 0/4` e o
  autoteste F3DEX2 de volta a `MtxOK`.
- **Calibração da sonda:** a tela de cima começa em **y de janela 56**, não 60.
  Toda linha de painel citada nas entradas anteriores deste arquivo está 4
  linhas baixa. `rerun.ps1`, `scanrg.ps1` e o novo `scankey.ps1` corrigidos.
- **Por que três execuções seguidas leram "a lista não pinta nada":** a grade do
  benchmark cobre o painel inteiro com profundidade 0,5; os quadrados do jogo
  estão em 0,75 e **perdem o teste de profundidade** onde a grade existe. A
  faixa apareceu assim que a grade foi desligada para a captura.

### Validação

melonDS 1.1, DS mode, 67 MHz. Capturas `ootvp3` (três faixas, modelo de tamanho
→ uma só visível), `ootvp5` (duas faixas de altura total → 39 linhas cada,
modelo de cantos), `ootvp6` (frame normal corrigido: `CC S a4 n1 v1 0-255,169-191`,
`DL t4 c0 p1 m2/1 x0 ok`, `Bl 0/4`), `ootvp7` (grade desligada) —
**faixa medida em painel linhas 5..16, x 64..117 vermelho e 118..190 verde**,
contra 5..17 e 64..191 previstos. Cena de demonstração inalterada (`ootvp8`).

### Erros meus

- Li `glViewport(x, y, width, height)` pelos **nomes dos parâmetros do libnds**
  e não medi. Uma sessão inteira de raciocínio sobre "o GE não respeita a ordem
  do FIFO" nasceu daí; a medição `ootvp5` mostra que respeita.
- Concluí "a lista do jogo não pinta pixel" a partir de uma armação de isolação
  que também não pintava nada (triângulo cru em `glBegin` + `glFlush(0x80000000)`).
  Harness que não tem controle positivo não prova nada.
- Disse três vezes que a ordem de submissão mudava o resultado. Era profundidade.
- Inverti o eixo do viewport e, no mesmo commit, troquei canto por tamanho —
  duas mudanças num único passo, e a segunda anulou a primeira de um jeito que
  parecia "piorou".
- Procurei um resíduo de 4,5 linhas no mapeamento quando era a origem da captura
  que estava errada; bastava medir onde a moldura da janela virava preto.

### Limites

- Tudo medido em emulador. O geometry engine real pode tratar retângulo
  invertido de outro jeito; o portão da tarefa #5 continua aberto.
- A escala 320×240 → 256×192 é aplicada por borda em inteiro: um viewport do
  jogo cujas linhas não sejam múltiplo de 5 chega a errar 1 linha.
- Não há recorte contra os planos do tronco: geometria fora do frustum é
  descartada pelo GE, não cortada como o RSP fazia.

### Próximo

Estágio de recorte (contra `x = ±w`, `y = ±w` e a faixa de `z`) em
`source/gfx/ge_draw.c`, e a política de escala mundo→GE (`shift`), porque as
coordenadas do OoT passam muito do ±8 em 1.19.12 que o motor aceita.

---

## Fase 2 — O cache de 64 slots e o despejo a meio do frame que não existe (2026-10-27)

> **ID** `OTD-013` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-013/` · **Commit** árvore sem histórico git

**Objetivo:** a fase anterior deixou uma regra correta e cara — um slot do cache
só podia ser reciclado se nenhum triângulo **deste** frame o amostrasse, e se
todos estivessem pinados o upload era recusado. Com 16 slots, uma sala do OoT
(ambiente + atores) passa disso e a recusa vira triângulo sem textura na tela.
A pergunta desta fase era: quanto o geometry engine realmente demora para dizer
"terminei", e o que acontece com a lista quando ele não diz?

**Resposta medida:** ele diz no fim do frame, e a meio do frame **não diz** —
nem no emulador. Então o caminho de verdade é capacidade (64 slots cabem uma
sala inteira), e a espera vira uma saída de emergência limitada por relógio.

### Entregue

- **`ge_draw.h` — `GE_TEX_CACHE` 16 → 64** ([:17](source/gfx/ge_draw.h#L17)).
  Os dezesseis da fase anterior foram palpite, não hardware: o DS tem 656 KB de
  VRAM, que comportam sessenta e quatro texturas 32×32 RGBA16 com folga, e o
  próprio libnds entrega até `MAX_TEXTURES` 2048 nomes. O limite real sempre foi
  memória, e é o LRU que mantém isso verdadeiro.
- **`ge_draw.c` — `geGpuDrain`** ([:357](source/gfx/ge_draw.c#L357)):
  espera por `GFX_STATUS` bit 26 (FIFO vazio) **e** bits 27/14 limpos (motor de
  geometria e pilha de matrizes ociosos) **e** `GFX_POLYGON_RAM_USAGE & 0xFFF`
  igual a zero. O terceiro termo é o que importa: o geometry engine só
  transforma vértice e arquiva polígono, quem lê texel é o renderizador, triân-
  gulo por triângulo, e ele está atrasado em relação à CPU.
- **`ge_draw.c` — o teto é relógio, não contagem**
  ([:354](source/gfx/ge_draw.c#L354),
  `GE_DRAIN_POLLS` 1024 / `GE_DRAIN_US` 2000): a contagem só decide de quantas
  em quantas leituras o relógio é consultado; quem limita o espera são 2 ms de
  tempo real. `drainGaveUp` garante no máximo **uma** espera perdida por lista.
- **`ge_draw.c` — `texUnpinAll` + `texEvictScan`**
  ([:400](source/gfx/ge_draw.c#L400)
  e [:413](source/gfx/ge_draw.c#L413)):
  quando a espera realmente retorna ocioso, o `frame` que pinava cada slot é
  rebaixado em um nível — o carimbo LRU fica intacto, porque é ele que decide
  qual slot é reabastecido primeiro. Os dois passam a varrer `texSlots`, não o
  array inteiro.
- **`ge_draw.c` — `geDrainProbe`** ([:904](source/gfx/ge_draw.c#L904)):
  roda a lista de demonstração três vezes no boot — cache encolhido para 8 slots
  (transbordo forçado), cache de produção, e a espera depois de um vblank — e
  escreve o resultado em duas linhas do HUD
  (`printDrain`, `source/main.c:184`). O `texSlots` é o que permite forçar o
  caminho sem mudar a configuração enviada.
- **`dl_demo.c` — 24 texturas novas numa lista aninhada**
  (`initDrill`, [:741](source/gfx/dl_demo.c#L741)):
  a lista principal chama a risca por `G_DL` (`0xDE010000`,
  [:326](source/gfx/dl_demo.c#L326)),
  cada um dos 24 quads tem imagem RGBA16 de 16×16 de cor própria, e a lista
  termina em `G_ENDDL`. Isso levou a lista de 10 para 26 imagens distintas e
  serviu de carga para o teste de capacidade.

### Descobertas medidas

| Medida | Valor no melonDS (DSi, tela 60 Hz) |
|---|---|
| Configuração de produção, em regime | `Tex b0 h34 x0 e0 p0` — 0 binds, 34 acertos, 0 recusas, **0 despejos** por frame |
| Imagens distintas da lista de teste | 26 ligadas, 0 recicladas (`D64 b26 e0`) |
| Custo da lista com 26 texturas | `FPS 60 f16.7 s3.5 w19 ms` — sem regressão nenhuma |
| Cache forçado a 8 slots | `D8 p26 b8 d0/1 c4096 2ms` — 8 ligadas, **26 recusas**, 1 espera que estourou o teto, 4096 leituras, 2 ms |
| Espera depois do vblank | `idle1` — a condição é atingível, os bits estão certos |
| Estado no instante em que a espera desistiu | `GXSTAT = 0x06000100` e **20 polígonos** na Polygon RAM |

A última linha é a descoberta da fase. A meio da lista, o FIFO já está vazio
(bit 26 ligado) e o motor de geometria ocioso (bit 27 limpo); o único termo que
falha é a contagem de polígonos, e o melonDS só esvazia a Polygon List RAM na
fronteira do frame. Ou seja: **não existe "o renderizador terminou este triân-
gulo" para a CPU no meio de um frame** — no emulador nunca, e no hardware real
só com a latência de até um frame inteiro de saída de scanline.

Isso também corrige uma nota de memória deste projeto, que dizia que o libnds
não expunha nem `glFinish` nem um bit de FIFO vazio. Expõe: `GFX_STATUS`
(`video.h:729`), `GFX_POLYGON_RAM_USAGE` (`video.h:776`) e
`glGetInt(GL_GET_POLYGON_RAM_COUNT)`. O que não existe é um `glFinish` que
espere o **rasterizador**.

### Validação

- Compilação limpa (`make`, sem aviso) e boot no melonDS em modo DSi.
- Captura `drain1.png` / `drain1_rows.png` (linhas 8-12 do console inferior) e
  `drain1_gfx.png` (linhas 17-22): as duas linhas do probe, o `Tex` de regime e
  o `DL t78 c0 p2 m9/7 x0 ok` — 78 triângulos, sendo 48 dos 24 quads novos.
- Captura `drain1_top2.png`: a risca de 24 cores distintas aparece inteira na
  faixa inferior da tela superior, nenhuma amostrada de textura errada. Isso é
  a prova visual de que a lista aninhada e o cache maior funcionam juntos.
- O caminho de recusa continua correto por construção: quando a espera estoura
  o teto, os uploads seguintes da mesma lista são recusados e contados, o triân-
  gulo sai sem textura, e nada de VRAM é reescrito por baixo de geometria
  enfileirada.

### Erros meus

- **A primeira versão do teto era uma contagem de polls (400 000).** No melonDS
  cada poll é uma armadilha de MMIO, então a espera queimou **236 ms de um frame
  de 16 ms**: `FPS 4`, `CPU submit 236.3 ms`, e o HUD mostrou o bug antes de eu
  pensar nele. Limitar por relógio e consultar o relógio de 1024 em 1024 polls é
  o que torna o custo independente da velocidade do emulador.
- **Eu tinha aceitado "16 slots" como se fosse um limite de hardware.** Não era:
  a VRAM cabe seis vezes aquilo, e o libnds entrega 2048 nomes. Um número da
  primeira tentativa virou requisito por inércia e só foi questionado quando o
  teste de 26 texturas existiu.
- **A sonda inicial não exercitava nada.** O `geDrainProbe` da primeira versão
  rodava a lista com o cache cheio de 64 e depois encolhia para 8 — com o cache
  já preenchido, toda lookup seguinte acertava as entradas antigas e o ramo de
  transbordo nunca era alcançado. Só deu para perceber ao reler o `ensureTex`:
  a busca percorre `texCacheCount`, não `texSlots`. Agora a sonda esvazia a
  tabela antes da rodada pequena (`texCacheCold`).
- **Gastei duas buscas na documentação errada** (`problemkaputt.de/nds3d.htm`
  devolve 404; o arquivo da GPU do melonDS foi renomeado de `PPU3D.cpp` para
  `GPU3D.cpp` e não tem um símbolo `GPUStatus` para grep). A tabela de bits que
  usei veio do espelho GBATEK no repositório de referência, e a confirmação
  final veio de ler o registrador na máquina, não da documentação.

### Limites

- O despejo a meio do frame **não é verificável no melonDS** — a contagem de
  polígonos só zera na fronteira do frame. A espera está certa por condição e
  barata por teto, mas "o hardware real libera os slots no meio da lista" é uma
  hipótese para a tarefa #5, que precisa de um DSi de verdade.
- 26 imagens não é uma sala do OoT. A lista de demonstração é sintética: o número
  real de texturas por sala só sai com o build de referência, que exige o ROM
  NTSC-U 1.0 (tarefa #6, bloqueada).
- Com 64 slots de RGBA16 128×128 no pior caso, o consumo passa da VRAM, e o
  backend hoje não tem contabilidade de bytes — só de entradas. Um asset grande
  pode estourar a VRAM antes de estourar a contagem.
- A recusa continua produzindo triângulo sem textura. É o comportamento certo
  (errado visível, não corrupção silenciosa), mas não é recuperável: a fase não
  tenta adiar o triângulo para o frame seguinte.

### Próximo

- Levar as duas linhas do probe para um DSi real na tarefa #5 e registrar se
  `GFX_POLYGON_RAM_USAGE` cai durante o frame. Se cair, o teto de 2 ms vira o
  caminho normal e a capacidade de 64 slots vira folga; se não cair, a espera
  deve ser removida e a política passa a ser "recusa e conta".
- Contabilidade de bytes na VRAM no lugar da contagem de entradas, antes de
  qualquer asset real do jogo.
- Ciclo 1 do combiner e fontes `ENV`/`PRIMITIVE`, que a lista de demonstração já
  emite e o backend ainda ignora.

---

## Fase 2 — Recorte paletado e a VRAM que não pode ser reescrita no mesmo frame (2026-10-27)

> **ID** `OTD-012` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-012/` · **Commit** árvore sem histórico git

**Objetivo:** no N64, uma entrada de paleta CI carrega um bit de alpha, e o OoT
usa isso em todo tecido com transparência (bandanas, penas, fumaça 2D, ícones de
magia): `cia4`/`cia8` são formatos comuns nos arquivos do jogo. O GE do DS tem
paleta de **15 bits, sem alpha nenhum** — a fase tinha que decidir o contorno e
medi-lo em pixels, não apenas registrar que o hardware não tem.

**Resultado secundário, e o que a fase valeu de fato:** a sonda que provava o
contorno expôs um bug de corrupção de VRAM no cache de texturas que estava
tornando **todo** o trabalho de paleta anterior invisível na tela.

### Entregue

- **`ge_draw.c` — três caminhos por upload paletado** (`GePalPath`, [:266](source/gfx/ge_draw.c#L266)):
  `palDecision` ([:272](source/gfx/ge_draw.c#L272)) varre os **índices realmente amostrados**
  da imagem (não a
  paleta inteira — asset do jogo traz cauda de TLUT sem uso) e escolhe
  `GE_PAL_PLAIN` (ninguém pede alpha: como era antes), `GE_PAL_COLOR0` (só o
  índice 0 pede: `GL_TEXTURE_COLOR0_TRANSPARENT` no param da textura, custo
  zero) ou `GE_PAL_EXPAND` (algum índice ≠ 0 pede: `texExpandCi` [:308](source/gfx/ge_draw.c#L308)
  horneia índice+paleta num buffer A1B5G5R5 e a textura sobe como cor direta).
- **`ge_draw.c` — o conserto da VRAM**: cada slot do cache agora guarda o
  `frame` que o preencheu, e a escolha de LRU **só considera slot de frame
  anterior**. Se todos estiverem pinados, o upload é **recusado** e contado em
  `texPinned` — o triângulo sai sem textura (errado visível) em vez de
  corromper geometria já enfileirada (errado silencioso).
  `GE_TEX_CACHE` 8 → 16.
- **`ge_draw.h`**: `GeTexSlot.frame`, `GeDraw.frameStamp`, `GeDraw.texPinned` e
  o parágrafo de documentação da regra.
- **`dl_demo.c`**: a risca de 4 texels com `pal4[3] = C_BLUE_XRAY` (alpha 0) no
  CI4 e a moldura de 4 texels com `pal8[0] = C_BLUE_XRAY` no CI8 são as duas
  sondas; o CI4 foi para o centro do painel (`mtx4 = (0, 0, -215)`) para que
  nenhum outro quad o tapasse.
- **`main.c:284`**: a linha 19 do HUD ganhou o contador de pinos —
  `Tex b<binds> h<hits> x<skip> e<evict> p<pinned> t<tris>/<pal> <on|off>`;
  a linha 21 reporta os dois caminhos paletados — `Cut 4/22 at1 e<expand> c<color0>`.

### Descobertas (medidas, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| Fechar o lote antes de reescrever um slot protege a geometria pendente (era o comentário do código) | **Não.** `batchEnd`/`batchBegin` só delimitam *latch* de atributo; nada drena o FIFO. O melonDS executa a lista de comandos no fim do frame, então o DMA da nova imagem chega **antes** dos triângulos antigos lerem a VRAM |
| O quad CI4 estava sendo desenhado com a paleta errada | **Não.** Ele estava sendo desenhado com **a imagem de outra textura**: com 10 texturas distintas e cache de 8, o 10º upload (`cut`) caiu no slot LRU que os triângulos do CI4 já referenciavam. O padrão observado era *exatamente* `initCut` — moldura `C_RED` em x<4∣∣x>27, xadrez verde/amarelo `((x>>2)+(y>>2))&1`, fendas `C_CLEAR` em `x%8==6 ∣∣ y%8==3`, bloco `C_BLUE` 4×4 em 14..17 |
| Trocar as cores da paleta mudaria o resultado na tela | **Não** — e é isso que denuncia o bug: `pal4[1]`/`[2]`/`[15]` foram trocados por vermelho/verde/amarelo e o capture saiu byte a byte idêntico. A paleta nunca chegou ao pixel, porque o buffer inteiro foi sobrescrito |
| Paleta CI pode carregar alpha | **Não.** `glColorTableEXT` escreve TLUT de 15 bits; o `TextureLookup` do melonDS (`GPU3D_Soft.cpp`) devolve alpha 31 para todo índice ≠ 0 e 0 para o índice 0 quando a flag `TexParam` bit 29 está ligada. Ou seja: só o **índice 0** tem recorte nativo |
| Horneiar CI para cor direta é caro sempre | Só quando um índice ≠ 0 pede alpha. CI4 horneado custa 4× o buffer de índices (2 KB vs 512 B num 32×32), CI8 custa 2×. O critério por **índice amostrado** deixa a cauda de TLUT sem uso do asset do jogo no caminho barato |

### Validação (melonDS 1.1, modo DS, 67 MHz — capture `ci11_top.png`, 2×)

Linha y = 192 atravessando o quad CI4, da borda esquerda à direita — 32 texels
com as larguras exatas de `initCi4`:

| x | cor medida | texel | esperado |
|---|---|---|---|
| 162–191 (30 px) | `#FF00FF` | índice 1, 5 texels | moldura magenta ✔ |
| 192–219 (28 px) | `#FF0000` | índice 2, 5 texels | campo vermelho ✔ |
| **220–243 (24 px)** | **`#867575`** | **índice 3, alpha 0, 4 texels** | **buraco** — é o vermelho α128/31 sobre a placa, medido na fase anterior. Placa pura ao lado vale `#00FFFF` ✔ |
| 244–261 (18 px) | `#FF0000` | índice 2, 3 texels | campo ✔ |
| 262–307 (46 px) | `#FFFFFF` | índice 15, 8 texels | mancha branca ✔ |
| 308–319 (12 px) | `#FF0000` | índice 2, 2 texels | campo ✔ |
| 320–347 (29 px) | `#FF00FF` | índice 1, 5 texels | moldura ✔ |

Contagem global de magenta no painel (`findcolor #FF00FF`): antes do conserto
`match=1872`, caixa só no quadradinho de teste em (428,262)–(481,301); depois
`match=14492`, caixa (162,114)–(481,301). O CI4 passou a existir na tela.

Linha y = 110 atravessando o quad CI8 (caminho `COLOR0`, sem horneamento):

| trecho | cor medida | leitura |
|---|---|---|
| x 386→406 | `#5555FF #5D5DFF #6565FF #6D6DFF #7575FF #7D7DFF` | rampa cinza+azul monotônica ✔ |
| x 410→460 | `#868600 … #DFDF00` (11 passos) | metade direita da rampa, monotônica ✔ |
| x 464–477 | **`#00B6FF`** | moldura de 4 texels do índice 0 **recortada** — é o azul α8/31 sobre a placa; ao lado, `#00FFFF` é a placa pura ✔ |

HUD do mesmo build: `Tex b0 h10 x0 e0 p0 t22/4 on` — no regime permanente as
10 texturas do demo **acertam** o cache, nenhum upload por frame, nenhuma
reciclagem, nenhum pino. `Cut 4/22 at1 e0 c0` (os contadores de caminho só
disparam no frame em que o upload acontece). Sem regressão: `F3 t4 mtx 1/1
0.9ms MtxOK`, `DL t30 c0 p2 m9/7 x0 ok`, `Bl 4/30 a8 n2 f11 p4 on`, 61 fps,
frame 16,7 ms.

**Efeito colateral medido:** com o cache grande o bastante para o frame, o
`CPU submit` caiu de **5,0 ms para 2,8 ms** — os 10 uploads por frame que o
demo vinha pagando viraram zero.

### Erros meus

1. **Validei a fase de paleta anterior no quad errado.** A medição de CI4 da
   fase passada olhou um magenta que era o quadradinho de teste nº 6, não o
   CI4; a risca horneada nunca tinha sido medida, e o `EXPAND` foi dado como
   funcionando sem estar visível. Duas fases de conclusão errada por uma
   medição mal localizada.
2. **Acreditei no meu próprio comentário.** "Fechar o lote protege o que está
   enfileirado" está no código desde a fase do cache e era falso; ele é
   exatamente o motivo do bug ter sobrevivido a três revisões.
3. **Comentário que mentia sobre o dado.** `mtx4` dizia `(-107,-57,-230)` e o
   valor era `0xFEE6` = **-282**. Toda a confusão de profundidade da fase
   anterior (três tentativas de mover a placa) partiu de confiar na descrição
   em vez de converter o número.
4. **Sonda errada primeiro.** Gastei dois ciclos de build trocando posição e
   cores de paleta antes de fazer o teste que responde sozinho: mover o quad e
   ver *qual* quadrado se move.
5. **Edição de doc que comeu uma linha.** Ao inserir o parágrafo da regra de
   VRAM em `ge_draw.h`, o `old_string` de duas linhas virou uma e a frase do
   OtherMode perdeu a cláusula `ZMODE other than OPA`. Mesma classe de erro da
   fase anterior (âncora curta em comentário); pega na releitura, não no
   compilador.

### Limites

- A regra de pino é uma troca: um frame que referencie **mais texturas
  distintas** que `GE_TEX_CACHE` perde as sobras (desenhadas sem textura,
  contadas em `p`). Uma sala real do OoT passa de 16 com folga. As duas saídas
  honestas são (a) dimensionar o cache pelo orçamento de VRAM — 656 KB, um
  64×64 A1B5G5R5 custa 8 KB — ou (b) drenar o FIFO no meio do frame antes de
  reciclar, o que exige ler `0x04000304` direto porque **o libnds não expõe
  `glFinish` nem o bit de FIFO vazio**. Nada disso está medido ainda.
- O horneamento CI→direto não preserva *dither* nem `G_TT_*` do fetch: ele só
  existe para o recorte. Um asset que precise de ciclo 1 continua no caminho
  paletado barato.
- `palDecision` varre a imagem inteira a cada upload. No demo é 1024 texels;
  numa textura 128×128 são 16 K leituras por upload — aceitável enquanto o
  cache acerta, a medir quando o jogo começar a errar.
- Ciclo 1, fontes ENV/PRIMITIVE, IA4/IA8, texturas retangulares e display list
  real do jogo continuam não cobertos / bloqueados pelo ROM NTSC-U 1.0.

---

## Fase 2 — Othermode, blending por polígono e a ordem real do `POLY_FORMAT` (2026-10-27)

> **ID** `OTD-011` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-011/` · **Commit** árvore sem histórico git

**Objetivo:** a água do lago Hylia, a névoa do Dodongo, a magia, as sombras
 Projetadas e todo fade do OoT são materiais desenhados com render mode
translúcido (`G_SETOTHERMODE_L` com `G_ZMODE_XLU` ou `G_BL_ENABLE`), e o fator de
mistura vem do **alpha do vértice** (`Ad0 = G_ACMUX_SHADE` do `CC_ModulateShade`).
Até aqui o tradutor jogava OtherMode fora e desenhava tudo opaco: os fades
sumiam e a água tapava o fundo. O alvo da fase era fechar o caminho
OtherMode → `POLY_ALPHA` + `POLY_ID` com **medição em pixels**, não com
"apareceu cor diferente".

### Entregue

- **`f3dex2.h` / `f3dex2.c`**: sombra de OtherMode. `G_SETOTHERMODE_H` e `_L`
  escrevem `len` bits de uma metade já deslocada num par de registradores
  acumulados; `omFlags()` ([source/gfx/f3dex2.c:101](source/gfx/f3dex2.c#L101)) destila a palavra em
  seis flags que o tradutor realmente consome — `F3_OM_XLU`, `F3_OM_DECAL`,
  `F3_OM_KEY`, `F3_OM_2CYC`, `F3_OM_TTALPHA`, `F3_OM_FILT` — e o hook novo
  `f3SetOmHook` entrega a sombra, as flags e o `G_AC_MODE` no material.
- **`ge_draw.c`**: `onOm` ([:447](source/gfx/ge_draw.c#L447)) guarda flags e alpha de
  combiner; `onTri` ([:535](source/gfx/ge_draw.c#L535)) monta
  `POLY_ALPHA(alpha) | POLY_CULL_NONE | POLY_ID(id)` — alpha = `vtx[a].a >> 3`
  quando o material pede `F3_AC_SHADE`, senão 31; `POLY_ID` numérico corrido
  1..63 (`0` é o ID do fundo). A palavra só é escrita quando mudou, e a mudança
  **fecha o lote, escreve e reabre**. `GE_FMT_OPAQUE` ([:533](source/gfx/ge_draw.c#L533)) é restaurado no fim da
  lista ([:663](source/gfx/ge_draw.c#L663)).
- **`ge_draw.c:171` `toDsColor`**: converte **A1R5G5B5 (N64) → A1B5G5R5 (DS)** —
  o bit 15 fica onde está e R/B trocam de lugar. É a correção de um bug real do
  port, e ela é condição do blending: o GE grava o alpha do pixel como
  *texel × polígono*, e um pixel que chega com alpha 0 é descartado pelo alpha
  test e nunca mais é misturado de novo. `palConvert` ([:254](source/gfx/ge_draw.c#L254)) usa a mesma
  função, então a paleta CI saiu junto do conserto.
- **`dl_demo.c`**: sonda de blending autorrespondida. Uma **placa ciano opaca**
  atravessa o painel superior inteiro e, sobre ela, dois quadrados translúcidos —
  vermelho com alpha 128/255 (→16/31) à esquerda, azul com 64/255 (→8/31) à
  direita. A placa torna o **destino** de cada pixel misturado conhecido, então
  cada metade é leitura direta do alpha que o GE recebeu. Cores do demo e da
  paleta passaram às macros `C_RED`/`C_BLUE_XRAY`/… escritas na ordem N64. A
  lista continua com 296 palavras e 11 comandos na área de blending.
- **`main.c:312`**: HUD linha 22 `Bl <xlu>/<tris> a<alpha> n<omChanges> f<flags> p<polyChanges> on`.
- **`f3dex2_selftest.c`**: 6 transições de OtherMode com as flags esperadas e o
  `G_AC_THRESHOLD` (gate de alpha do `CK_KEY`) verificado por nome.

### Descobertas (medidas, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| `glPolyFmt` é um comando de FIFO e vale por triângulo | **Não.** `videoGL.h:774` escreve o registrador mapeado (`GFX_POLY_FORMAT = params`), e o melonDS faz o *latch* de `CurPolygonAttr` **no `glBegin`** (`GPU3D.cpp:2308-2314`), não na palavra 0x29 (`GPU3D.cpp:2230` só atualiza `PolygonAttr`; `:1257` copia `CurPolygonAttr` para a polígono). Alpha/ID por triângulo **exige fechar e reabrir o lote** |
| Fechar o lote basta | Fechar sem reabrir derruba **todo vértice seguinte**, em silêncio. O HUD continuava `Bl 4/30 … p11` saudável por três builds enquanto a placa e as metades tinham sumido da tela |
| O alpha do texel já chegava ao GE | **Não**: `toDsColor` lia o alpha do **bit 0** e deslocava R de 11. Todo texel RGBA16 e toda entrada de paleta tinham "azul ímpar = opaco". Os recortes antigos passaram por sorte (`0x003E` tem bit 0 limpo) |
| Blending precisa de `GL_BLEND` | No renderer de software do melonDS a translucidez vem só de `POLY_ALPHA` 1..30 (`GPU3D.cpp:1268`); `GL_BLEND`/`glBlendFunc` não participam do caminho |
| O destino do pixel é irrelevante | **Relevante, duas vezes**: `GPU3D_Soft.cpp:830-858` — se `dstalpha == 0` o fonte é devolvido **puro**, sem mistura; e `:990` — `poly_id` igual ao já armazenado descarta o pixel translúcido. Daí a placa opaca e o ID corrido |
| Alpha de texel paletado existe | Confirmado de novo: `TextureLookup` dos casos 2/3/4 dá alpha 31 a qualquer índice ≠ 0 (só o índice 0 herda o `alpha0` de `G_TT_*`) |

### Validação (melonDS 1.1, **DS mode**, 67 MHz, `[3D] Renderer = 0`)

| Teste | Resultado |
|---|---|
| Pixel sobre a placa, vermelho α16/31 | previsto `#847373`, medido **`#867575`** → 53 % do fonte no lugar (a aritmética do emulador é `src·17 + dst·15 >> 5` com `alpha++`) |
| Pixel sobre a placa, azul α8/31 | previsto `#00B5FF`, medido **`#00B6FF`** → 28 % do fonte |
| As mesmas metades **fora** da placa | `#860075` e `#9E0C8A` — o fonte atravessa quase inteiro porque o fundo tem alpha 0. Prova de que a conta depende do destino, exatamente como o código lido |
| Costura das metades (1 px de borda comum) | `#C33434` / `#0082FF`: dois translúcidos de `POLY_ID` diferentes no mesmo pixel, o esperado, não um buraco |
| HUD | `Bl 4/30 a8 n2 f11 p4 on` — 4 triângulos translúcidos dos 30, alpha final 8 (a última camada desenhada), 2 transições de OtherMode, flags `0x11` = `XLU\|TTALPHA`, 4 lotes reabertos |
| Sem regressão | `F3 t4 mtx 1/1 0.9ms MtxOK` · `DL t30 c0 p2 m9/7 x0 ok` · `Tex b10 h0 x0 e10 t22/4 on` · `CC S a4 n6 v1 0-255,24-16…` · `Cut 4/22 test1 thr1` |
| Desempenho | 61 fps, 16,7 ms/quadro, submissão 4,4 ms (pior 26 ms) — os lotes extras custam ~0,1 ms; `Logic 20 Hz / 185 ticks` |

Capturas: `%TEMP%\ordsi\bl9_top.png` e `bl10_top.png` (painel superior 2×),
`bl10_hud.png` (HUD 2×) — build final. O varredor `scanrow.ps1 -In bl10_top.png
-Y <linha>` é o que produziu os hex acima.

### Erros meus que a inspeção pegou

- **A `PROBE-A`** (forçar alpha 8 em todo triângulo) estava no `onTri` e dava a
  impressão de blending de ponta a ponta. Só provava que translúcidos compostos
  entre si aparecem. Removida; a sonda da placa é a que responde.
- Primeira edição das cores do demo com palavras **na ordem do DS**, quando o
  loader espera A1R5G5B5: corrompeu o matiz. Revertida e refeita pelas macros.
- Teoria "o destino tem alpha 0 porque o texel é opaco" durou até a leitura de
  `toDsColor` mostrar que todo texel já chegava com A=1 pelo caminho errado.
- Comentários em `ge_draw.h` e `ge_draw.c` afirmavam que `glPolyFmt` entrava na
  FIFO ordenada. Errado; corrigidos junto com o código.
- Armadilha evitada: uma quarta camada translúcida faria `cmds` passar de 296 e
  deslocaria as 27 constantes `OFF_*` da display list. A placa **substituiu** uma
  das três camadas, e a contagem de comandos não mudou.

### Limites / o que fica para a próxima

- Age apenas o bit `F3_OM_XLU`. `G_ZMODE_DEC` e `G_BL_ENABLE` separados são
  reconhecidos nas flags mas ainda não têm comportamento próprio, e `F3_OM_FILT`,
  `F3_OM_KEY` e `F3_OM_2CYC` ficam marcados sem efeito.
- **Alpha por triângulo** (o `a` do primeiro vértice), não interpolado pelos
  três. Fades de OoT são uniformes, então serve; gradientes translúcidos, não.
- `POLY_ID` reinicia a cada `geDrawList`: sobreposição entre duas listas do mesmo
  quadro não é protegida. A ordenação por material (XLU por último) é o próximo
  passo natural quando o jogo existir.
- **Nada disso foi visto em DSi real** (tarefa 5). O modelo do blend é o do
  renderer de software do melonDS; o hardware tem pipeline próprio.
- **Próximo:** paleta CI expandida em `A1B5G5R5` na upload quando
  `F3_OM_TTALPHA` estiver ligado, para as folhas e penugens CIA8/CIA4; ciclo 1;
  fontes `ENV`/`PRIMITIVE`; e a primeira display list do decomp (bloqueada pela
  ROM NTSC-U 1.0 do build de referência).

---

## Fase 2 — Alpha do texel como recorte (`GL_ALPHA_TEST`) (2026-10-27)

> **ID** `OTD-010` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-010/` · **Commit** árvore sem histórico git

**Objetivo:** cercas, grama, penugens e mechas de cabelo do OoT são texturas com
texels de alpha 0 num material cujo próprio alpha vem do texel
(`Ad0 = G_ACMUX_TEXEL0`, o `CC_DecalRGBA` da vida). Sem ler isso, cada um desses
materiais sairia como um retângulo sólido no DS. Como blending ainda não existe,
o alvo é o **recorte**: o texel transparente não é desenhado e o que está atrás
aparece.

### Entregue

- **`video_top.c`**: `glEnable(GL_ALPHA_TEST)` + `glAlphaFunc(1)` uma vez, no
  `topInit`, globais. `DISP3DCNT` não é ordenado com a FIFO, então ligar/desligar
  por material exigiria fechar o lote em cada `G_SETCOMBINE`; com um único
  threshold o corte é "alpha 0 do A1B5G5R5 fora, resto dentro".
- **`f3dex2.h` / `f3dex2.c`**: enum `F3_AC_*` com os valores exatos de
  `G_ACMUX_*` do `gbi.h` do decomp (`COMBINED 0`, `TEXEL0 1`, `SHADE 4`, `1 6`,
  `0 7`); `decodeCC` passou a comparar e documentar por nome, e o autoteste
  também.
- **`ge_draw.c`**: contador `cutTris` — triângulos que amostraram a cena
  (`texLive`) **e** cujo alpha do ciclo 0 é o texel. Zera por quadro junto com o
  resto das estatísticas.
- **`dl_demo.c`**: textura RGBA 32×32 nova — moldura vermelha opaca de 4 texels,
  xadrez amarelo/verde de 4×4, núcleo azul 4×4 e fendas de alpha 0 a cada 8
  texels nas duas direções — desenhada no centro-alto da tela, em `z = -240`,
  na frente do quad grande e do CI8, com `G_SETCOMBINE` DECALRGBA. A lista foi
  para 242 palavras e 48 vértices na ROM; o `G_VTX` novo continua dentro do
  buffer de 32 do RSP porque escreve nos slots 20..23, que já estavam livres.
- **`main.c`**: HUD linha 21 `Cut %lu/%lu test%u thr%u`.
- **`video_top.h/.c`**: `topAlphaTest()`, que devolve o bit de enable lido de
  `GFX_CONTROL` e o threshold gravado.

### Descobertas (medidas, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| Ler `GFX_ALPHA_TEST` (0x04000340) de volta prova o threshold | Não: ele lê **0** no melonDS enquanto o recorte está visivelmente acontecendo. `GFX_CONTROL` lê normal. O HUD mostra o bit lido e o valor escrito, e a legenda diz qual é qual |
| Só o material novo contaria como recorte | `Cut 4`, não `Cut 2`: o DECALRGBA do terceiro quad do demo **já** era `Ad0 = TEXEL0`. Todo DECAL é recorte por definição — a contagem estava certa, a expectativa é que estava errada |
| Recorte precisaria de `GL_BLEND` ou de polígono por material | Não: com `GL_ALPHA_TEST` ligado o GE descarta o fragmento antes de escrever, então a geometria atrás sobrevive sem nenhuma mudança de pipeline |

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| HUD linha 21 | `Cut 4/22 test1 thr1` — 4 dos 22 triângulos texturizados são de material texel-alpha, teste de alpha **habilitado no registrador**, threshold 1 |
| Contadores vizinhos | `DL t24 c0 p2 m9/7 x0 ok` (2 triângulos novos, pilha equilibrada), `Tex b10 h0 x0 e10 t22/4 on` (uma textura a mais, despejo LRU igual), `CC D a1 n5 v1 0-255,24-167` (modo final DECAL, alpha 1 = TEXEL0, 5 mudanças de combiner) |
| Autoteste do decodificador | `F3 t4 mtx 1/1 1.0ms MtxOK` sem `FAIL`, agora comparando os alphas por nome (`F3_AC_SHADE` / `F3_AC_TEXEL0` / `F3_AC_COMBINED`) |
| Recorte visível | `cut0_zoom.png` (6×): pelas fendas de 1 texel aparecem o vermelho e o azul da textura do quad grande **atrás** e a diagonal preta dele, em traço interrompido; a moldura vermelha, o xadrez e o núcleo azul seguem opacos |
| Sem regressão | Os seis quadradinhos de 8×8, o CI4 (moldura magenta), o CI8 (rampa), o SHADE (gradiente de vértice com textura ligada) e o DECAL antigo continuam todos corretos em `cut2_top.png` |
| Desempenho | 60 fps, 16,7 ms/quadro, submissão 4,3 ms contra 3,8 ms antes — o custo é a décima upload por quadro, não o alpha test |

Capturas: `%TEMP%\ordsi\cut0.png` (quadro 1×), `cut0_zoom.png` (o quad de
recorte em 6×), `cut2.png`, `cut2_top.png` e `cut2_hud.png` (3×) — o build
final, já com `thr1`.

### Medida de apoio: a paleta CI não carrega alpha

Para a penugem e as folhas do OoT, que são CIA8/CIA4 com o bit de alpha na
paleta, ligamos o alpha test e trocamos uma entrada da paleta do quad CI4 do
demo por **azul com o bit de alpha zerado** (`0x003E`), abrindo uma faixa de 4
texels dentro do campo vermelho. Controle deliberado: cor visível *e* alpha 0,
para que "não recortou" e "a faixa não existe" não se confundam (a primeira
versão do teste usava o mesmo vermelho e era ambígua — `pal1_ci4.png`).

Resultado em melonDS 1.1, DS mode: a faixa saiu **azul opaca** (`pal2_ci4.png`,
6×), com o serrilhado de 1 texel nas bordas que confirma a ordem dos nibbles. O
alpha da paleta não chega ao GE — bate com o doc do libnds (`glColorTableEXT`
carrega paleta de 15 bits) e com o GBATEK, onde texel paletado no 3D tem alpha
sempre cheio.

**Consequência para o motor:** recorte paletado exige expandir índice + paleta
para texéis A1B5G5R5 na upload, o que custa 2× a VRAM de um CI8 e 4× a de um
CI4, e hoje cada variante de paleta já ocupa um slot próprio no cache (a chave
inclui `tlut`), então o custo é real, não amortizado. A decisão fica para a
próxima fase junto com o orçamento de VRAM por bytes, porque expandir sem critério
abriria buraco em material não-recorte cujo asset tem entrada de alpha 0
espúria — o mesmo problema já listado para RGBA.

### Erros meus que a inspeção pegou

- Uma edição ancorada em `glAlphaFunc(1);` **juntou a chamada com
  `glClearColor(...)` na mesma linha** em `video_top.c`. É a terceira vez nesta
  obra que uma âncora curta engole a linha seguinte; a leitura antes de compilar
  pegou.
- `crop_any.ps1` pediu `[System.Drawing.Imaging.PixelOffsetMode]` — o tipo mora
  em `Drawing2D`. O arquivo saía salvo mesmo com a exceção, o que quase passou
  um crop errado como bom.
- Enviar `Ctrl+R` por SendKeys para o melonDS em segundo plano não fez nada e o
  HUD seguiu em `thr0`; quase tirei conclusão do resultado velho. Trocar o ROM e
  reiniciar o processo é o caminho reproduzível aqui.

### Limites / o que fica para a próxima

- Recorte hoje só existe em **RGBA5551**. Medido acima: a paleta CI não tem
  canal de alpha no GE, então um texel CI4/CI8 chega sempre opaco. Para a
  penugem e as folhas do OoT (CIA8) é preciso expandir índice + paleta em
  A1B5G5R5 na upload, com o custo de VRAM que isso tem.
- Threshold único: sem `G_SETOTHERMODE_H` não dá para distinguir o key gate do
  RDP em outros níveis, nem misturar recorte com blending parcial.
- Um material que **ignora** o alpha do texel mas carrega texel de alpha 0
  espúrio vai abrir buraco. A correção é no bake do asset (forçar alpha opaco
  quando o material não é recorte), não no tradutor.
- **Próximo:** `G_SETOTHERMODE_H` (mix de alpha, fog, blend), blending real com
  `POLY_ALPHA` + `GL_BLEND`, expansão de índice + paleta em A1B5G5R5 para
  material de recorte, ordenação de triângulos por textura e a primeira DL do
  decomp (bloqueada pela ROM NTSC-U 1.0 para o build de referência).

---

## Fase 2 — Combiner (G_SETCOMBINE) e viewport (G_MOVEMEM) (2026-10-27)

> **ID** `OTD-009` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-009/` · **Commit** árvore sem histórico git

**Objetivo:** até aqui o tradutor ignorava `G_SETCOMBINE` e `G_MOVEMEM`, então
toda geometria saía como *texel × cor de vértice* e no quadro inteiro. O OoT
escolhe o modo por material (MODULATE, DECAL, SHADE) e define a área de
renderização com um viewport em espaço N64; sem isso o 2D do jogo (HUD,
texturas de tela) não tem como casar com o painel do DS.

### Entregue

- **Decodificação de `G_SETCOMBINE`** (`f3dex2.c` → `decodeCC`): extrai os muxes
  do ciclo 0 (a0/b0 em 4 bits, c0 em 5, d0/Aa0/Ab0/Ac0/Ad0 em 3) e classifica em
  `F3_CC_MODULATE`, `F3_CC_DECAL`, `F3_CC_SHADE` ou `F3_CC_OTHER`, mais a fonte
  de alpha do ciclo 0. Contador `ccChanges` e hook `f3SetCCHook`.
- **Captura de `G_MOVEMEM` VIEWPORT** (`F3_OP_MOVEMEM`, índice 8 no
  `F3DEX_GBI_2`): resolve o endereço por segmento, copia os 8 `s16` de `Vp_t`
  (scale[4] + trans[4], 2.2) para `st->vp`, incrementa `vpLoads` e dispara
  `f3SetVpHook`. Antes o comando caía no mesmo `case` de `G_MOVEWORD` e era
  descartado.
- **Mapeamento para o pipeline fixo do GE** (`ge_draw.c`): o DS não tem
  combiner programável, então as três formas viram estados do que existe.
  MODULATE é o nativo; DECAL força `glColor3b(255,255,255)` nos vértices para a
  modulação devolver o texel puro; SHADE liga a textura branca de 8×8 para que
  o texel neutro deixe só a cor de vértice. A escolha passou por
  `liveTexId`/`syncTex`, chamados tanto em `onTex` quanto em `onCC`.
- **Viewport N64 → DS**: `trans ± scale` divididos por 4 (2.2 → pixel N64),
  escalados por 4/5 (320×240 → 256×192), canto inferior direito menos 1 porque
  o `glViewport` do libnds é inclusivo, e clampado em 0..255 / 0..191. O lote é
  fechado em volta da escrita.
- **Contador de triângulos texturizados corrigido**: `texTris` media
  "G_TEXTURE ligado", que passaria a contar errado com SHADE. Agora mede
  `texLive`, isto é, "o triângulo realmente amostrou a cena".
- **DL de demonstração**: `G_MOVEMEM` com viewport 320×180 (letterbox), mais
  quatro `G_SETCOMBINE` — MODULATE antes do quad grande, SHADE antes do segundo
  (que continua com `G_TEXTURE` **ligado**), DECAL antes do terceiro e
  MODULATE de volta antes dos CI. Os vértices do terceiro quad ganharam UV
  0..1024 para que o DECAL tenha o que mostrar.
- **Autoteste do decodificador** (`f3dex2_selftest.c`): bloco novo que roda
  `G_MOVEMEM` + os três pares de palavras reais de `gDPSetCombineLERP` e um
  quarto não classificável, conferindo `vpLoads`, `ccChanges`, os campos do
  viewport e a sequência de modos/alphas via hooks.
- **HUD linha 20 nova**: `CC %c a%lu n%lu v%lu %u-%u,%u-%u`.

### Descobertas (medidas, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| Comparar os muxes com os enums de `gbi.h` (`G_CCMUX_0 = 31`) | Os campos **a0/b0 têm 4 bits e d0 tem 3**, então 31 chega truncado como `0xF` e `7`. A classificação só fecha contra o valor truncado — comparar com 31 nunca casa |
| `G_TEXTURE` desligado é o que produz geometria só com cor de vértice | Não: SHADE faz isso **com a textura ligada**. Por isso o teste tem que manter `G_TEXTURE` on e ainda assim mostrar o gradiente — foi o que separou os dois caminhos |
| Escrever `GFX_VIEWPORT` no meio do quadro afetaria tudo que ainda não foi rasterizado | No melonDS a escrita pegou só a geometria submetida depois: a grade (desenhada antes) continuou em tela cheia e a DL do demo entrou na faixa 24..167. Fechamos o lote mesmo assim, porque a ordem não é garantida pela API |
| O terceiro quad do demo já servia para testar DECAL | Ele tinha `s = t = 0` em todos os vértices: com DECAL amostraria um único texel e o teste não distinguiria nada. Precisou de UV cheio |

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Autoteste do decodificador com os casos novos | `F3 t4 mtx 1/1 0.9ms MtxOK` — sem `FAIL`, ou seja, os 4 modos classificados, os 4 alphas e o viewport bateram |
| HUD linha 20 | `CC M a4 n4 v1 0-255,24-167` — modo final MODULATE (o último `SETCOMBINE` antes dos CI), alpha SHADE, 4 mudanças de combiner, 1 carga de viewport, retângulo exatamente o letterbox calculado |
| SHADE com textura ligada | Segundo quad continua o gradiente de 4 cores de vértice (antes ele dependia de `G_TEXTURE` off) |
| DECAL | Terceiro quad passou de gradiente azul/magenta (build anterior, `ev4_top.png`) para os quatro quadrantes da textura RGBA com o marcador branco (`cc1_top.png`) |
| Viewport | Extensão vertical do quad grande: linhas ~2..183 → ~28..163, o fator 144/192 = 0,75 do letterbox |
| Contadores | `DL t22 c0 p2 m8/6 x0 ok` inalterado; `Tex b9 h0 x0 e9 t20/4 on` — `t` subiu de 18 para 20 porque os 2 triângulos do DECAL agora amostram textura e os 2 do SHADE não contam mais |
| Desempenho | 60 fps, 16,7 ms/quadro, submissão 3,8 ms (3,7 ms antes), pior 27 ms |

Capturas: `%TEMP%\ordsi\cc1.png` / `cc2.png` (quadro completo),
`cc1_top.png` e `cc2_top.png` (tela superior 2×), `cc1_hud.png` (linhas
13..20 em 3×), `cc1_perf.png` (fps e submissão), `ev4_top.png` (referência do
build anterior, para o antes/depois de SHADE/DECAL).

### Erros meus que a inspeção pegou

- Uma edição ancorada em `static bool isRdpOp(u32 op)\n{\n` **juntou a chave com
  o `switch`** (`{    switch (op) {`). Só a leitura do arquivo antes de compilar
  pegou; é o segundo incidente do tipo nesta sessão com âncoras curtas demais.
- O plano inicial era testar DECAL no segundo quad, que não tinha UV útil. Ter
  lido os vértices do demo antes de escrever os comandos teria economizado uma
  reescrita.

### Limites / o que fica para a próxima

- Só o **ciclo 0 de RGB** é classificado; ciclo 1 (detail/multitextura), key
  blend, fontes ENV e PRIMITIVE caem em `F3_CC_OTHER` e hoje viram MODULATE. O
  OoT usa `G_CC_MODULATEIA_PRIM`, `G_CC_ADD_TEX`, `G_CC_ENVIRONMENT` e companhia
  — falta a tabela de formas reconhecidas e o que fazer com cada uma.
- O **alpha está decodificado mas não aplicado**: `glColor3b` não tem alpha por
  vértice e `GL_BLEND` não foi ligado; translucidez N64 (mistura por alpha do
  texel, `G_BLLENV`) ainda não existe.
- Viewports múltiplos por quadro: o demo exercita um só. O HUD do OoT troca de
  viewport várias vezes e cada troca custa um `glEnd`/`glBegin`.
- Falta validar em DSi real (o emulador não mede contenção de VRAM/DMA nem
  garante a ordem de `GFX_VIEWPORT` com a FIFO).
- Ordenar triângulos por textura (runs) continua sendo a alavanca de cache.

---

## Fase 2 — Despejo do cache de textura (LRU) (2026-10-27)

> **ID** `OTD-008` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-008/` · **Commit** árvore sem histórico git

**Objetivo:** o cache tinha 8 slots fixos e nenhuma substituição; a nona
textura de um quadro caía em `texSkip`. O jogo alterna dezenas de texturas por
quadro, então era preciso reciclar slots sem deixar geometria pendente
apontando para a textura sobrescrita.

### Entregue

- **Despejo LRU** (`ge_draw.c`): cada slot ganhou `stamp` e o `GeDraw` um
  `texClock` que **atravessa quadros**; no acerto o stamp é renovado, no miss
  com cache cheio varre-se os 8 slots e recicla-se o menor stamp. `boundId` é
  invalidado para o `bindTex` não achar que a textura antiga ainda está ligada.
- **Batch fechado em volta do upload**: o DMA da textura não é ordenado com a
  FIFO de geometria, então `ensureTex` fecha o `glBegin` antes de subir a
  imagem e reabre depois — nada que já estava na fila fica apontando para um
  slot reciclado.
- **Sem realocar o objeto de textura**: o id do slot reciclado é **reaproveitado**
  e o `glTexImage2D` sobrescreve o conteúdo; `glGenTextures` só acontece no
  preenchimento inicial. Medido: 4,7 → 3,7 ms de submissão por quadro.
- **HUD linha 19**: ganhou o campo `e` (despejos do quadro):
  `Tex b%lu h%lu x%lu e%lu t%lu/%lu`.
- **DL de demonstração**: 6 quads 8×8 sólidos (vermelho, verde, azul, amarelo,
  ciano, magenta) em `z = -250`, somados a RGBA16 + CI4 + CI8 = **9 texturas
  distintas** num cache de 8 — a nona obriga o despejo todo quadro.

### Descobertas (medidas, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| Com 9 texturas o cache faria 1 despejo por quadro | **Não com acesso cíclico**: a LRU expulsa exatamente o item que será pedido em seguida, então o padrão repete 9 falhas e 9 despejos por quadro. Varredura cíclica é o caso patológico da LRU; o teste prova corretude do despejo, não taxa de acerto |
| `glDeleteTextures` + `glGenTextures` por reciclagem | Funciona, mas custa ~1 ms/quadro a mais. Com o lote fechado, reutilizar o id e reenviar os dados é seguro e mais barato |
| `F3_VTX_MAX 32` podia ser aumentado para caber o demo | **32 é o buffer real de vértices do RSP**. O correto era o demo emitir `G_VTX` de 4 vértices em `v0 = 0` por quad, como o jogo faz — não afrouxar o decodificador |

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Lista completa (11 quads, 9 texturas, 8 matrizes, 6 pops) | `DL t22 c0 p2 m8/6 x0 ok` — 22 triângulos, nenhum clamp, pilha balanceada |
| HUD linha 19 | `Tex b9 h0 x0 e9 t18/4 on` — 9 uploads, 9 despejos, 0 skip, 18 triângulos texturizados (9 quads), 4 indexados |
| Cores sob reciclagem total | Os seis quads 8×8 aparecem com a cor própria (verde/azul/ciano visíveis inteiros; os demais atrás da grade girando) mesmo com todos os slots reciclados no quadro |
| Desempenho | 60 fps, 16,7 ms/quadro, submissão 3,7 ms, pior 27 ms |

Capturas: `%TEMP%\ordsi\ev4.png` (cena), `ev3_six.png` (quads 8×8),
`ev2_hud.png` (contadores em 8×).

### Erros meus que a inspeção pegou

- A DL nova usava **44 vértices num único `G_VTX`**, acima do limite de 32 do
  RSP: o parser abortava no quarto quad minúsculo, a pilha ficava com um nível
  sobrando e o HUD mostrava `t16 … m8/5 x1 FAIL`. Passei dois ciclos
  desconfiando do cache antes de ampliar os contadores — o `t16` contra os 22
  triângulos esperados era a pista.
- Li o HUD num zoom que cortava a linha (`b9 h0 e0`) e tratei o `e0` como bug
  do despejo; a linha certa estava poucos pixels abaixo e o cache nunca estava
  cheio, porque o parser morria antes da sétima textura.
- Comecei com cursor round-robin; o resultado neste teste é o mesmo da LRU
  (o acesso é cíclico), mas a LRU é a política que o resto do cache usa.

### Limites e pendências

- O cache é limitado a **8 slots**, não a bytes de VRAM: um cenário real com
  dezenas de texturas por quadro vai precisar de orçamento em bytes e/ou atlas
  de 128×128, não de mais slots soltos.
- O teste é sintético e cíclico: mede corretude do despejo, não a taxa de
  acerto de uma cena do jogo.
- O custo de 3,7 ms só aparece quando o conjunto de trabalho excede o cache;
  ordenar triângulos por textura (runs) é a alavanca seguinte.
- Falta validar em DSi real (o emulador não mede contenção de VRAM/DMA).

---

## Fase 2 — Texturas CI4/CI8 com paleta (2026-10-27)

> **ID** `OTD-007` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-007/` · **Commit** árvore sem histórico git

**Objetivo:** cobrir os formatos indexados, que são a maior fatia das texturas
de OoT. Até aqui só RGBA16 virava textura no DS; CI4/CI8 exigem capturar
`G_LOADTLUT`, guardar a paleta por slot de TMEM e casar o índice do tile de
render com a `glColorTableEXT` do libnds.

### Entregue

- **`G_LOADTLUT` no decodificador** (`f3dex2.c`): a contagem vem de
  `(w1 >> 14) + 1` (o `gDPLoadTLUTCmd` grava `count - 1`, 10 bits) e a origem é
  o endereço que o `G_SETTIMG` corrente apontava. O slot sai do TMEM do
  **tile 7** (`G_TX_LOADTILE`), que o `gDPLoadTLUT_pal16` preenche com
  `256 + pal * 16`; `G_SETTILE` do tile 7 passou a guardar esse TMEM
  (`loadTmem`). O resultado fica em `F3State.tluts[16]` (`{img, count}`) e é
  copiado para `F3Tex.tlut/tlutCount` no `G_SETTILESIZE`, lendo o campo
  `palette` do tile de render (`w1` bits 20-23).
- **Conversão** (`ge_draw.c`): CI4 vira `GL_RGB16` (16 cores, 4bpp) e CI8 vira
  `GL_RGB256` (256 cores, 8bpp); em ambos os casos o binário vai para
  `glTexImage2D` e a paleta para `glColorTableEXT(0, 0, count, 0, 0, pal)`
  **depois** do upload da imagem, com a textura ainda ligada, que é como o
  ngl associa paleta ao objeto de textura.
- **Swap de nibble no CI4** (`texConvertCi4`): o N64 guarda o texel da
  esquerda no **nibble alto**; o DS lê o da esquerda no **nibble baixo**. Um
  `((b << 4) | (b >> 4))` por byte resolve.
- **Paleta convertida igual ao texel**: entrada RGBA5551 → A1B5G5R5 com o
  mesmo `toDsColor`, ou seja, troca de canais mas nenhum outro tratamento.
- **Cache por textura + paleta**: a chave passou a ser
  `{img, w, h, tlut, tlutCount}` (tlut zerado para formatos diretos), então a
  mesma imagem com duas paletas diferentes ocupa dois slots — que é o caso
  real de `gDPLoadTLUT_pal16` trocando cor de NPC/ambiente.
- **Bancos de VRAM**: `vramSetBankE(VRAM_E_TEX_PALETTE)` no `topInit`; A e B
  continuam só com textura (256 KB) e o I segue com a tela de baixo.
- **Contadores**: `palBinds` (uploads que também carregaram paleta),
  `palTris` (triângulos com textura indexada) e `texPal`; a linha 19 do HUD
  virou `Tex b%lu h%lu x%lu t%lu/%lu`.
- **DL de demonstração** (`dl_demo.c`): dois quads novos — CI4 na metade
  inferior esquerda via **slot 5** e CI8 na superior direita via **slot 0**
  (256 entradas) — com `SETTIMG`/`SETTILE`/`LOADTLUT` próprios antes de cada
  um. O `cmds` foi para 128 palavras e os offsets foram refeitos; as 20
  posições de vértice ficam contíguas em 0x200.

### Descobertas (medidas no emulador, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| Dá para confiar no `siz` do `SETTIMG` para saber o formato | **Não**: o `gDPLoadTextureBlock` sempre emite `G_IM_SIZ_16b` nesse comando (é o tamanho do *bloco*); o formato real é o `fmt`/`siz` do tile de render do `G_SETTILE` |
| Ordem de nibble do DS poderia ser a mesma do N64 | **Falsa**: sem o swap, a borda de largura ímpar (5 texels) e o patch branco em texel ímpar saem serrilhados/deslocados; com o swap saem retos, medidos em pixels (borda 11-12 px = 5 texels, patch exatamente nos texels 17..24) |
| Paleta em formato próprio | É o **mesmo** A1B5G5R5 do texel; magenta `0xF81F` sai magenta, branco `0xFFFF` sai branco |
| Slot da paleta vem do tile de render | Confirmado: `palette` (bits 20-23 do `w1` do `G_SETTILE` do tile 0) indexa `tluts[]`; o TMEM do tile 7 indexa o slot na carga |

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| CI4 (32×32, moldura magenta de 5 texels, campo vermelho, patch branco 8×8 em texel ímpar) | Moldura uniforme de 11-12 px, cantos retos, patch começando em 17,1 texels e terminando em 25,2 — sem serrilhado |
| CI8 (32×32, rampa de 32 degraus, metade de cima com `b = 31`) | 32 bandas verticais nítidas; azul→azul-claro à esquerda do meio, oliva→amarelo à direita, exatamente como a paleta define (`v = i >> 3`, `b` só para `i < 128`) |
| HUD linha 19 | `Tex b0 h3 x0 t6/4 on` — 3 acertos de cache por quadro (RGBA + CI4 + CI8), 6 triângulos texturizados, 4 deles indexados, nenhum skip |
| Matrizes/projeção | `DL t6 c0 p2 m5/3 x0 ok`, `MtxOK` |
| Desempenho | 60 fps, 16,7 ms por quadro, submissão de CPU 1,4 ms no pior caso |

Capturas: `%TEMP%\ordsi\ci_final.png` (cena completa),
`ci3_q4b.png` (zoom do CI4), `q5.png` (zoom do CI8).

### Erros meus que a inspeção pegou

- Deixei dois `else if (tile == 7)` idênticos no `G_SETTILE` durante a edição;
  o compilador não reclama, só o segundo nunca executa.
- A primeira moldura do CI4 era azul (`0x003F`) sobre o quadrante azul do quad
  RGBA: a borda existia mas era invisível, o que me fez suspeitar de bug por
  dois testes. Passou a magenta para medir contra qualquer fundo.
- A rampa de cinza que eu tinha posto nas entradas 3..14 da paleta CI4 não era
  usada por nenhum texel da imagem — paleta de teste precisa casar com o
  padrão de teste, senão não mede nada.
- A leitura inicial do `COUNT` do `LOADTLUT` como contagem direta
  (sem o `+1`) faria a paleta CI8 vir com 255 entradas e cair no `texSupported`.

### Limites e pendências

- Continuam de fora **IA4/IA8/I4/I8**, texturas retangulares (exigem padding ou
  atlas) e qualquer tamanho acima de 128 ou fora de potência de 2.
- Não há máscara/shift de wrap por eixo, deslocamento de TMEM no tile de
  render, matriz de textura nem combiner: assume-se REPEAT nos dois eixos e
  modulação por cor de vértice.
- `G_LOADTILE`/`G_LOADBLOCK` com `uls/ult` diferentes de 0 (carga parcial de
  TMEM) são ignorados; o decodificador assume carga linear da imagem inteira.
- O cache então era de 8 slots sem despejo; a fase seguinte (topo do arquivo)
  trouxe a reciclagem LRU.
- Custo medido só em DS mode; falta DSi real e texturas reais do jogo.

---

## Fase 2 — Projeção da display list e pilha por tipo de matriz (2026-10-27)

> **ID** `OTD-006` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-006/` · **Commit** árvore sem histórico git

**Objetivo:** parar de ignorar `G_MTX_PROJECTION` (a projeção fixa do
`video_top.c` não serve para cena 3D), aplicar `G_MTX_MUL` de verdade e fazer o
`G_POPMTX` devolver cada nível para a pilha (projeção ou modelview) em que ele
foi empilhado.

### Correção de uma conclusão errada da fase de matrizes

A seção "Matrizes da display list no GE" afirmou que o array plano do N64
precisa ser **transposto** para o libnds. Isso estava errado e foi corrigido
aqui:

- libultra guarda 16.16 **linha por linha** e transforma vetor-linha
  (`v · M`); o DS guarda 1.19.12 **coluna por coluna** e transforma vetor-coluna
  (`M · v`). O elemento `[row][col]` da forma linha-vetor é exatamente o
  `[col][row]` da forma coluna-vetor — que é o slot coluna-maior. Ou seja, são
  **os mesmos 16 valores na mesma ordem**: conversão é cópia elemento a
  elemento, sem troca de índice.
- A translação fica em `m[12..14]` nas duas formas e é a única parte dividida
  por `2^shift` (os vértices entram no GE em unidades de tela, 64 unidades de
  mundo = 1 unidade de tela com `shift = 6`).
- Em `guPerspective` o `-1` fica em `mf[2][3]` = plano 11 e o termo
  `2nf/(n−f)` em `mf[3][2]` = plano 14 — **os mesmos slots** que o
  `glFrustumf32` do libnds escreve. Em forma coluna-vetor o plano 11 é o
  coeficiente de `z` na linha de `w` (`w' = -z`, correto) e o plano 14 é a
  constante somada a `z'`, que está em unidades de mundo e por isso também é
  dividida por `2^shift`. O `-1` **não** é dividido.
- Evidência usada: `guPerspective`/`guTranslate` do decomp, `GPU3D.cpp` do
  melonDS (`MatrixMult4x4`) e a doc F3DZEX2 do CloudModding para
  `G_MTX_MUL` ("new matrix * top of the stack").

O erro passou despercebido porque a DL antiga montava a matriz já transposta à
mão (com a translação no plano 11). Com matrizes reais do jogo, a transposição
jogaria a translação fora.

### Entregue

- **Conversão única para modelview e projeção** (`ge_draw.c`): `mtxToDs` sem
  transposição; `>> 4` no corpo e `>> 10` (`4 + shift`) no plano 12..14.
- **Tipo por nível da pilha**: `GeDraw.mtxType[F3_MTX_STACK]` guarda se o nível
  foi empilhado como projeção; `popLevels` seleciona `GL_PROJECTION` ou
  `GL_MODELVIEW` antes de cada `glPopMatrix(1)`. Os portos `MATRIX_*` são
  FIFO-ordenados (ao contrário de `GFX_CONTROL`), então trocar de modo no meio
  da lista é seguro.
- **`G_MTX` completo**: `LOAD`/`MUL` para projeção e modelview; `PUSH` faz
  `glPushMatrix` no modo certo. `MUL` usa `glMultMatrix4x4`, que no GE calcula
  `novo × atual` — a mesma ordem do RSP, sem composição na CPU.
- **Desempilhamento no fim da lista**: se a DL terminar com níveis pendentes, o
  `geDrawList` desempilha pelo tipo e reporta a sobra em `mtxLeft`, para as
  pilhas do GE não derivarem entre frames.
- **HUD**: `DL t.. c.. p.. m../.. x.. ok` — triângulos, saturações, operações
  de matriz na projeção, operações totais, pops e níveis sobrando.
- **`topRender` recarrega a orto todo frame**: a DL agora substitui a projeção,
  então a grade não pode confiar na projeção definida no `topInit`.
- **Autoteste** (`geMtxSelfTest`): agora confere duas matrizes — modelview
  `Rz(30°) + T(0,0,32)` no layout do libultra e uma `guPerspective(1,2,100,300)`
  (escala x 1, y 2, z −2, `−1` no plano 11 e `−300` no plano 14).
- **DL de demonstração** (`dl_demo.c`): 3 quads, 5 matrizes e 50 palavras —
  projeção perspectiva (60°, 4:3, near 200, far 2000), quad texturizado girado
  30° em z=−300, quad colorido em (−130,70,−290), `PUSH|MUL|PROJECTION` com um
  zoom 0,75 em x/y e quad colorido em (130,−70,−250), e **três** `G_POPMTX`
  seguidos: modelview, projeção, modelview.

### Codificação F3DEX2 que estava errada na DL

| Item | Descoberta | Correção |
|---|---|---|
| `PUSH\|MUL\|PROJECTION` | `gSPMatrix` inverte o bit de push: o valor real `0x05` vai como `0x04`, não `0x06` (0x06 decodifica como PUSH+**LOAD**) | com `0x06` a projeção virava só o zoom, sem perspectiva, e o 3º quad caía no plano de corte |
| Endereço do `G_VTX` | O `G_VTX` do F3DEX_GBI_2 copia `n` vértices **do endereço em `w1`**; `v0` só escolhe o destino no buffer de estado | 2º e 3º quads apontavam para `0x0E0` e reliam os 4 primeiros vértices (todos brancos); agora apontam para `vtx+4` e `vtx+8` |
| `w0` do `G_VTX` com `v0=8` | `w0 = cmd<<24 \| n<<12 \| (v0+n)<<1` → `0x01004018` | a DL usava `0x01C04010`, que decodifica `v0=4` |

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Autoteste das duas matrizes | `MtxOK` |
| Projeção perspectiva da DL | quad texturizado girado 30° com perspectiva; textura (quadrantes, diagonal e marca branca) sem distorção |
| Quad em (−130,70,−290) | canto superior esquerdo, ~69×69 px, na frente do quad grande |
| Zoom de projeção (MUL) + quad em (130,−70,−250) | ~59×59 px no canto inferior direito (0,75 × o tamanho que teria sem zoom), na frente do texturizado |
| Pilha por tipo | `p2 m5/3 x0`: 2 ops na projeção, 5 matrizes, 3 pops, 0 níveis sobrando |
| HUD | `DL t6 c0 p2 m5/3 x0 ok`, `Tex b0 h1 x0 t2 on` |
| Desempenho | 60 fps, 16,7 ms, submit 2,3 ms, pior quadro 27 ms |

Captura: `%TEMP%\ordsi\mtx9.png` (topo ampliado em `mtx9_top.png`).

### Erros meus que a inspeção pegou

- A transposição herdada da fase anterior (acima) — só apareceu ao montar a
  matriz como o libultra monta, com a translação no plano 12..14.
- `G_MTX` de zoom com `0x06`: como `0x06 ^ 0x01 = 0x07`, o decodificador viu
  `PUSH|LOAD|PROJ` e **substituiu** a perspectiva. O sintoma foi o 3º quad
  sumir (z_ndc constante em −1) e não um erro de matriz.
- Os dois `G_VTX` apontando para a base: o quad "pequeno" saiu como um retângulo
  branco gigante (os 4 vértices do quad grande, todos brancos) — o tamanho e a
  cor branca pura foram a pista.
- `(proj ? d->proj : d->mtx) = ds;` não é C válido (expressão condicional não é
  lvalue); virou `if/else`.

### Limites e pendências

- A projeção já vem da DL, mas `glViewport` e a faixa de profundidade continuam
  fixos; near/far do jogo precisam ser mapeados para ≤ 8192 unidades de mundo,
  senão o termo Q12 da projeção satura (±128 em unidades de tela).
- Sem `G_MOVEMEM` (VIEWPORT, LIGHT) e sem matriz de textura; `GUARD`/`CULLDL`
  não cortam nada ainda.
- Texturas seguem só RGBA16 quadradas; CI4/CI8 (paletas) são a próxima lacuna,
  e são a maioria das texturas do jogo.

---


**Objetivo:** tirar a textura de dentro da display list (SETTIMG/SETTILE/
LOADBLOCK/SETTILESIZE/G_TEXTURE), convertê-la para o formato do DS e amarrá-la
aos vértices por `glTexCoord2t16`, com cache para não reenviar por frame.

### Entregue

- **Decodificador de textura** (`f3dex2.c`): estado `F3Tex` com imagem
  (endereço de segmento resolvido), fmt/siz do SETTIMG, descritor do tile de
  render (fmt/siz/line/tmem/palette), extensão em texels vinda do
  SETTILESIZE (10.5) e contagem de words do LOADBLOCK. Hook `F3TexHook` com
  eventos READY/ON/OFF. `G_LOADTILE` e `G_LOADTLUT` só são contados.
  Correção de contagem: `0xFD` (SETTIMG) saiu do `default` e virou caso
  próprio, senão o `imgChanges` nunca subia.
- **Conversão de cor** (`ge_draw.c`): RGBA5551 do N64 (R 11-15, G 6-10,
  B 1-5, A 0) para A1B5G5R5 do DS (A 15, B 10-14, G 5-9, R 0-4). É só um
  swizzle por texel — **a textura do N64 é linear na ROM e a DS também lê
  linear**, então não há swizzle 8×8.
- **Cache de texturas**: 8 slots por ponteiro de imagem + dimensões, VRAM nos
  bancos A e B (256 KB). Depois do primeiro frame o custo é um
  `glBindTexture` por troca, sem reenvio.
- **Textura branca 8×8**: como `GL_TEXTURE_2D` é um bit global do GE
  (DISP3DCNT) e **não é ordenado com a FIFO**, ligar/desligar no meio do frame
  atingia a geometria errada. Solução: textura sempre ligada e, quando
  `G_TEXTURE` está desligado, amarrar uma branca opaca e emitir a coordenada do
  centro do texel (0,0) — o resultado é a cor do vértice pura. A grade do
  `topInit` também passou a emitir essa coordenada.
- **UV**: s/t do N64 são 10.5; o DS usa 12.4, então `t16 = s >> 1`.

### Descobertas (medidas no emulador, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| DS quer textura em tiles 8×8 | **Falso**: `glTexImage2D` copia os bytes como estão (dmaCopy, conferido no fonte do libnds) e o GE lê linear. Pré-tilear produziu listras horizontais de 2 texels |
| RGBA5551 → A1B5G5R5 trocando R/B | Incompleto: a máscara de R é 11-15, não 0-4; a primeira versão lia A+B como se fossem R |
| Ligar/desligar `GL_TEXTURE_2D` por primitiva | Não funciona: registro do GE fora da FIFO, o `glDisable` do fim do frame chegava antes da rasterização do quad texturizado |
| G_TEXTURE com `on` no bit 0 | Em F3DEX_GBI_2 o campo `on` fica nos bits 1-7 (`gSPTexture` do `gbi.h`), como o decodificador já assumia |

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Textura 32×32 sintética (4 quadrantes + diagonal + marca branca) | Aparece no quad com orientação correta, girada 30° junto com a matriz |
| `Tex b0 h1 x0 t2 on` (HUD) | 0 uploads no frame, 1 acerto de cache, 2 triângulos texturizados |
| Quad sem textura sob matriz empilhada | Continua com as cores de vértice, `MtxOK` e `DL t4 c0 m2/1 ok` |
| Frame | **60 fps, 16,7 ms**, submit 1,7 ms, mundo 28 ms, sem deriva da pilha de matrizes |

### Erros meus que a inspeção pegou

- Primeira conversão de cor errada (R nos bits baixos) — o quad saiu
  branco-avermelhado e escondeu o problema real por alguns testes.
- Pré-tilear 8×8 "porque o N64 usa tiles": o tile é do TMEM, não do asset.
- `bindTex` chamava `glBegin` mesmo com o lote fechado, deixando um `glBegin`
  aberto entre frames — corrigido reabrindo só se estava aberto antes.
- A textura sólida vermelha usada no diagnóstico ficou marcada como DEBUG e
  foi restaurada antes do build final.

### Limites e pendências

- Só **RGBA16, quadrada, potência de 2, 8..128 texels**. CI4/CI8/IA/I, texturas
  não quadradas (precisam de padding ou atlas) e 256+ caem em `x` (skip) e
  desenham com a textura anterior/branca.
- Não há semântica de wrap/mask/shift do tile, deslocamento de TMEM, matriz de
  textura nem combiner: assume-se REPEAT nos dois eixos e modulação por cor de
  vértice.
- O buffer de conversão é estático de 32 KB (128×128×2); o cache não faz
  despejo (8 slots) e não compartilha VRAM com paletas.
- Custo medido só em DS mode; falta DSi real e texturas reais do jogo (sem ROM
  de referência ainda).

---

## Fase 2 — Matrizes da display list no GE (2026-10-27)

> **ID** `OTD-005` · **Data** 2026-10-27 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-005/` · **Commit** árvore sem histórico git

**Objetivo:** fazer a geometria deixar de usar a transformação fixa do
`main.c` e passar a ser posicionada pela matriz 16.16 que vem dentro da
própria display list, incluindo a pilha de matrizes.

### Entregue

- **Gancho de matriz** (`f3dex2.c/.h`): `F3MtxHook` recebe `G_MTX`
  (`F3_MTX_EV_LOAD`, com a matriz de 64 B e as flags) e `G_POPMTX`
  (`F3_MTX_EV_POP`, com a quantidade de níveis).
- **Conversão 16.16 → m4x4** (`ge_draw.c`): a matriz N64 é coluna-maior com a
  translação na **4ª linha** porque lá o vértice é vetor-linha; o libnds usa
  vetor-coluna. O resultado é a transposta — no array plano, trocar
  `índice = 4*col + row` por `4*row + col` — e a translação cai em `m[12..14]`.
  A parte de rotação/escala só é convertida (`>> 4`); a translação é dividida
  por `2^shift`, porque os vértices chegam ao GE divididos por esse fator
  (1 unidade de mundo = 1/64 de unidade de tela). Translação acima de ~8.192
  unidades de mundo estoura o Q12.
- **Pilha**: `G_MTX` com PUSH vira `glPushMatrix()` + `glLoadMatrix4x4`;
  `G_POPMTX` vira `glPopMatrix(n)`.
- **Autoteste da conversão** (`geMtxSelfTest`): matriz 0,5 / −0,5 / 30° com
  translação, conferida contra os valores Q12 esperados; o resultado aparece
  como `MtxOK`/`MtxBAD` na linha 17.
- **DL de demonstração** (`dl_demo.c`): dois quadrados e duas matrizes —
  `LOAD` (30° em Z + z=32) para o grande, `PUSH` (identidade + 200,150,48)
  para o pequeno, `POP` no fim.

### Codificação F3DEX2 que estava errada no decodificador

| Item | Descoberta no `gbi.h` do decomp | Correção |
|---|---|---|
| Bit de PUSH do `G_MTX` | `gSPMatrix` emite `(p) ^ G_MTX_PUSH`: o bit chega **invertido** (`G_MTX_NOPUSH\|LOAD` vai como `0x03`, e `PUSH\|LOAD` como `0x02`) | o decodificador faz o XOR de volta antes de usar as flags |
| Argumento do `G_POPMTX` | `gSPPopMatrixN` põe em `w1` o **número de bytes** (`64 * n`), não a contagem | `n = w1 / 64`, com erro se não for múltiplo |

Ambos passariam batido nas DLs sintéticas antigas e quebrariam na primeira DL
real do jogo (matriz sem push viraria push, e o pop pediria 64 níveis).

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Conversão da matriz | `MtxOK` (rotação, translação e `ww` conferidos em Q12) |
| Quadrado grande | Inclinado 30°, deslocado para z=+32, na frente da grade |
| Quadrado pequeno | Sem rotação, 30×30 px, ~50 px à direita e ~37 px acima do centro, na frente do grande |
| Pilha | `m2/1` no console; após 12 s (>700 quadros) sem desvio (se ficasse desbalanceada, o GE saturaria em ~30 quadros) |
| Desempenho | 60 fps, 16,7 ms; submissão de CPU 2,6 ms; pior quadro 28 ms |

### Erros meus que o autoteste/inspeção pegaram

- `-0.5` em 16.16 não é `int = 0x8000, frac = 0`: é `int = 0xFFFF, frac =
  0x8000`. A primeira versão mandou o quadrado para `x = -32768`, fora da tela.
- A translação estava sendo **multiplicada** por `2^shift` em vez de dividida —
  o `MtxBAD` acusou na hora.

### Limites e pendências

- Matrizes de projeção da DL são contadas (`projIgnored`) e ignoradas: a
  projeção continua sendo a orto fixa de `video_top.c`. Converter a projeção
  perspectiva do N64 exige mapear o near/far para a convenção do DS.
- O `G_POPMTX` não diz se o topo da pilha é projeção ou modelview; hoje o
  `ge_draw` sempre popa o GE. Antes de rodar DLs reais é preciso guardar o tipo
  de cada nível (a pilha do decodificador já tem os bytes, falta o tipo).
- `G_MTX` com `MUL` (concatenação) usa `glMultMatrix4x4`; nenhuma DL de teste
  exercita o caminho MUL ainda.

---

## Fase 2 — Geometry engine: primeira display list na tela (2026-10-26)

> **ID** `OTD-004` · **Data** 2026-10-26 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-004/` · **Commit** árvore sem histórico git

**Objetivo:** deixar de só contar triângulos e realmente desenhar geometria
vinda de uma display list, pelo pipeline do DS (não pelas chamadas de conveniência
do `topRender`).

### Entregue

- **Gancho de triângulo no decodificador** (`f3dex2.h/.c`): `F3TriHook` é
  chamado por `TRI1`/`TRI2` com os índices **já divididos por 2**, depois da
  validação de faixa. `f3Run` continua sem conhecer o geometry engine.
- **Tradutor para o GE** (`source/gfx/ge_draw.c`): para cada triângulo converte
  as 3 posições N64 (`s16`, unidades de mundo) para o vértice do DS, que é
  **1.3.12 fixo** (`v16`, faixa ±8,0), com `v16 = mundo << (12 - shift)`.
  Com `shift = 6` o alcance é ±511 unidades de mundo. Overflow é contado
  (`clamped`) antes de saturar. Cor por vértice via `glColor3b`, envio com
  `glVertex3v16`, tudo dentro de um único `glBegin(GL_TRIANGLES)` em volta de
  `f3Run` (o GE só aceita vértice depois de `glBegin`).
- **DL sintética** (`source/gfx/dl_demo.c`): lista F3DEX_GBI_2 completa e
  autocontida em segmento 6 — `G_VTX` com 4 vértices (ponteiro `0x06000040`)
  e dois `TRI1`, colorida por vértice (vermelho/verde/azul/amarelo).
- **Integração no loop** (`source/main.c`): o quadrado da DL é decodificado e
  desenhado todo quadro sobre a grade, girando em torno de Z com ângulo
  próprio. Linha 18 do console mostra `DL tri.. cl.. ok/FAIL`.

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Quadrado da DL sobre a grade | Visível, 4 cores interpoladas, gira, sem corrupção |
| Contadores no console | `DL tri2 cl0 ok` (2 triângulos, 0 saturações) |
| Custo do quadro com a DL | 60 fps, 16,7 ms; submissão de CPU 2,4 ms (era 1,6 ms sem a DL) |
| Pior quadro | 28 ms (mesmo valor de antes; a DL não cria picos) |

### Limites e pendências

- O `G_MTX` da DL é decodificado para o buffer de 64 B mas **ainda não é
  aplicado**: a matriz de modelo continua vindo das chamadas do libnds, e a
  projeção é a orto fixa de `video_top.c`. O passo seguinte é converter a
  matriz N64 (16.16) para a `m4x4` do DS (20.12) e usá-la em
  `glLoadMatrix4x4`/`glMultMatrix4x4`.
- Sem textura, iluminação ou culling: `POLY_CULL_NONE` e ordem por
  profundidade (o quadrado usa `z = +32` para ficar na frente da grade).
- Ainda não há medição do orçamento de 2.048 triângulos por quadro com
  vértices reais de cena; só a DL de 4 vértices.

---

## Fase 2 — Preparação: decodificador F3DEX2 (2026-10-26)

> **ID** `OTD-003` · **Data** 2026-10-26 · **Frente** motor · **Fase** 2 · **Estado** implementado · testado no computador · **Evidência** `evidencias/OTD-003/` · **Commit** árvore sem histórico git

**Objetivo:** começar o renderizador pela parte que não depende de ROM nem de
hardware: interpretar display lists no formato que o decomp realmente emite.

### Entregue

- **Decodificador F3DEX_GBI_2** (`source/gfx/f3dex2.c`): opcodes conferidos no
  `include/ultra64/gbi.h` do decomp (que **não** são os do F3DEX antigo:
  `G_VTX=0x01`, `G_TRI1=0x05`, `G_MTX=0xDA`, `G_DL=0xDE`, `G_GEOMETRYMODE=0xD9`).
  Inclui resolução de segmento, pilha de matrizes de 10 níveis com cópia de
  64 B, `G_VTX` com o layout de vértice N64 de 16 B, `TRI1/TRI2`, modo de
  geometria, textura, contadores de ops RDP e códigos de erro.
- **Vetor sintético** (`f3dex2_selftest.c`): display list montada em RAM com
  vértices, matriz, sub-DL e uma sequência de comandos RDP; valida contagens,
  conteúdo dos vértices, pilha e três caminhos de erro.
- **Benchmark de decodificação**: lista de 2.048 triângulos executada 20 vezes.

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Lista principal | OK: 1 carga de 3 vértices, 4 triângulos, 7 ops RDP |
| Pilha de matrizes | push/pop 1/1, profundidade 0 |
| Caminhos de erro | opcode desconhecido, índice ≥ 32 e `POPMTX` vazio detectados |
| Decodificação de 2.048 triângulos | **0,9 ms** (441 ns/triângulo) |

### Limites e pendências

- O decodificador **conta** triângulos mas ainda não emite nada para o
  geometry engine; transformação, culling e envio de vértices são o próximo
  passo. *(Feito na entrada acima.)*
- `G_QUAD`, `G_BRANCH_Z`, `G_MODIFYVTX`, luzes/viewport (`G_MOVEMEM`) e
  `G_MOVEWORD` são reconhecidos e ignorados; entram junto com a tradução.
- Sem ROM de referência, os testes usam DLs sintéticas — o formato foi
  conferido nas macros do `gbi.h`, não em dados reais do jogo.
---

## Fases do motor ainda não iniciadas

| Fase | Conteúdo | Portão |
|---|---|---|
| 2 | Blending (`G_SETOTHERMODE_H`, `G_SETPRIMCOLOR`, `G_SETENVCOLOR`), ciclo 1 do combiner, expansão CI→A1B5G5R5 para recorte paletado, IA4/IA8 e texturas retangulares | três salas do jogo estáveis a 20 fps |
| 3 | Efeitos sem FBO (blend modes, `G_TEXTURE` com shift/mask, ordenação por textura, orçamento de VRAM por bytes) | VRAM estável em cena carregada |
| 4+ | Sombras, névoa por vértice e o que a primeira display list real exigir | bloqueado pelo build de referência |
