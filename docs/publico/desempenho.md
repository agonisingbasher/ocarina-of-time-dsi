# Desempenho: o que cada número significa

Toda comparação deste projeto usa o mesmo ambiente, a mesma câmera e o mesmo build de referência. Sem isso, dois números lado a lado não comparam nada — foi exatamente o erro que a fase 7 do histórico do jogo encontrou e corrigiu.

## Indicadores

| Indicador | Definição | Como é medido | O que não é |
|---|---|---|---|
| **FPS exibido** | quadros terminados na última janela de 1 segundo | `perfEnd` conta iterações; quando o relógio de barramento fecha 1 s, o contador vira `fps` e recomeça. Inteiro, impresso na linha 8 e atualizado no ecrã a 4 Hz | não é média histórica; não passa de 60 porque cada iteração termina em `swiWaitForVBlank()`; esconde variação dentro da janela |
| **Taxa de simulação** | ticks de lógica por segundo | `schedFrame` lê o contador de IRQ de VBlank e faz `elapsed / 3` (3 VBlanks por tick, 20 Hz); a taxa é recalculada a cada segundo | não é taxa de desenho: um quadro pode devolver 0, 1 ou mais ticks, e a lógica continua a 20 Hz com o render a 19 FPS |
| **Tempo de quadro (`f`)** | intervalo entre duas chamadas a `perfEnd`, ou seja a última iteração inteira | timer de hardware em cascata; subtração antes da conversão para microssegundos; inclui entrada, render, texto do HUD, lógica e espera de VBlank | não é média nem tempo de GPU; inclui trabalho fora do render |
| **Pior quadro (`w`)** | maior `f` desde o boot | marca d'água atualizada em `perfEnd`, nunca zerada | não é o custo atual: inclui os quadros de carregamento da sala |
| **Tempo de render (`s`)** | tempo de CPU entre o início da montagem da cena e depois de `glFlush(0)` | par de leituras do timer em volta de `topRender`, do decodificador e do `geDrawList` | **não inclui o rasterizador** nem a espera de VBlank; o custo de preencher pixel não aparece aqui |
| **Tempo de submissão** | parcela do render gasta enviando matrizes, vértices e texturas | acumulador das chamadas de envio | no emulador, acesso a MMIO tem custo diferente do hardware; serve para comparar builds |
| **Trocas de textura (`r`)** | mudanças de id de textura ligado no quadro | contador `texRebinds` na linha 22 | não conta uploads nem rasterização; um id diferente não implica upload |
| **Cache de textura (`Tex b/h/x/e`)** | uploads, acertos, recusas e despejos por quadro | contadores na linha 19 | `b0` não quer dizer "sem textura": quer dizer que tudo acertou o cache |
| **Grão do painel** | média da diferença de luminância entre pixels vizinhos no painel 3D | `ferramentas/speck2.py` sobre o recorte `_topbig` | não é qualidade visual universal; textura detalhada e borda também aumentam o número |
| **Contagem** | eventos instrumentados: triângulos, binds, uploads, acertos, recusas, despejos | contadores impressos nas linhas 18..22 | contagem não mede tempo; dois contadores que sobem juntos não provam causalidade |

### Por que os tempos não são intercambiáveis

`f` é o período do loop; `s` é o trabalho de CPU do render; a submissão é uma parte de `s`. Nenhum dos três inclui o rasterizador. Quando a cadeia de mip foi ligada, `s` subiu ~20 ms e o FPS caiu de 20 para 19 — o loop inteiro passou a caber em menos quadros, mas o número que explica a causa é `s`, não o FPS arredondado.

Os números **não coincidem por construção**, e não são ajustados para coincidir. Numa captura do build entregue: `FPS 19`, `f50,1`, `s48,8`, `w349`. `1000/50,1 = 19,96` é truncado para 19 na janela de 1 s, então o FPS inteiro e o período do loop contam a mesma coisa em escalas diferentes; os 1,3 ms entre `f` e `s` são entrada, texto do HUD, lógica e o resto da espera de VBlank; `w349` é um recorde de carregamento, não o custo de repouso. O FPS também não passa de 60 porque cada iteração termina em `swiWaitForVBlank()`, e o período fica quantizado em múltiplos de ~16,7 ms.

A taxa de simulação é independente: o loop de lógica roda a 20 Hz mesmo quando o desenho cai. Por isso uma demonstração com FPS baixo ainda pode ter lógica correta, e vice-versa.

## Condição da medição

| Item | Valor |
|---|---|
| Emulador | melonDS 1.1, **modo DS**, 67 MHz |
| Janela | 272×448; painel 3D em X 8, Y 56, 256×192 |
| Cena | `Bmori1_room_0`, 646 triângulos, 75 imagens, 2 paletas |
| Câmera | presa por `ROOM_SPIN 0` / `ROOM_ANGLE 0` em `source/main.c` |
| Filtro de textura | `GE_TEX_MIP_MAX` conforme a linha; `GE_TEX_LOD_BIAS 2048` |
| Instrumentação | linha 8 da consola (`f`, `s`, `w`); contadores nas linhas 18..22 |
| Regime | após o carregamento da sala e a estabilização do loop; não são os primeiros quadros |
| Coleta | captura da janela, recorte com `ferramentas/crop_rect.ps1` (vizinho mais próximo, 2×) e leitura da linha 8 |
| Repetibilidade | duas capturas do mesmo build e ângulo dão o mesmo md5 do painel; cada linha da tabela A/B foi medida em dois builds e dois quadros |

Não existe versão numerada do port: a árvore não tem histórico git. O identificador do build é o `OTD-NNN` da entrega, e a condição de cada linha está declarada nas tabelas e nos resumos.

## A/B da cadeia de mip

### Medição atual (2026-10-27, com sombreamento por vértice e lerp no build)

Ângulo preso, quatro builds na mesma linhagem, duas capturas por build; os dois quadros de cada build dão o mesmo md5:

| Configuração | Grão do painel | FPS | `f` (ms) | `s` (ms) | `r` | Custo / ganho |
|---|---|---|---|---|---|---|
| cadeia desligada (`MAX 0`) | **4,22** | 20 | 33,4 | 32,4 | 136 | referência |
| **teto 2 (entregue)** | **2,91** | 19 | 50,1 | 48,8 | 247 | **−31 % de grão por +16,4 ms** (34 % do render) |
| teto 2 + seletor calculado e descartado | 4,22 | 19 | 50,1 | 48,1 | 136 | paga a aritmética, não usa o nível |
| teto 2 + uma troca forçada por triângulo | 2,91 | 19 | 50,1 | 49,1 | 909 | +0,3 ms por 662 trocas, **0 px de diferença** |

O grão absoluto desta tabela **não é comparável** com o da tabela histórica abaixo: o painel deixou de ser pintado de branco quando o build passou a usar a cor de vértice. O que se compara entre as duas é a razão do A/B, e ela se repete: −31 % agora, −32 % antes.

### Decomposição do custo da cadeia

| Parcela | Como foi isolada | Valor |
|---|---|---|
| aritmética do seletor | build que calcula o nível e descarta | **15,7 ms** (96 %) |
| escolha do nível, busca no cache e bind | entregue menos o build acima | **0,7 ms** |
| troca de textura | uma ligação forçada por triângulo, mesmo id e mesmos pixels | **0,45 µs por troca**; 662 trocas = 0,3 ms |
| upload de textura | `Tex b0` nos quatro builds em repouso | **0 por quadro** (o cache acerta tudo) |

A conclusão da fase 8 — "a otimização que paga é agrupar primitivos por nível e cortar os re-binds" — **foi refutada**: com 0,45 µs por troca, as 111 trocas de nível por quadro valem no máximo 0,7 ms, 4 % do custo da cadeia. O custo está na conta por face, como a fase 9 já dizia.

### Compromisso em avaliação

A cadeia continua ligada porque 20 → 19 FPS é um quadro e o grão cai 31 %, mas **34 % do tempo de render para um ganho medido numa sala e num ângulo** não é conclusão fechada. O build sem cadeia (`MAX 0`) fica declarado como alternativa, com o mesmo painel do build entregue antes da cadeia. A leitura visual acompanha o número: com a cadeia, os tijolos da parede leem-se como tijolos; sem ela, a parede é aresta de ruído. O ganho e o custo aparecem juntos porque um não existe sem o outro.

### Medição histórica (fase 7, painel branco)

| Configuração | Grão do painel | FPS | `f` (ms) | `s` (ms) | Custo / ganho |
|---|---|---|---|---|---|
| cadeia desligada (`MAX 0`) | **13,28** | 20 | 33,4 | 30,0 | referência |
| cadeia com teto 2 (`MAX 2`) | **9,09** | 19 | 50,1 | 49,6 | **−32 % de grão por +19,6 ms** |
| cadeia com teto 3 (`MAX 3`) | 9,06 | 15 | 66,8 | 50,7 | 0,03 de grão a mais por 4 FPS |

O teto 2 foi escolha medida, não preferência: o terceiro nível compra 0,03 de grão e custa 4 FPS. A tabela foi medida **antes** da otimização por tabela de recíprocos; o build entregue depois dela mediu `s 47,5` com o mesmo grão 9,09 e o mesmo md5 de painel.

## De onde vinha o custo (investigação da fase 7, preservada)

O primeiro diagnóstico atribuiu o custo à troca de textura. Um build de controle calculou o nível e descartou o resultado, mantendo o seletor e eliminando os binds:

| Build | O que faz | `s` (ms) |
|---|---|---|
| sem seletor (`MAX 0`) | desenha sem cadeia | 30,0 |
| seletor calculado e descartado (`GE_TEX_MIP_NULL`) | paga a aritmética, não troca textura | 49,9 |
| projeção com multiplicações de 32 bits | menos precisão, sem as duas multiplicações de 64 bits | 40,1 |
| uma multiplicação de 64 bits por canto | variante intermediária | 43,5 |
| fórmula exata | entrega da fase 7 | 49,6 |
| **tabela de recíprocos** | **entrega da fase 8** | **47,5** |

O build com resultado descartado custa o mesmo que a cadeia inteira: os binds extras custam dentro do ruído. As três divisões por face valiam ~4 ms, e a tabela devolveu 2,1 ms sem perda visual mensurável. A medição de 2026-10-27 confirma a leitura com um número por troca: 0,45 µs.

## A métrica de grão

**Definição.** Média do módulo da diferença de luminância entre pixels vizinhos horizontais no painel 3D. É um indicador de ruído de alta frequência, não de cor.

**Script.** [`ferramentas/speck2.py`](../../ferramentas/speck2.py). Entrada: o recorte `_topbig`. O script remove a duplicação da ampliação 2×, converte cada pixel para luminância `0,30R + 0,59G + 0,11B`, ignora pares em que ambos os pixels têm luminância abaixo de 8 e devolve a média de `|lum(x) − lum(x+1)|`.

**Limitações.**

- só luminância, sem calibração publicada contra referência externa;
- ignora pares escuros, então pode subestimar ruído em áreas escuras;
- textura detalhada e borda de geometria também aumentam o número;
- só é comparável entre capturas do mesmo ângulo, câmera e build;
- não substitui comparação visual nem referência do N64.

Os valores 13,28 / 9,09 / 9,06 foram obtidos com o mesmo script, o mesmo recorte e o mesmo ângulo. O md5 do painel foi usado como prova de que a diferença vinha da cadeia e não de outra vista.

## O que ainda não foi medido

- varredura de `ROOM_ANGLE` com o instrumento ligado — o custo e o grão só foram medidos em um ângulo;
- segunda sala e sala com atores;
- tempo de rasterizador e custo de preenchimento de pixel;
- latência de SD e vazão real de leitura;
- qualquer coisa em DSi físico;
- taxa de quadros do jogo completo, com câmera, colisão, animação e áudio rodando juntos.

Enquanto esses itens não existirem, os números desta página descrevem **uma sala, um ângulo e um build**, não o jogo.
