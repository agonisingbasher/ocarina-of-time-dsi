# Histórico do Jogo — Ocarina of Time DSi

Registro por etapa do port em si: plataforma libnds, loop de 20 Hz, memória,
streaming de assets, camada de jogo, UI das duas telas, áudio e integração. A
entrada mais recente fica no topo. Nenhuma etapa é marcada como concluída sem
medição no hardware indicado.

**Escopo deste arquivo:** `source/nds/`, `source/ui/`, `source/main.c`,
`game/oot` e tudo que diga respeito a rodar o jogo. O tradutor de gráficos
(decodificador F3DEX2 e geometry engine) tem série própria em
`HISTORICO_MOTOR_GRAFICO.md`; o índice das duas séries está em
`HISTORICO_FASES.md`.

---

## Onde o jogo está

| Caminho | Estado |
|---|---|
| Toolchain, esqueleto do projeto e ROM compilável | coberto |
| Sonda de hardware (DSi mode, clock, heap, RAM, SD) | coberta e medida no emulador; **portão de DSi real aberto** |
| Base de tempo em timer de hardware, escalonador 20 Hz, telemetria | cobertos e medidos |
| Arena de memória do jogo (alocador linear, pico, alinhamento DMA) | coberta e medida |
| Yaz0 em memória e em streaming em blocos | cobertos e medidos (3,5–3,9 MB/s em DS mode) |
| Vídeo das duas telas, toque e grade de inventário 3×3 | cobertos e medidos |
| Entrada: teclas DS + touch → bitmask `BTN_*` do OoT, stick e C-botões | **coberta e medida** (31/31 no autoteste) |
| Camada `nds/fs` sobre libfat para ler assets do SD | **coberta e medida** (19/19; 11,6 MB/s de leitura, 2,9 MB/s fim a fim) |
| Substituição da libultra (threads, PI DMA, VI, scheduler) | **parcial**: loop/tempo/memória/`fs` prontos; os três `os*` que o allocator do jogo pede existem (`osGetTime`, `osSetIntMask`, `osGetThreadId`); faltam os de vídeo, DMA e threads |
| Código real do decomp no ROM (allocator, `Gfx`, matemática do jogo) | **coberto e medido**: 9 translation units do `zeldaret/oot` ligadas, 49/49 asserções passando no emulador |
| Pilha de matrizes do `sys_matrix.c` (`Matrix_Init/Put/Push/TranslateRotateZYX/ToMtx/Pop` + arena `THA`) gerando a modelview do jogo | **coberto e medido em pixels**: faixa do jogo visível no quadro completo, emenda a 8,43 colunas/linha contra 8,33 previstas |
| Display list montada pelo código do jogo (`Gfx_Alloc` + `guPerspective` + `gSP*` + endereço de segmento) até o painel | **coberto e medido em pixels**: 49/49 asserções, 4 triângulos, faixa em painel linhas 2..19 com a rotação de 45° do jogo |
| Baserom e extração de segmentos | **coberto e medido**: `Rom/…(USA) (Rev B).n64` é **NTSC-U 1.2** byteswapped (MD5 deitado `57a9719a…` = `baseroms/ntsc-1.2/checksum-US-compressed.md5`); 1510 segmentos, 54 MB, sendo 388 salas, 101 cenas e 378 objetos. A extração de *assets* (`tools/`, Linux-only) não roda aqui — o port lê os bins crus em tempo de execução |
| Sala real do jogo no DSi (`oot_room.c`: `SceneCmd`, MESH, `Gfx_Alloc`, prologue, câmera do `sys_matrix.c`) | **coberto e medido**: `market_day_room_0` cru do SD, 22640 B, 510 triângulos, `DL t510 c0 k0 x0 ok`, 30 FPS / 22,9 ms de render |
| Sala com textura, com os objetos da cena montados | **coberto e medido**: `Bmori1_room_0` (Forest Temple), 646 triângulos, 75 `G_SETTIMG` nos segmentos 2/3/8 e 8 `G_LOADTLUT` em 2 paletas da cena, `Tex b0 h65 x0 e0 t646/532 on`, 30 FPS. Com a cadeia de mip do motor (nível até 2) o grão do painel cai de 13,28 a 9,09 no mesmo ângulo preso, a troco de `s30,0 → s49,6` |
| Consola 2D de telemetria do motor (linhas 0..22 da tela de baixo) | **coberta e medida**: `DL`, `Tex`, `F3`, `Bl`, `Bk` — o `Bk` mede o grão dos texéis enviados e o span de UV do frame. Em build de sonda (`GE_LOD_PROBE`) as linhas 21 e 22 viram `Lo n0/n1/n2/n3 r…` e `Lo gp… gt… a… b…`, e a 22 deixa de mostrar `Bk` |
| Régua de medida: câmera do demo presa para duas capturas serem comparáveis | **coberta e medida**: `ROOM_SPIN`/`ROOM_ANGLE` em `source/main.c`; com a câmera presa, duas capturas do mesmo ângulo têm md5 igual. Foi o que expôs que o `17,25 → 7,00` da fase 6 comparava três vistas diferentes |
| Atores, animação, colisão, câmera do jogo, HUD, texto, inventário real, áudio | **não iniciados** |

---

## Fase 7 — a régua de medida foi consertada, e ela refutou a fase 6 por engano antes de se refutar a si mesma (2026-10-27)

> **ID** `OTD-024` · **Data** 2026-10-27 · **Frente** motor + jogo · **Fase** 7 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-024/` · **Commit** árvore sem histórico git

> **Correção dentro desta entrega:** `OTD-025` — a posição projetada da sonda
> estava 4096× fora de escala e 467 de 646 faces caíam pela guarda `pxA == 0`;
> a correção e as capturas posteriores estão em
> `evidencias/OTD-025/`. Não é um avanço independente.

**Objetivo:** a tarefa #37 é do motor (cadeia de mip por triângulo; ela está em
`HISTORICO_MOTOR_GRAFICO.md`). Do lado do jogo o que havia a fazer era tornar a
pergunta respondível: a fase 6 terminou com um número de grão que ninguém podia
repetir, e o motor ia medir "melhorou ou piorou" com o mesmo instrumento. Escopo
aqui é `source/main.c`: prender a câmera, dar à consola as linhas que a sonda do
motor precisa, e registrar o que a medição consertada disse da hipótese desta
própria série.

### Entregue

- **Câmera presa por build.** `ROOM_SPIN` (0) e `ROOM_ANGLE` (`0u`, um `u16`, 65536
  por volta) substituem a órbita automática. A órbita não era um enfeite inocente:
  o ângulo entrava na construção da display list, na orientação da luz **e** no
  recorte, e o quadro em que a captura caía dependia do FPS — que é justamente o
  que se estava medindo. Com `ROOM_SPIN 0` duas capturas do mesmo build têm md5
  igual, e foi assim que `mp6`, `mq8` e `mq9` puderam ser comparados.
- **Stick continua vivo com a câmera presa.** `angle += stick_x * 8` por input
  relativo, então a varredura de ângulo é feita pelo touchpad sem recompilar. É
  como se vai procurar uma vista que minifique sem perder a reprodutibilidade do
  ponto preso.
- **Duas linhas de sonda no HUD.** Em build com `GE_LOD_PROBE`, linha 21:
  `Lo n0/n1/n2/n3 r…` (histograma de níveis e `16·R²` da pior face) e linha 22:
  `Lo gp… gt… a… b…` (as guardas e as duas áreas da face recordista). Em build
  normal a linha 22 é o `Bk d… m…/…/… r… e…` da fase 6. A linha **23 não renderiza**
  — o console são 23 linhas, índices 0..22 —, então a sonda desloca o `Bk` em vez
  de criar linha nova; está escrito no comentário do código.

### Descobertas medidas

- **O `17,25 → 7,00` da fase 6 não comparava nada.** `mp1` 7,00, `mp2` 7,11 e a
  linha de partida 17,25 eram três vistas diferentes da mesma sala, tomadas em
  frames de órbitas distintas. A "vitória" era o ângulo.
- **A régua presa funcionou, e foi ela que deixou a fase 7 se enganar com
  confiança.** Com `ROOM_SPIN 0`, duas capturas do mesmo build dão md5 igual — foi
  isso que fez o `Lo 646/0/0/0` parecer uma medição e não um artefato. Repetibilidade
  não é exatidão: o instrumento media a mesma coisa errada duas vezes. A escala da
  projeção estava 4096× fora, 467 de 646 faces perdidas por guarda, e a sala
  minifica muito (detalhe no histórico do motor).
- **A hipótese da fase 6 está de pé.** Contada direito, a sala dá `Lo 2/25/256/363`
  no mesmo ângulo preso, e a cadeia leva o grão do painel de **13,28 a 9,09**. A
  anotação de refutação que esta série escreveu mais cedo hoje foi retirada.
- **Custo, A/B repetido no mesmo ângulo:** `s30,0` / FPS 20 sem cadeia contra
  `s49,6` / FPS 19 com ela (`f33,4 → 50,1`). O que cobra é troca de textura, não a
  matemática do nível: cadeia de profundidade 1 já custa o mesmo que a de 2.
- **A varredura que achou o bug** foi feita pelo stick com o build preso: procurar
  um ângulo que minificasse e achar, no primeiro testado, o `gp467` e o
  `b4294967295` que denunciavam a escala. A sonda custou ~10 % do render e pagou a
  fase.

### Validação

melonDS 1.1, DS mode, 67 MHz, `Bmori1_room_0`, 646 triângulos, `ROOM_SPIN 0` /
`ROOM_ANGLE 0`. Capturas em `%TEMP%\ordsi`: `mp6` (sem cadeia), `mp13` (seletor de
aresta), `mq1`–`mq9` (sonda com a escala errada — foi o `Lo 646/0/0/0` que a fase 7
retirou), `mr0`/`mt0`/`mt1` (sonda com a projeção corrigida), `mu0a`/`mu0b` (sem
cadeia), `mu3a`/`mu3b` (profundidade 3), `mv1` (nível 1), `mv2` e `mf0` (**build
entregue**, profundidade 2), `sw8192` (viés 4096). Cada par de um mesmo regime é
md5-idêntico; `mf0` = `mv2` = `05992cb58c8229ba3b60a28f949e2e93`. Build entregue sem
warnings e sem a sonda; `DL t646 c0 k0 p1 m2/0 x0` e `Tex … t646/532 on` inalterados
em toda a série. Grão medido com `speck2.py` no recorte 256×192 do painel de cima.

### Erros meus

1. **Deixei a demo girar a própria câmera uma fase inteira** e tratei o grão como
   grandeza estável. Foi a fase 6 publicar `17,25 → 7,00` por causa disso, e eu só
   fui olhar o instrumento quando o motor discordou de si mesmo.
2. **Escolhi a linha 23 do HUD para a sonda** sem checar que ela não é renderizada
   — erro que este arquivo já registrou na fase de `fs`, e cometi de novo.
3. **Acreditei que prender a câmera era cosmético** e o stick relativo era o
   controle; levei uma captura para entender que o controle *é* o instrumento de
   varredura, e que sem ponto preso ele não serve para comparar builds.
4. **Confiar na repetibilidade sem checar a exatidão.** A régua presa foi um
   conserto real, e foi exatamente ela que fez um número errado parecer dois
   números iguais e, portanto, verdadeiros — com o md5 batendo, declarei a fase 6
   refutada. A régua presa garante que a medição é *repetível*; quem garante que é
   *certa* é a derivação aritmética, e essa eu não reli antes de publicar.
5. **Aceitei um campo de sonda saturado (`b4294967295`) como grandeza.** u32 no teto
   não é medida, é transbordo; uma área que bate no teto devia ter virado guarda
   visível, não histograma.

### Limites

- A câmera do jogo ainda não existe: a órbita sintética (agora presa) segue sendo
  uma câmera de demo, com o `angle` alimentando `ootDlBuild` e `ootRoomBuild`
  diretamente. Prendê-la serve à medição, não ao jogo.
- Uma sala só, carregada no boot, sem portas, sem atores, sem HUD do jogo.
- A régua presa é o ângulo 0, e só há captura de referência nesse ponto. A
  varredura de ângulo que achou o bug foi feita com a sonda ligada e o build
  desregulado; ela localizou a anomalia, não produziu curva de grão por ângulo.
- O +19,6 ms da cadeia ainda não foi pago. A sonda foi desligada no build entregue,
  então o painel hoje mede o custo real, não o de instrumento.

### Próximo

Do lado do jogo: (1) re-prender o build em cada ângulo da varredura e reportar o
grão por vista, com a sonda desligada e a cadeia em profundidade 2; (2) a comparação
de paleta com uma referência do N64 no ângulo 0; (3) a segunda sala e os ids de
objeto reais da cena montados como segmentos. O conserto do custo por troca de
textura é do motor e está listado lá primeiro.

---

## Fase 6 — A sala texturizada entrou, e a consola ganhou o instrumento que matou a hipótese da paleta (2026-10-27)

> **ID** `OTD-023` · **Data** 2026-10-27 · **Frente** motor + jogo · **Fase** 6 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-023/` · **Commit** árvore sem histórico git

**Objetivo:** depois de `market_day_room_0` flat-shaded, pôr uma sala **com
textura** no ar e descobrir por que o painel saía com confete colorido. O motor é
tratado em `HISTORICO_MOTOR_GRAFICO.md`; aqui é o lado do jogo: qual sala, o que o
carregador precisou, e o que a consola de telemetria passou a medir para que a
causa fosse fechável com número.

### Entregue

- **`Bmori1_room_0` (Forest Temple) no ar.** É a primeira sala do ROM que tem
  comandos de textura: **646 triângulos**, **75 `G_SETTIMG`** (49 no segmento 3 —
  o arquivo da sala — 25 no 2 e 1 no 8) e **8 `G_LOADTLUT`** resolvendo em **2
  paletas da cena** (`0x14288` e outra). As 6 imagens CI8 únicas são 32×32, 64×32,
  16×32, 16×64. `market_day_room_0`, ao contrário, **não tem nenhum** comando de
  textura e fecha com `G_TEXTURE on=0` — a fase anterior atribuiu o flat-shading
  dela a segmentos de objeto faltantes, e essa explicação morreu aqui (tarefa #31
  refutada).
- **Linha 22 da consola — `Bk`.** `snprintf` em `source/main.c` logo depois do `Bl`:
  `Bk d<grão> s<texMaxS> t<texMaxT> n<bakePix>`, isto é, o `|Δluma|` médio entre
  vizinhos dos texéis **que estão sendo enviados**, o span máximo de coordenada de
  texel que qualquer vértice do frame carregou, e quantos texéis foram enviados. É
  a peça que permite comparar a textura com a tela em vez de opinar sobre ela.
- **Linha 9 — `B1`, a sonda de identidade da textura**, que fechou o caminho da
  paleta: `iR:B128 pS:14288 f2 s1 b11` — a mesma imagem, a mesma paleta, o mesmo
  fmt/siz e a mesma endianidade que o script offline usa.

### Descobertas medidas

- **O confete não é cor.** Os texéis enviados medem `r13 g12 b12` e grão **2,4**; o
  painel mede **17,25**; zero em 45952 texéis é azul-dominante. Trocar cada textura
  por uma cor chata leva o painel a **0,68** e a cena a um trapézio liso.
- **A causa é a minificação** — 448 × 347 texéis de UV numa parede de ~150 px, e o
  DS não filtra. Detalhe e medição no histórico do motor.
  *(A fase 7 chegou a refutar isto no mesmo dia e depois retirou a própria
  refutação: o seletor de nível que mediu "nenhuma face minifica" projetava a
  posição de tela 4096 vezes maior que o certo, então as áreas de pixel saíam
  saturadas ou zeradas. Contado com a escala certa, na mesma câmera presa, o
  histograma é `Lo 2/25/256/363` e a parede de `448 × 347` texéis minifica como a
  fase 6 disse. O que não sobrevive é o grão `17,25 → 7,00`: essas três capturas
  eram vistas diferentes.)*
- **A sala é reconhecível com o estado atual.** Piso, madeira e painéis de parede
  distintos, estáveis entre ângulos de órbita; o grão é degradação de resolução,
  não geometria nem textura errada.

### Validação

melonDS 1.1, DS mode, 67 MHz, builds `bm33` e `bm37` da mesma cena: `DL t646 c0 k0
p1 m2/0 x0`, `Tex b0 h65 x0 e0 t646/532 on`, `Bk d2.4 s14336 t11109`, FPS 30,
grão do painel 17,25 / 17,31 entre builds. Capturas em `%TEMP%\ordsi`: `bm33`,
`bm34` (controle chato), `bm35`–`bm37`.

### Erros meus

1. Aceitei a explicação "faltam segmentos de objeto" da fase anterior sem checar a
   lista de `market_day_room_0`, e ela estava errada — a cena não usa textura
   nenhuma. Uma fase inteira de caça (tarefa #31) nasce dessa frase não relida.
2. Duas capturas rodaram binário velho porque `make` não recompila `source/main.c`
   nesta árvore OneDrive; o `touch` obrigatório nos dois arquivos é regra agora.
3. Coloquei um contador de bake no reset por frame, o que fazia o HUD dizer `Bk 0`
   justo no regime estacionário que se queria medir.

### Limites

- Uma sala texturada, carregada no boot, sem portas, sem atores desenhados, com
  órbita sintética em vez da câmera do jogo.
- O grão da minificação continua visível até a fase de mip. *(A fase 7 entregou a
  mip por triângulo: com a câmera presa em `ROOM_ANGLE 0`, o mesmo instrumento dá
  13,28 sem cadeia e 9,09 com ela. O grão baixou, não sumiu.)*

### Próximo

Tarefa #37 (mip por triângulo, no histórico do motor). Do lado do jogo, depois
disso: a segunda sala e os ids de objeto reais da cena
`[197,261,263,264,269,268,273,19,124,224,363,368,362,386,396]` montados como
segmentos, que é o que falta para uma sala cujo `G_SETTIMG` aponte a objeto.

---

## Fase 6 — A primeira sala real do jogo apareceu na tela de cima, e o bloqueio do baserom era um byte-swap (2026-10-27)

> **ID** `OTD-022` · **Data** 2026-10-27 · **Frente** motor + jogo · **Fase** 6 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-022/` · **Commit** árvore sem histórico git

**Objetivo:** responder aos quatro passos pedidos — descobrir que ROM de
referência o zeldaret/oot exige, extrair as tabelas de mapa/ator/display list,
compilar o que faltava do decomp e renderizar a primeira sala real no motor do
DSi. O primeiro item travava os outros três: desde a Fase 1 o projeto carregava
a frase "bloqueado: exige o NTSC-U 1.0".

### Entregue

- **O bloqueio não existia.** `Rom/Legend of Zelda, The - Ocarina of Time (USA)
  (Rev B).n64` está em ordem **byteswapped**; deitado par a par, o MD5 vira
  `57a9719ad547c516342e1a15d5c28c3d`, que é o `checksum-US-compressed.md5` de
  `baseroms/ntsc-1.2`. **Rev B é NTSC-U 1.2**, o build aceito pelo decomp, e a
  1.0 não é necessária — não há nada para converter. `decompress_baserom.py
  ntsc-1.2` + `extract_baserom.py` rodaram no Python do venv
  (`<ambiente Python local>`), sem PIL.
- **1510 segmentos crus, 54 MB**, em `game/oot/extracted/ntsc-1.2/baserom/`:
  **388 salas**, **101 cenas**, **378 objetos**, mais `code`, `ovl_*`, tabelas de
  áudio e as estáticas. `market_day_room_0` tem 22640 bytes e md5
  `19fb00a81d01dfae378d25a0f0882284` — o mesmo arquivo, byte a byte, que o port
  lê do SD como `/MKTDAY.ROO`.
- **A extração de *assets* não é o caminho deste port.** Os `tools/` do
  zeldaret/oot que geram as tabelas `.h` são Linux-only. Em vez de esperar por
  eles, o carregador **interpreta o binário cru em tempo de execução**, que é o
  que o hardware faz de qualquer jeito: `ootRoomOpen` percorre a `SceneCmd` e
  `roomMeasure` roda a display list pelo decodificador para obter a caixa real.
- **`source/oot/oot_room.c`** — abre o arquivo como o jogo abre: varre a lista de
  comandos de 8 bytes, trata `0x0A` MESH (forma NORMAL, entradas opaca/xlu),
  `0x0B` OBJECT_LIST, `0x01` ACTOR_LIST e `0x14` END, e **rejeita** com `stage`
  próprio (2 = forma não desenhável, 3 = tabela de entradas inválida) em vez de
  desenhar lixo. Monta no blob do `Gfx_Alloc` um prologue com `gSPViewport` de
  320×240, a projeção do `guPerspective` do jogo, a modelview da pilha
  `sys_matrix.c` e um `gSPDisplayList` por entrada. Cinco `_Static_assert`
  fecham o layout do blob contra o `RmFileHeader`/`RoomMeshHeader` do decomp.
- **HUD arrumado:** as três linhas provisórias da sala viraram duas permanentes
  (linhas 10 e 11), o probe de drain foi comprimido em uma linha (12) e a sonda
  temporária `printRoomLive` saiu do código.

### Descobertas medidas

- **A lei de composição da pilha de matrizes do jogo** — a descoberta da fase, e
  ela custou quatro builds. `Matrix_Translate(T1, MTXMODE_NEW)` →
  `Matrix_RotateZYX(R, APPLY)` → `Matrix_Translate(T2, APPLY)` dá
  `out = R·v + T1 + R·T2`, porque o ramo APPLY de `Matrix_Translate` **só soma**
  `M·(x,y,z)` nos slots `xw/yw/zw` (`cmf->xw += xx*x + xy*y + xz*z`, lido em
  `src/code/sys_matrix.c`) e o ramo APPLY de `Matrix_RotateZYX` **reescreve só a
  3×3**, deixando a translação intacta. Tradução: **uma translação aplicada
  depois de uma rotação é rotacionada junto.** A órbita que eu queria é
  `out = R·(v − centro) + (0,0,−dist)`, então a ordem correta é **distância
  primeiro, centralização por último**.
- **O que acontece com a ordem inversa, medido:** a sala cai atrás da câmera. O
  `zw` da modelview saiu `+2137` unidades de mundo contra um plano perto em
  `−667`, e o estágio de recorte descartou **os 510 triângulos**:
  `DL t0 c0 k510`. Tela preta, sem nenhum sintoma de textura.
- **A sala é maior do que o motor aceita cru, e o `shift` resolve.** Caixa
  medida `x −1000..1000`, `y −20..320`; raio do canto ≈ 1423, distância
  escolhida 2314 (= 1,625 × o raio), `eyeMax = dist + raio`. O menor `shift` em
  `[6,14)` que mantém `eyeMax · 2^(12-shift) ≤ 32000` foi **9**, e o alvo devolveu
  `c0` — nenhum vértice clampado. Fecha a tarefa #27.
- **`SkinMatrix_MtxFToMtx` é a autoridade sobre a ordem dos 16 inteiros** do
  `Mtx` (`src/code/z_skin_matrix.c:530`): `ip[0]=xx, ip[1]=yx, ip[2]=zx,
  ip[3]=wx, ip[4]=xy, …` — `ip` é a **transposta** do array `MtxF`, e
  `ip[12..14]` são de fato a translação. Isso valida o que `mtxToDs` faz com os
  índices 12..14, e derruba qualquer suspeita de transposição no caminho.
- **Os `G_SETTIMG` estão lá.** A primeira sub-lista da sala tem 36 comandos de
  imagem apontando para segmentos de objeto; como `object_0/1/3/20` (os ids da
  `OBJECT_LIST` da sala) não estão montados, a sala sai flat-shaded com a cor dos
  próprios vértices — `(0,120,0)` no array em 0x2F0 é o verde do piso.

### Validação

melonDS, ROM em `%TEMP%\ordsi`, builds `room1`…`room7`. Estado final (`room7`):
`RM st0 e1 sh9 d2314 22640b`, `RM x-1000..1000 y-20..320 n667`,
`DL t510 c0 k0 p1 m2/0 x0 ok`, `Bl 0/510 a31 n34 f0 p0 on`,
`F3 t4 mtx 1/1 1.5ms MtxOK`, `OoT 49/49 … dl 510 2.2ms ok`,
`FPS 30 f334.4 s23.2 w36 ms`. Tela de cima em `room7_top.png`: piso em grade,
muros leste e oeste e o quad de céu no horizonte, estáveis entre ângulos de
órbita (`room5_top`, `room6_top`). Nenhuma linha do HUD em `FAIL`/`BAD`.

### Erros meus

- **Assumi o bloqueio por três sessões.** A ROM estava em `Rom/` desde o começo;
  eu testei o MD5 do arquivo **como estava**, ele não bateu, e eu registrei
  "falta a NTSC-U 1.0" em três documentos sem testar o byte-swap que o próprio
  nome `(Rev B)` e o formato `.n64` pediam.
- **Escrevi a câmera na ordem que a minha álgebra pedia, não na que o código do
  jogo faz**, e chamei o resultado de "problema de textura". Levei quatro builds
  para ler o `zw` no HUD.
- **Publiquei números de andadores de display list em Python que não eram
  reproduzíveis.** "1016 triângulos e nenhum `G_SETTIMG`" para esta sala saiu de
  script com a ordem de campos do `Vtx` errada e com `gsSPVertex` decodificado
  sem `v0 = ((w0>>1)&0x7F) − n` (o ramo ativo do gbi é o de `F3DEX_GBI_2`).
  **Retirado.** A medida boa é a do alvo: 510 triângulos, 36 `G_SETTIMG`.
- **Linhas de HUD na 23ª linha**, que é a última da tela de baixo e sai meio
  cortada na captura. Duas iterações lendo screenshot sem enxergar o número que
  eu mesmo tinha impresso.

### Limites

Uma sala só, carregada uma vez no boot, sem portas, sem atores desenhados, sem
transição de cena. A câmera é uma órbita sintética em torno do centro da sala,
não a câmera do jogo. Nenhum `G_MTX` da lista da sala é aplicado por cima do
prologue. Os objetos não estão montados, então **nada na sala é texturizado** e
o `Tex b0 h0 x0 e0 p0 t0/0 off` do HUD é literal.

### Próximo

Tarefa #31: montar os segmentos pedidos pela `OBJECT_LIST` da sala
(`object_0`, `object_1`, `object_3`, `object_20`) para os `G_SETTIMG` resolverem,
o que abre também a endianidade de texel. Depois: câmera do jogo no lugar da
órbita, e a segunda sala — que é o teste real do carregador, porque hoje ele só
precisa acertar uma.

---

## Fase 3 — A pilha de matrizes do `sys_matrix.c` gera a modelview, e a tabela de senos do jogo não é simétrica a 45° (2026-10-27)

> **ID** `OTD-016` · **Data** 2026-10-27 · **Frente** jogo · **Fase** 3 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-016/` · **Commit** árvore sem histórico git

**Objetivo:** tirar a modelview de dentro do `guMtxIdent` do teste e colocá-la na
pilha de matrizes **do próprio jogo** — `Matrix_Init`, `Matrix_Put`,
`Matrix_Push`, `Matrix_TranslateRotateZYX`, `Matrix_ToMtx`, `Matrix_Pop` — com o
arena `THA` que o `sys_matrix.c` espera encontrar num `GameState`. Depois medir
que a faixa que sai no painel é a que aquela matriz manda desenhar.

**Entregue:**

- **`source/oot/oot_dl.c` — a pilha do jogo no caminho de verdade**
  ([:55](source/oot/oot_dl.c#L55)):
  `mtxStackInit()` abre um arena de 2048 bytes, chama `THA_Init` e
  `Matrix_Init(&sMtxState)` — os mesmos 20 `MtxF` que o jogo aloca um por ator.
  A matriz sai de `Matrix_Put(&gIdentityMtxF); Matrix_Push();
  Matrix_TranslateRotateZYX(&pos,&rot); Matrix_ToMtx(mv); Matrix_Pop();`, o
  idiomata de ator, com `Math_SinS`/`Math_CosS` e `THA_AllocTailAlign16` por
  trás. `Matrix_GetCurrent` é re-declarada localmente porque `sys_matrix.c` a
  define sem pôr no header.
- **`source/oot/oot_selftest.c` — asserções 76, 81, 82 e 83 reescritas**
  ([:250](source/oot/oot_selftest.c#L250)):
  a modelview não é mais identidade (76 lê o stack base com `Matrix_Get`), e as
  três seguintes fixam a assimetria da tabela. **49/49 no emulador.**
- **`source/nds/video_top.c` — `GRID_Z = -3072`**
  ([:7](source/nds/video_top.c#L7)):
  a grade de benchmark é um fundo, e estava a `z = 0` no meio da cena. Ela
  cobria a faixa do jogo **por profundidade**, não por superposição, desde a
  entrada anterior. Regra completa e os quatro pontos medidos estão em
  `HISTORICO_MOTOR_GRAFICO.md`, Fase 3 — "Quem manda na profundidade é o menor
  `z/w`".
- **`source/main.c`** — o comentário da ordem de desenho diz agora o mecanismo
  verdadeiro (empate de profundidade com um fundo ortho), não "foi pintada por
  cima".

### Descobertas medidas

- **A tabela de senos do jogo é assimétrica no ângulo de teste.** `sins.c` faz
  `angle >>= 4` e, se o bit `0x400` estiver ligado, lê `sintable[0x3FF - ângulo]`.
  Em binângulo `0x2000`: `sins(0x2000)` lê `sintable[0x200] = 23187` direto, mas
  `coss(0x2000) = sins(0x6000)` dobra para `sintable[0x1FF] = 23151`. **Um degrau
  de 36 em 15 bits, 72 em 16.16.** Os slots medidos no `Mtx` do jogo são
  `cos = 46303` e `sin = 46375`, e o `−sin` do outro lado da diagonal fica em
  `intPart 0xFFFF / fracPart 19161`, com `19161 + 46375 = 65536` exato. A
  asserção 82 cobra a diferença de 72; a 83 cobra o complemento. Assumir
  simetria a 45° era o erro que fazia as duas falharem.
- **A faixa medida no quadro completo, com a demonstração desenhada:** painel
  linhas **2..19**, verde acima e vermelho abaixo, e a emenda inclinada
  **8,43 colunas por linha** contra as **8,33** previstas de `Cx/Cy = 1,299/1,732`
  do `guPerspective` com o viewport da faixa — 1,2 % de desvio.
- **A lista é reconstruída a cada frame com a matemática do jogo:** duas
  capturas do mesmo build põem a emenda nas colunas ~117 e ~74, e o HUD mostra
  `e=20` e `e=44`, que é `OOT_DL_TRAVEL · sins(angle) / 32767`.
- **A escala do mundo bate com a previsão ingênua**, ao contrário do que a nota
  da sessão anterior dizia: 0,331 coluna por unidade de mundo e a faixa centrada
  na coluna 127,5. Não há nenhum fator 2 escondido no caminho; o que havia era a
  grade ortho na frente.

### Validação

melonDS 1.1, DS mode, 67 MHz. `ootmtx3` (`OoT 49/49`, `e=20` → `e=44`),
`ootmC` e `ootmC2` — quadro normal completo, com a cena de demonstração
inalterada em relação ao baseline `ootvp8` (linha 24: `R55..70 W71..97 R98..116`,
idêntica) e a faixa do jogo visível pela primeira vez sem isolar nada.
Autoteste F3DEX2 continua em `MtxOK`.

### Erros meus

- Escrevi três asserções sobre o `sys_matrix.c` **sem ler o `sys_matrix.c`**
  até o fim: uma exigia identidade onde agora há rotação, uma assumia
  `sin == cos` a 45° e uma comparava `fracPart[1][0]` com o slot do cosseno em
  vez do do seno. As três eram suposições sobre código que estava a dois
  diretórios de distância.
- Herdei do resumo anterior um número de medição (coluna 247,5 / 0,664) e o
  reconstruí em comentário antes de remedir. O comentário estava errado e só
  durou até a primeira varredura honesta.
- Aceitei por três sessões que a grade "pintava por cima" da faixa porque um
  comentário antigo dizia isso. Era teste de profundidade, e a entrada anterior
  já tinha visto — e contornado desligando a grade em vez de consertar.

### Limites

- A lista ainda é sintética: conteúdo real exige o ROM NTSC-U 1.0 (tarefa #6).
- A modelview sai de uma pilha que o jogo usa para um ator por frame. Câmera,
  `SkinMatrix_MtxFMtxFMult` com mais de um peso e a escala real do mundo
  (coordenadas do OoT passam muito de ±8 em 1.19.12) ainda não foram exercidas.
- Nada visto em DSi real (tarefa #5).

### Próximo

Primeira matriz de câmera do jogo na pilha (o `lookAt` do `camera.c` sobre a
mesma arena), e as duas pendências de motor que bloqueiam cena real: recorte de
polígono equivalente ao RSP (#26) e política de profundidade/escala com ortho e
perspectiva no mesmo frame (#27).

---

## Fase 3 — A display list montada pelo código do jogo chega ao painel (2026-10-27)

> **ID** `OTD-015` · **Data** 2026-10-27 · **Frente** jogo · **Fase** 3 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-015/` · **Commit** árvore sem histórico git

**Objetivo:** fechar o item (a) da fase anterior — ligar o `Gfx_Alloc` do decomp
ao bump pointer que o decodificador F3DEX2 consome, e provar que uma lista
escrita com as macros e os tipos do jogo (`Mtx`, `Vtx`, `Vp`, `guPerspective`,
`gSP*`, endereço relativo a segmento) não só **decodifica** como **pinta pixels
onde o N64 os pintaria**. Sem baserom, o que se pode construir é o caminho
inteiro com conteúdo sintético; o conteúdo continua bloqueado.

**Entregue:**

- **`source/oot/oot_dl.{h,c}`** ([:46](source/oot/oot_dl.c#L46)):
  um blob de 1024 bytes do próprio `Gfx_Alloc` do jogo, de onde saem as duas
  `Mtx`, o `Vp`, os oito `Vtx` e as 12 `Gfx` — todas as famílias de endereço
  segmentares, como o RSP espera. Segmento 7, separado do blob da demonstração.
  A emenda entre os dois quadrados usa `sins()` da tabela fixa do jogo, então o
  desenho muda de frame para frame e prova que a lista é reconstruída.
- **`source/oot/oot_selftest.c` — `testGameDl`**, nove asserções novas (70–78):
  o blob coube, o `Gfx` anterior à lista é o ramo `G_DL` que o `Gfx_Alloc` fecha
  por trás, lista e vértices dentro do blob, `guPerspective` com o termo de
  perspectiva em `m[2][3] = -1`, `guMtxIdent` exata em 16.16 e a emenda em
  `sins(0x2000) = 70`. Total do autoteste: **30 → 39**.
- **`source/main.c`** — a lista do jogo entra no `geDrawList` do frame, depois
  da de demonstração ([:357](source/main.c#L357)),
  e a linha 16 do HUD passou a mostrar `e<emenda> t<triângulos>` em vez dos dois
  números de matemática, que continuam no autoteste.

### Descobertas medidas

- **`Gfx_Alloc` não encosta um bloco no outro**: cada bloco devolvido é seguido
  dos 8 bytes do ramo para o próximo, então a segunda `Mtx` fica
  `sizeof(Mtx) + sizeof(Gfx)` depois da primeira. A asserção 78 mede isso.
- **`guMtxF2L` do decomp escreve a matriz como palavras de 32 bits** — suposição
  de big-endian. Em ARM9 little-endian o `Mtx` saía trocado e as duas asserções
  de conteúdo (75, 76) falharam até o layout ser tratado aqui, não no jogo.
- **`Vtx` chega inteiro ao geometry engine**: `F3Vtx` é 16 bytes com `s16 x,y,z`
  e a cor em 12..15, copiado por `memcpy` pelo decodificador — o `z = -500` do
  jogo atravessa a tradução e é ele que faz a divisão por `w` dar o valor certo
  no painel. Descartou-se assim a hipótese de colapso em `w = 0`.
- **A lista do jogo era classificada translúcida e por isso não pintava** —
  detalhe do backend registrado na série do motor gráfico, entrada
  "O viewport é dois cantos contados de baixo".

### Validação

melonDS 1.1, DS mode, 67 MHz. Captura `ootvp7` (grade do benchmark desligada
para a captura, porque ela cobre o painel com profundidade 0,5 e ganha o teste
contra os 0,75 da lista do jogo):

- `OoT 39/39 6489us` — autoteste do decomp inteiro.
- `F3 t4 mtx 1/1 1.0ms MtxOK` — decodificador sobre a lista do jogo.
- `DL t4 c0 p1 m2/1 x0 ok` — quatro triângulos entregues, nenhum descartado por
  estouro de faixa, uma projeção e duas matrizes aplicadas, pilha equilibrada.
- `Bl 0/4` — nenhum dos quatro triângulos é translúcido, como `RM_OPA_SURF` manda.
- **Pixels: painel linhas 5..16, colunas 64..117 puramente vermelhas e
  118..190 puramente verdes.** Previsto, pela projeção medida do jogo e pelo
  viewport corrigido: linhas 5..17 e colunas 64..191.

### Erros meus

- Aceitei "a lista do jogo não pinta" como conclusão três execuções seguidas,
  sem controle positivo na armação de isolação. A correção só veio quando o
  passe de verificação foi feito pela lista de demonstração, que se sabe pintar.
- Confundi profundidade com ordem de submissão: desenhei uma história sobre o
  FIFO do geometry engine a partir de capturas onde a faixa estava simplesmente
  atrás da grade.
- Anotei as linhas do painel com a origem errada (janela y 60 em vez de 56) e
  tentei encaixar o resíduo de 4 linhas no modelo do viewport, que já estava
  certo quanto ao eixo.

### Limites

- A lista é sintética: nenhuma `Gfx` veio de asset real do OoT. A tarefa #6
  continua bloqueada pelo ROM NTSC-U 1.0.
- A faixa de teste fica no topo do painel porque é a única região que a grade do
  benchmark não alcança — é escolha de instrumento, não comportamento do jogo.
- Sem recorte de polígono: os quadrados estão dentro do frustum de propósito. O
  OoT real manda geometria para fora dele o tempo todo (tarefa nova).
- O `shift` mundo→GE é fixo em 8; ainda não há política para coordenadas do
  mundo que passem de ±8 em 1.19.12.

### Próximo

Estágio de recorte no tradutor e a política de escala do mundo; em paralelo, os
`libultra/gu/mtx*.c` + `c_mtxtrans` para que as matrizes de modelview venham do
jogo em vez de identidade.

---

## Fase 3 — camada de compatibilidade Ultra64: as primeiras instruções do jogo no ROM (2026-10-27)

> **ID** `OTD-014` · **Data** 2026-10-27 · **Frente** jogo · **Fase** 3 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-014/` · **Commit** árvore sem histórico git

**Objetivo:** sair do "código nosso imitando a libultra" e entrar no "código do
jogo compilando", sem ter o baserom em mãos. Isso quer dizer escolher um fecho
pequeno de translation units do `zeldaret/oot` que compile para ARM9 sem tocar
em asset nenhum, escrever por cima as chamadas `os*` que elas pedirem, e medir o
resultado rodando de verdade no emulador — não só ligando.

**Entregue:**

- `source/oot/oot_plat.h` + `source/oot/oot_plat.c` — a costura entre os dois
  mundos de include. O cabeçalho não inclui nada e usa só tipos C puros; a
  implementação é compilada com as flags do projeto e enxerga `<nds.h>`. São
  duas funções: `ootPlatUs()` (relógio de micros com contador de volta do
  `cpuGetTiming()`, que é de 32 bits e enrola a cada ~128 s) e
  `ootPlatIrq()` (liga/desliga o `REG_IME`).
- `source/oot/oot_os.c` — `osGetTime`, `osSetIntMask`, `osGetThreadId`,
  compilados com o flag set do decomp. São os três únicos externos não-libc
  que o conjunto escolhido pede.
- `source/oot/oot_selftest.{h,c}` — 30 asserções sobre o allocator de arena do
  jogo, o `ListAlloc`, o bump pointer de `Gfx`, a matemática de ponto fixo
  (`sins`/`coss`/`Math_Atan2S`) e a de ponto flutuante (`Math_FAtan2F`,
  `Math_FAtanContFracF`, `guNormalize`), mais a volta do `osSetIntMask`.
- 9 translation units do decomp no link: `libc64/__osMalloc_n64.c`,
  `libc64/malloc.c`, `libc64/math64.c`, `code/listalloc.c`, `code/gfxalloc.c`,
  `code/sys_math_atan.c`, `libultra/gu/sins.c`, `libultra/gu/coss.c`,
  `libultra/gu/normalize.c`.
- `Makefile`: flag set do decomp separado (`OOT_CFLAGS` para o que escrevemos
  com `-Wall -Wextra`, `DECOMP_CFLAGS` com `-w` para o que é do jogo), regra
  `build/oot/%.o` que espelha a árvore `game/oot/src`, `-lm`, e a linha 16 do
  HUD reservada para o resultado.

**Descobertas medidas:**

- Varredura de compilação dos 957 `.c` do decomp com o nosso flag set: **542
  compilam**, 415 falham. Das falhas, **400** são ausência de cabeçalhos de
  asset gerados a partir do ROM e **1** é `dmadata_table_spec.h` (também
  gerado); só **15** são problemas reais de define por arquivo
  (`PLAYER_CUEID_*`, `ARRAY_COUNT`, `FaultMgr`, `CAM_DATA_MAX`, …). A
  progressão das quatro varreduras foi 474 → 499 → 535 → 542. Ou seja: a
  ausência do baserom custa 401 arquivos, não os 957.
- Defines sem os quais nada compila: `-D_LANGUAGE_C` (sem ele `u32`/`f32`/`s32`
  não existem em `ultratypes.h`), `-DPLATFORM_N64=1` (sem ele o `Arena` não tem
  `size` e o `malloc` do NTSC não casa), `-DOOT_VERSION=NTSC_1_0
  -DOOT_REVISION=0` (senão `line_numbers.h` faz `#error`), `-D_MIPS_SZPTR=32`,
  `-DNON_MATCHING`, `-DDEBUG_FEATURES=0`, `-DF3DEX_GBI_2`.
- Os dois mundos de include **não** se misturam: `game/oot/include/libc/stdint.h`
  tipa `intptr_t` como `signed long` e o newlib como `int`, o que é um conflito
  de typedef. Daí a costura em `oot_plat.h`.
- `ds_rules` linka `$(OFILES)`, não `$^` (Makefile:61 da regra `%.elf`): os
  objetos do decomp compilavam, ficavam em `build/oot/` e nunca chegavam ao ELF
  até entrarem na lista de `OFILES`.
- `__osCheckArena()` responde **1** quando um nó perdeu a mágica e **0** quando
  a lista ainda está caminhável — o inverso do booleano que o resto do teste
  usa.
- `ListAlloc_Alloc()` pendura o bloco novo em `head->next` mas encaminha
  `node->next` para o bloco alocado *depois* dele: `head->prev` é o nó mais
  antigo, e caminhar a partir de `head->next` com `->next` enxerga **um** bloco
  de oito.
- Custo do allocator do jogo, medido na linha 16: 256 pares `alloc(64)`+`free`
  em **6319 µs** (≈24,7 µs o par) em DS mode a 67 MHz. É caro demais para usar
  `SystemArena_Malloc` por frame no caminho quente; a arena linear nossa
  (`GameMem`) continua sendo a via de streaming.
- Soft-float do ARM9 (sem FPU) bate com IEEE: `Math_FAtan2F(1,1)` = **785398**
  µrad (π/4), `Math_FAtanContFracF(0,5)` = **463647**, erro de `guNormalize` em
  (3,4,0) = **0** milionésimos, `Math_Atan2S(1,1)` = **8192** exato (0x2000).
- Tamanho do ROM com as 9 units: `text 199588 / data 25804 / bss 123920`
  (349312 dec), 183 entradas de `build/oot` no map.

**Validação:** `make` no msys2 → `OcarinaDsi.nds` copiado para
`%TEMP%\ordsi` → melonDS reiniciado com o ROM como argumento → captura
PrintWindow → corte da linha 16 do HUD (`-X 8 -Y 384 -W 256 -H 8 -Scale 6`) em
`oot7_row16.png`, que lê **`OoT 30/30 6319us t785398 n0`**. Nenhum número acima
é inferido do código: todos saíram dessa tela.

**Erros meus:**

- Gastei três ciclos de build atrás de um suposto desvio de 1,9 mrad no
  `Math_FAtanContFracF` porque li `783501` numa linha do HUD que os sprites do
  inventário estavam cobrindo. O valor sempre foi `785398`. A lousa de
  diagnóstico com cinco números na mesma linha era ilegível; a lição é imprimir
  pouco quando algo falha (a forma nova, `OoT #<id>,<id> <pass>/<total>`, coube
  e leu limpo) e desconfiar de leitura de dígito sob sprite.
- Assumi que `head->next` era o nó mais antigo no `ListAlloc` e escrevi a
  travessia pelo lado errado — essa foi a única falha real da suíte (id 32).
- Primeira versão do teste chamou coisas que não existem (`Math_FSqrtF`), usou
  `SystemArena_Check()` como se devolvesse algo (é `void`) e comparou
  `__osCheckArena()` com `!= 0`.
- Adicionei os objetos do decomp como pré-requisito de `$(TARGET).elf` em vez de
  `OFILES`, o que deu `undefined reference` mesmo com o `.o` compilado.

**Limites:** nada aqui lê asset do jogo; as 9 units são um subconjunto escolhido
por fechar sob si mesmo. Não existe `osCreateThread`, `osPiReadProf`, `osVi*`
nem `osMapStack` — o `main` do decomp ainda não pode ser chamado. O portão de
hardware real continua aberto: tudo foi medido em melonDS DS mode. E os 401
arquivos dependentes de header gerado continuam esperando o baserom.

**Próximo:** usar o que acabou de ser validado em vez de só testá-lo — (a)
apontar o `Gfx_Alloc` do jogo para o mesmo bump pointer que o nosso
decodificador F3DEX2 consome, para que uma display list criada por código do
jogo entre no geometry engine sem cópia; (b) atacar os 15 arquivos com define
próprio para alargar o conjunto compilável; (c) trazer `libultra/gu/mtx*.c` e
`c_mtxtrans` para que as matrizes de projeção venham do código do jogo, não de
uma reimplementação nossa.

---

## Fase 1 — `nds/fs`: os primeiros bytes de asset entrando pelo cartão (2026-10-27)

> **ID** `OTD-021` · **Data** 2026-10-27 · **Frente** jogo · **Fase** 1 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-021/` · **Commit** árvore sem histórico git

**Objetivo:** dar ao carregador de assets um jeito de ler arquivos do cartão SD
sem nunca ter o blob inteiro na RAM — o adaptador libfat que alimenta o
`yaz0DecompressStreamed` — e medir a vazão do caminho inteiro, não do decoder
sozinho.

**Estado:** concluído e medido no emulador. [`source/nds/fs.c`](source/nds/fs.c)
existe, fecha 19 verificações, e um arquivo Yaz0 entrou pela primeira vez pelo
cartão em vez de vir de um array na ROM. Para isso foi preciso **criar** um
cartão SD no emulador: em DS mode o melonDS não tem nenhum.

### Entregue

- **[`source/nds/fs.h`](source/nds/fs.h)**
  — `NdFile` (alça + contadores) e a API `ndFsInit` / `ndFsOpen` / `ndFsRewind` /
  `ndFsReadAt` / `ndFsFill` / `ndFsWrite` / `ndFsRemove` / `ndFsSelfTest`.
- **Resolução de montagem**
  ([`fs.c:15`](source/nds/fs.c#L15)):
  tenta `sd:`, depois `fat:`, depois o caminho puro, e grava em `NdFile.mount`
  qual deles abriu. É ordem de preferência, não chute — e o `ndFsInit` monta uma
  vez só, então o
  [`bench.c`](source/nds/bench.c#L54)
  agora pergunta à mesma montagem em vez de chamar `fatInitDefault` de novo.
- **`ndFsFill` assina `Yaz0FillFn`**
  ([`fs.c:111`](source/nds/fs.c#L111)):
  lê exatamente `len` bytes e acumula `reads` / `readBytes` / `readUs` na própria
  alça. A vazão do SD sai de dentro da decodificação, não de uma estimativa
  depois.
- **Acesso aleatório**
  ([`fs.c:136`](source/nds/fs.c#L136)):
  `ndFsReadAt` só faz `fseek` quando a posição pedida é diferente da atual, por
  isso o streaming inteiro reporta **1** seek e não um por bloco.
- **Autoteste de 19 casos**
  ([`fs.c:192`](source/nds/fs.c#L192)):
  monta, escreve o fixture de 6 460 B, reabre, confere o tamanho, lê o cabeçalho
  em offset 0, reconhece `Yaz0`, tira de lá os 10 240 B declarados, aloca o
  destino, volta ao começo, decodifica em streaming com janela de 1 KB, confere o
  hash `0x91c82584`, confere que vieram exatamente 6 460 B do cartão, lê os
  últimos 16 B, varre o arquivo 32× para ter amostra de vazão, rejeita caminho
  inexistente, apaga e confirma que sumiu.
- **Asset colocado pelo host**
  ([`fs.c:267`](source/nds/fs.c#L267)):
  se `/ocarina.y0` existir no cartão, o teste consome ele também. É o mesmo
  blob, mas escrito por uma ferramenta de fora — o formato que a sala real vai
  ter. A estrela na linha do HUD diz que ele estava lá.
- **HUD**
  ([`main.c:89`](source/main.c#L89))
  — linha 7. As duas linhas antigas de Yaz0 viraram uma
  ([`main.c:63`](source/main.c#L63))
  para liberar a 7, porque a 23 não existe (ver Descobertas).

### Descobertas medidas

| Descoberta | Número | Como se sabe |
|---|---|---|
| melonDS em DS mode não tem SD; o driver DLDI embutido resolve | `fatOk` passa de `false` a montado só de ligar `[DLDI] Enable` | captura `fs0` (linha 7 ainda vazia, cartão ausente) contra `fs1` (`FS 17/17`) |
| Vazão sequencial real do caminho de asset | **11,6 MB/s** nos 206 720 B da varredura (≈17 ms) | `fs7`: `FS 19/19 * sd11.6/dec2.9 MB/s` |
| Uma leitura só de 6 460 B superestima | 14,4 MB/s, **+24 %** contra a varredura | `fs6`: `FS 18/18 * sd14.4/...` |
| Ler do SD quase não custa nada além do decoder | 2,9 MB/s fim a fim contra 3,2 MB/s do mesmo streaming em RAM (≈3,4 ms por asset) | `fs7` linha 6 × linha 7 |
| O console de texto do `consoleDemoInit` mostra 23 linhas, não 24 | `line(23,…)` nunca apareceu na captura, com a última linha visível sendo `Bl …` (índice 22) | `fs0_hud.png` |
| melonDS ignora `SDPath`; a chave é `ImagePath`, e o cartão padrão é `dldi.bin` no diretório do próprio melonDS | `dldi.bin` + `dldi.bin.idx` ("SIZE 536870912") criados ao ligar o DLDI | `ls` no diretório do executável |

### Validação

Todas as capturas abaixo são do mesmo ROM em melonDS (DS mode, ARM9 a 67 MHz),
linhas 4–7 do HUD recortadas em 3× da região (8,288)–(264,320).

| Captura | Linha do FS | O que provou |
|---|---|---|
| `fs0_hud.png` | (linha 7 em branco) | a linha escolhida não é renderizada — console tem 23 linhas |
| `fs1_hud.png` | `FS 17/17 sd14.2/dec3.2 MB/s` | SD montado por DLDI; escrever, reabrir, decodificar do cartão, ler em offset, rejeitar ausente, apagar |
| `fs3_fs.png` | `FS 17/18 BAD #13` | o arquivo do host foi **aberto**, mas a verificação falhou |
| `fs4_fs.png` | `FS 17/18 BAD #13 h4 59617A30` | mágica lida do host = `Yaz0` correta; falha é depois da decodificação |
| `fs5_fs.png` | `FS 17/18 BAD #13 h4 r1024 91C8` | só 1 KB foi lido e o "hash certo" era o buffer da decodificação anterior |
| `fs6_fs.png` | `FS 18/18 * sd14.4/dec3.0 MB/s` | com o rewind corrigido, o asset vindo do host decodifica igual ao escrito pelo app |
| `fs7_fs.png` | `FS 19/19 * sd11.6/dec2.9 MB/s` | amostra de vazão de 206 KB; nada regrediu no resto do HUD |
| `fs7_top.png` | — | demo 3D intacto: `DL t4 c0 p2 m9/7 x0 ok`, `Cut 4/22 at1 e0 c0`, `Bl 4/30 a8 n2 f11 p4 on`, 61 fps |

### Erros meus

1. **Inventei a chave `SDPath`** em vez de ler as chaves reais do binário do
   melonDS. O emulador montou `dldi.bin` dele, o app falou um cartão vazio, e o
   arquivo que eu tinha copiados para a minha imagem nunca esteve lá. `grep -a`
   no executável me dava `DLDI.ImagePath` / `DLDI.FolderSync` na hora; custou uma
   rodada de emulador a mais.
2. **Esqueci o `ndFsRewind` no caminho do host.** O teste do arquivo próprio
   tinha rewind (era o caso 9); no do host eu li o cabeçalho, deixei a alça em 16
   e mandei o decoder ler de 16 como se fosse de 0.
3. **Confirmei o bug acima com um hash que não significava nada.** O `yaz0Hash`
   era calculado mesmo quando a decodificação falhava, sobre um `dst` que ainda
   tinha a saída da decodificação anterior — `91C8` bonito e falso. Agora o hash
   só é calculado se a decodificação voltar verdadeira.
4. **Assumi 24 linhas de console** porque 256×192 ÷ 8 = 24. A linha 23 não é
   desenhada. Foi a segunda vez nesta fase que uma suposição sobre a janela de
   captura custou uma rodada; a primeira foi a origem Y do painel de baixo.
5. **Quase publiquei 14,4 MB/s como taxa do cartão.** É a taxa de uma leitura de
   6 460 B. A varredura de 206 KB diz 11,6 MB/s, 24 % mais baixa.

### Limites

- **11,6 MB/s é o número do emulador**, onde cada leitura virou acesso a arquivo
  no disco do PC. Em DLDI de flashcart real a ordem de grandeza é ~1 MB/s, e no
  SDHC de DSi é outra. O orçamento de sala tem que ser planejado pelo pior caso,
  não por este.
- O autoteste escreve e apaga o próprio arquivo de teste no cartão. Num DSi real
  isso é uma gravação de 6 KB toda vez que o programa liga.
- `/ocarina.y0` é opcional: sem ele o teste fecha 18/18 e a estrela não aparece.
  Não existe ainda listagem de diretório (`opendir`), nem cache, nem leitura em
  segundo plano — cada sala vai pagar as leituras de 1 KB no laço.
- O SD bench de 1 MB (`BTN_A`) continua **sem medição**: o script de captura não
  aperta tecla, e o melonDS está com o teclado sem mapeamento para os botões.
- Verificação em DSi real (tarefa #5) continua aberta para esta fase também.

### Próximo

Motor gráfico: uma sala real referencia mais texturas distintas do que o cache de
16 slots, e `libnds` não expõe `glFinish` nem bit de FIFO vazio — o despejo de
VRAM no meio do frame é o próximo bloqueante medido. Do lado do jogo, o que falta
para o decomp andar é o ROM NTSC-U 1.0 como baserom; com ele, o `fs` daqui já é o
canal por onde os `.arc` entram.

---

## Fase 1 — Entrada: o DS não tem analógico, e o contrato do OoT espera um (2026-10-27)

> **ID** `OTD-020` · **Data** 2026-10-27 · **Frente** jogo · **Fase** 1 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-020/` · **Commit** árvore sem histórico git

**Objetivo:** trocar o `padmgr`/libultra por uma camada própria que entregue ao
jogo exatamente o formato que `game/oot` já consome — o bitmask `BTN_*` e o par
de structs `Input`/`OSContPad` — a partir do hardware do DS: oito teclas,
D-pad e tela de toque.

**Estado:** concluído e medido no emulador. `source/nds/input.c` existe, tem
autoteste de 31 casos e é o único leitor de teclado/toque do programa: o demo
agora consome `BTN_A`/`BTN_L`/`BTN_R`/`BTN_START` e o `rel.stick_x` vindo dela.

### Entregue

- **[`source/nds/ndinput.h`](source/nds/ndinput.h)**
  — os 14 `BTN_*` copiados de
  `game/oot/include/controller.h`
  com os mesmos valores, e o espelho de layout de `OSContPad`/`Input`
  (`NdPad`/`NdInput`). O quarto byte de `NdPad` se chama `err` só porque libc
  define `errno` como macro; o deslocamento é o mesmo.
- **[`source/nds/input.c:15`](source/nds/input.c#L15)
  `keyMap`** — dez teclas físicas: `A B X→Z START` e o D-pad inteiro, `L`, `R`.
  `SELECT` e `Y` ficam sem uso por enquanto.
- **Zonas de toque** ([`input.c:67`](source/nds/input.c#L67)
  `ndInputZone`): disco de stick em (56,124) e cruzeta de C-botões em (152,124).
  O stick aceita toque até 64 px do centro (`ND_STICK_GRAB`) mas só é linear até
  36 px (`ND_STICK_R`); o anel entre um e outro trava no fundo de curso.
- **Mapeamento do stick** ([`input.c:96`](source/nds/input.c#L96)):
  o vetor é clampado ao círculo por `isqrt` inteiro (sem FPU) e escalado a
  ±67. O eixo Y inverte, porque a tela cresce para baixo e o manche do N64
  cresce para cima.
- **Deadzone e `rel`** ([`input.c:125`](source/nds/input.c#L125)
  `relAxis`): transcrição literal de `PadUtils_UpdateRelXY`
  (`game/oot/src/libu64/pad.c:82`),
  que morre em |v| ≤ 7 e satura em 0x43 − 7 = 60. É o `rel` que
  `Lib_GetControlStickData` lê, não o `cur`.
- **`press`/`rel` por borda** ([`input.c:144`](source/nds/input.c#L144)
  `ndInputStep`): mesma aritmética do caminho de jogo de
  `PadMgr_RequestPadData`
  (`game/oot/src/code/padmgr.c:455`)
  — `diff = prev ^ cur`, `press = cur & diff`, `rel = prev & diff`.
- **Overlay medível** ([`ui_bottom.c:195`](source/nds/ui_bottom.c#L195)
  `bottomInputInit` / [:229](source/nds/ui_bottom.c#L229)
  `bottomInputShow`): anel do stick, cruzeta de C, botão de curso e ponto aceso
  no braço pressionado, todos como sprites do sub engine.
- **Fantasma autorrespondida** ([`main.c:264`](source/main.c#L264)):
  até o primeiro toque de verdade, o HUD e o overlay mostram a deflexão
  *roteirizada* do autoteste. Sem isso nenhuma captura prova nada, porque o
  script de captura não sabe apertar tecla.
- **Demo dirigido pela camada** ([`main.c:245`](source/main.c#L245)):
  `BTN_A` roda o benchmark de SD, `BTN_L`/`BTN_R` mudam a carga de quadriláteros,
  `BTN_START` sai, e o giro automático da câmera dá lugar a
  `angle += rel.stick_x * 8` sempre que o stick — do toque **ou** do D-pad —
  estiver defletido.

### Descobertas (medidas, não deduzidas)

| Hipótese inicial | Realidade |
|---|---|
| O manche do N64 chega em `s16` com curso de ±80 | **Não.** `OSContPad` é `{u16 button; s8 stick_x; s8 stick_y; u8 errno}` — 6 bytes, e o campo se chama `button`, não `buttons` (`ultra64/controller.h:127`). O curso útil é ±67 e o resto é deadzone de 7 |
| Um toque fora do raio linear do stick é caso morto do clamp de borda | **Não era alcançável**: `ndInputZone` exigia `mag ≤ ND_STICK_R`, então o clamp dentro de `stickFromTouch` nunca rodava. O autoteste pegou isso no caso #20, e o motivo pelo qual o caso existia |
| O nome `input.h` está livre no projeto | **Não.** libnds tem `nds/input.h` (o próprio `scanKeys`/`touchRead`). O header próprio passou a `ndinput.h` para não depender da ordem dos `-I` |
| A tela de baixo começa em Y=256 na captura de 272×448 | **Y=248.** Dois sprites de tamanhos diferentes (16×16 da grade e 32×32 do anel) mediram o mesmo desvio de 8 px — se a coordenada OAM fosse o centro, os desvios seriam 8 e 16. Então o `x,y` do `oamSet` é canto **superior esquerdo** e o que estava errado era a constante do script de captura, que há várias fases cortava a linha 0 do HUD |

### Validação

ROM `in4` (melondDS 1.1, DS mode, 67 MHz). Captura `in3`, origem da tela de
baixo em (8, 248):

| Medição | Esperado | Medido |
|---|---|---|
| Autoteste de mapeamento | 31/31 | `InMap 31/31` ✓ |
| `sizeof(NdPad)`, `sizeof(NdInput)` | 6, 24 (igual a `Input`) | `sz6,24` ✓ |
| Fantasma: toque em (80,108) → stick | (+45,+30) | HUD `S+45,+30` ✓ |
| Fantasma: `rel` do mesmo toque | (+38,+23) | HUD `R+38,+23` ✓ |
| Anel do stick (32×32 em OAM 40,108) | caixa (48,356)-(79,387) | medido (48,356)-(79,387), 156 px `#52D6D6` ✓ |
| Cruzeta de C (32×32 em OAM 136,108) | caixa (144,356)-(175,387) | medido (144,356)-(175,387), 231 px `#C552D6` ✓ |
| Botão do stick: (56+8, 124−5) − 4 | caixa (68,363)-(75,370) | medido (68,363)-(75,370), 47 px `#D6FF52` ✓ |
| Ponto aceso do `BTN_CUP` (braço de cima) | caixa (156,356)-(163,363) | medido (156,356)-(163,363), 47 px `#FF31FF` ✓ |
| Zona fora das duas (`Touch 0,0 z0`) | `z0` = ND_ZONE_NONE | ✓ |
| Tela de cima sem regressão | sonda de blending igual | `Bl 4/30 a8 n2 f11 p4 on`, `Cut 4/22 at1 e0 c0` ✓ |

### Erros meus

1. **Escrever o autoteste sem índice de falha.** A primeira versão dizia só
   `28/29 BAD`; foi preciso reler 29 casos à mão. A segunda versão imprime
   `BAD#20`, e o caso #20 é exatamente o que estava errado. Um contador que não
   diz *qual* falhou custa o dobro.
2. **Escolher o caso de teste pelo desenho, não pelo código.** O ponto
   "fora do raio linear" foi posto em (128,124) — dentro da cruzeta de C, que é
   testada antes. A falha era real (o clamp era inalcançável) mas o teste
   apontava para o lugar errado; custou um ciclo de build inteiro.
3. **Confiar na origem de captura herdada.** O `Y=256` do `rerun.ps1` veio de
   fases anteriores e nunca foi conferido; ele cortava a linha 0 do HUD em
   todas as capturas já tiradas.
4. **Colidir com um header da biblioteca.** `input.h` era o nome óbvio e já
   existe em libnds. Só percebi ao listar a árvore de includes.
5. **Editar e deixar sobra.** A substituição de `bottomInputShow` deixou três
   linhas órfãs do código antigo no arquivo; pego na releitura, não no
   compilador (o `-Wall` não vê o que não é mais instrução válida em contexto).

### Limites

- O `errno` de `OSContPad` não tem equivalente no DS: sem controle, sem erro.
  O campo existe por layout e fica 0.
- O D-pad vira manche por padrão (`dpadStick`). No OoT isso significa que
  "segurar para cima" chega ao jogo como deflexão *e* como `BTN_DUP`; a
  ocarina usa o segundo. Quando o decomp ligar, vai precisar desligar
  `dpadStick` nas cenas onde o D-pad é botão e não manche.
- `SELECT` e `Y` não têm destino no contrato de 14 botões do OoT; vão sobrar
  para a casca (menu, mapeamento).
- O toque não tem repetição por pressão contínua nas zonas: um dedo parado no
  stick produz o mesmo `cur` todo tick, e `press` só dispara em borda — que é
  o comportamento do N64, mas significa que a UI de inventário não pode usar
  `press` para "segurar e arrastar".
- O autoteste roda na ROM e cobre o mapeamento; **não** cobre o caminho
  `scanKeys()`/`touchRead()` → `ndInputStep`, que continua dependendo de alguém
  apertar uma tecla. O `'g'`/`'l'` no fim da linha 13 do HUD diz qual dos dois
  estados está sendo mostrado.

### Próximo

`nds/fs.c` — leitura dos assets do SD sobre libfat, com o `Yaz0` já medido em
3,5–3,9 MB/s em DS mode. É o último bloco da Fase 1 que não depende do ROM
NTSC-U 1.0.

---

## Fase 1 — Base do loop: concluída no emulador; portão de hardware pendente (2026-10-26)

> **ID** `OTD-002` · **Data** 2026-10-26 · **Frente** jogo · **Fase** 1 · **Estado** implementado · testado em emulador · **Evidência** `evidencias/OTD-002/` · **Commit** árvore sem histórico git

**Objetivo:** rodar o loop do jogo (zeldaret/oot) sobre a camada libnds, sem 3D.

**Estado:** os blocos de base (tempo, escalonador, memória, decompressor,
sonda) estão prontos e medidos no emulador, e o mapeamento de entrada saiu na
seção acima. O que falta é a camada `nds/fs` (libfat) — e a verificação em
DSi real.

### Entregue

- **Código-fonte do decomp**: clone raso de `zeldaret/oot` em `game/oot`, pinado
  no commit `14759abf9abb4295de142cdc1452998630ea70fb` (main, 18/10/2026).
  60 MB, 3.319 arquivos. É um repositório aninhado: ao versionar o projeto,
  tratar como submodule ou ignorar.
- **Yaz0 em memória** (`source/nds/yaz0.c`): decodificador com validação de
  magic, tamanho declarado, backref fora da janela e cópia além do destino.
  Formato idêntico ao `src/boot/yaz0.c` do decomp (cabeçalho de 16 bytes,
  tamanho big-endian, cópia longa `0x12+N`).
- **Yaz0 em streaming** (`yaz0DecompressStreamed`): consome o blob em blocos
  (janela de 8 B a 1 KB) sem manter tudo em RAM, pronto para receber leituras
  de SD na próxima etapa. O caminho rápido do refill é embutido no laço;
  só a troca de bloco chama função.
- **Vetor de teste próprio** (`tools/yaz0gen.mjs` + `source/nds/yaz0_test_data.h`):
  gera um blob de 10 KB → 6,3 KB cobrindo literais, cópias curtas, cópias
  longas e distância 2048; valida o resultado por hash FNV-1a. Um decoder
  independente em Node confere o mesmo hash fora da ROM.
- **Correção de telemetria**:
  - contador de ticks de 32 bits estoura a cada 128,2 s; a conversão para µs
    passou a ser feita **depois** da subtração, senão um frame a cada 128 s
    virava ~4,17 s (`worst` falso);
  - a medição de frame passou para o fim da iteração, incluindo a escrita de
    texto no console (antes reportava 15,0 ms em um jogo de 16,7 ms).
- **Escalonador de 20 Hz** (`source/nds/sched.c`): contador incrementado por
  IRQ de VBlank e `schedFrame()` convertendo o delta real de VBlanks em ticks
  de 3 quadros (20 Hz). Um quadro perdido devolve mais de um tick, então a
  lógica não deriva nem desacelera; a taxa real é medida a cada segundo.
  `swiWaitForVBlank()` continua funcionando com o handler próprio instalado.
- **Telemetria de lógica**: linha `Logic 20 Hz ticks N` no console, e o
  `perfConsume` deixou de receber o `Perf` (só o acumulador e a frequência).
- **Arena de memória do jogo** (`source/nds/gamemem.c`): bloco linear único
  retirado do heap do libnds no boot, com alinhamento de 32 B (exigido pelo
  geometry engine/DMA), `gameMemAlloc` power-of-two, `gameMemReset` e pico
  acumulado. Reserva 384 KB para newlib/libfat/console. Mede o heap pelo
  `sbrk(0)` **antes** de qualquer alocação; `mallinfo()` do newlib desta
  toolchain devolve zeros e foi descartado.
- **Sonda de RAM sem malloc** (`probe.c`): passou a testar o próprio bloco da
  arena, então o resultado descreve a memória que o port vai usar e não altera
  mais a medição de heap livre.
- **Decisão de pipeline confirmada**: 3,5–3,9 MB/s de Yaz0 é suficiente para
  cargas frias, mas não para streaming por sala. Assets vão para o SD já em
  formato DS, sem Yaz0, no caminho quente.

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Teste | Resultado |
|---|---|
| Yaz0 em memória | OK, 10.240 B, 2.511 µs (**3,9 MB/s**) |
| Yaz0 em streaming, janela de 1 KB | OK, 10.240 B, 2.723 µs (**3,5 MB/s**) |
| Yaz0 em streaming, janela de 8 B | OK (todo token cruza um refill) |
| Escalonador 20 Hz | 1.231 ticks em ~61,5 s de janela medida (**20 Hz**), FPS 60 |
| Arena do jogo | OK, **3,3 MB** alinhados, autoteste em ~1 µs, heap restante 384 KB |
| Sonda de RAM na arena | OK, 3.448 KB em 0,7 ms |
| Regressão de telemetria | após vários minutos, `frame 16,7 ms`, `worst 29 ms` |

### Limites e pendências

- Medição só em DS mode; no DSi real (134 MHz) os números devem subir, mas
  precisam ser refeitos.
- Nenhuma leitura de SD no decodificador ainda: o adaptador de arquivo entra
  junto com a camada `nds/fs`.
- Build de referência do decomp **não validado**: exige o ROM NTSC-U 1.0 do
  usuário como `baserom`, que ainda não está no projeto. Sem ele também não há
  assets para o `dsbake`.
- Ainda não implementados: adaptador libfat do decodificador (camada
  `nds/fs`) e o resto da substituição da libultra (`osCont*`, `osCreateThread`
  e companhia). O mapeamento de entrada saiu na seção de 2026-10-27.

---

## Fase 0 — Concluída no emulador; portão de hardware pendente (2026-10-26)

> **ID** `OTD-001` · **Data** 2026-10-26 · **Frente** jogo · **Fase** 0 · **Estado** implementado · testado em emulador · portão de hardware bloqueado · **Evidência** `evidencias/OTD-001/` · **Commit** árvore sem histórico git

**Objetivo:** provar que o DSi entrega o ambiente necessário (DSi mode, 16 MB,
clock, SD, duas telas).

### Entregue

- **Toolchain**: devkitARM r68-1 (GCC 16.1.0), binutils 2.46.0, libnds 2.0.2,
  libfat-nds, maxmod-nds, ndstool 2.3.1 e runtime Calico 1.2.0, instalados via
  pacman do MSYS2 com os repositórios `dkp-libs` e `dkp-windows` (nenhum
  instalador baixado foi executado).
- **Esqueleto do projeto** (`Ocarina Dsi/`): Makefile próprio sobre
  `ds_rules`, com `LD := $(CC)` e `LIBPATHS` definidos antes do include.
- **Sonda de hardware** (`source/nds/probe.c`): detecta DSi mode, lê
  `SCFG_CLK`/`SCFG_EXT`, pede clock de 134 MHz, reporta início/fim/limite do
  heap e roda um teste de padrão esparso em até 12 MB (3 palavras por página de
  4 KB, detectando espelhamento).
- **Benchmark de SD** (`source/nds/bench.c`): escrita/leitura de 1 MB com
  verificação de conteúdo, disparado pelo botão A.
- **Base de tempo em timer de hardware** (`source/nds/timebase.c`): o
  `tickGetCount()` do Calico não avança nos timers disponíveis; a base usa
  timer 0/1 em cascata, com a subtração feita em ticks.
- **Overlay de performance** (`source/nds/perf.c`): FPS, tempo de frame, pior
  frame e custo de submissão da cena 3D.
- **Vídeo**: main engine em `MODE_0_3D` desenhando uma grade de 1.024 quads
  (2.048 triângulos, o limite do hardware) em ponto fixo; sub engine em
  `MODE_0_2D` com console e grade 3×3 de sprites 16×16 com paleta por item.
- **Toque**: cursor como OBJ e hit-test que seleciona a célula do inventário.
- **ROM**: `OcarinaDsi.nds`, build limpo com `-Wall -Wextra`.

### Validação (melonDS 1.1, **DS mode**, 67 MHz)

| Métrica | Resultado |
|---|---|
| Main engine, 2.048 triângulos | 60 fps, frame 16,7 ms, pior quadro 29 ms |
| Custo de CPU para submeter a cena | ~0,4 ms por frame |
| Sub engine (console + sprites + cursor) | sem corrupção |
| Toque | célula (3,3) → item 8 (correto) |
| DSi mode | **NO** (esperado em DS mode) |
| SCFG_CLK | `0000 → 0000` (boost negado) |
| Heap | `0x023F0000` = 3,9 MB (limite de DS mode) |
| Teste de padrão de RAM | 3.848 KB OK em 0,7 ms |
| Benchmark de SD | não executado no emulador (sem SD montado) |

### Limites e pendências

- **Portão de hardware aberto**: falta rodar no DSi real via TWiLight /
  nds-bootstrap e confirmar `DSi mode: YES`, heap ≈ 15,9 MB, bit de 134 MHz,
  SD ≥ 1 MB/s e as duas telas simultâneas. Sem DSi mode o projeto para.
- Emulador só em DS mode; melonDS desta instalação não tem BIOS/firmware nem
  teclado mapeado (`melonDS.toml` com bindings `-1`), então só o toque foi
  testado ali.
- DeSmuME 0.9.13 não inicia homebrew baseado em Calico — descartado.


## Fases do jogo ainda não iniciadas

| Fase | Conteúdo | Portão |
|---|---|---|
| 1 (resto) | Camada `nds/fs` sobre libfat, mapeamento de entrada N64 → DS, substituição da libultra e build de referência do decomp | sala inicial carregando do SD |
| 4 | Atores, animação, câmera, colisão, streaming de cena | Hyrule Field < 15 fps → cortar |
| 5 | HUD, texto, mapa, ocarina em 256×192 | sem clipping |
| 6 | Inventário 2D na tela de baixo (4 páginas, touch) | equipar/usar tudo |
| 7 | Áudio (seqplayer/síntese, 16 canais) | sem estalos, ≤ 25% do ARM9 |
| 8 | Cobertura 100%, otimização, save, release | título → créditos |
