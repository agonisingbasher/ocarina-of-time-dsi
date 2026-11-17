# Desempenho: o que cada número significa

Toda comparação deste projeto declara o ambiente, a câmera e o par de builds de que saiu. Sem isso, dois números lado a lado não comparam nada — foi exatamente o erro que a fase 7 do histórico do jogo encontrou, e é o que faz a tabela de outubro e a de novembro abaixo terem números absolutos que não se somam.

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
| Câmera | duas condições convivem nesta página: **presa** por `ROOM_SPIN 0` / `ROOM_ANGLE 0` em `source/main.c` (todas as tabelas de outubro) e **de caminhada**, com o ator `En_Door` (id 9) em (119,359,60) e `Bmori1_room_1` no cartão (a medição vigente da cadeia e o perfil da travessia) |
| Filtro de textura | `GE_TEX_MIP_MAX` conforme a linha; `GE_TEX_LOD_BIAS 2048` |
| Instrumentação | linha 8 da consola (`f`, `s`, `w`); contadores nas linhas 18..22 |
| Regime | após o carregamento da sala e a estabilização do loop; não são os primeiros quadros |
| Coleta | captura da janela, recorte com `ferramentas/crop_rect.ps1` (vizinho mais próximo, 2×) e leitura da linha 8 |
| Repetibilidade | duas capturas do mesmo build e ângulo dão o mesmo md5 do painel; cada linha da tabela A/B foi medida em dois builds e dois quadros; o A/B vigente usou seis execuções e dezesseis capturas, com as três execuções do braço ligado dando a mesma mediana |

Não existe versão numerada do port: a árvore não tem histórico git. O identificador do build é o `OTD-NNN` da entrega, e a condição de cada linha está declarada nas tabelas e nos resumos.

## A/B da cadeia de mip

### Medição vigente (2026-11-02, `OTD-044`, no binário que hoje se mede)

A cadeia estava ligada desde outubro por um número medido em **outro binário**. `OTD-044` refez o comparativo com os dois braços saindo da mesma árvore e diferindo por uma única macro — e para isso a macro teve de se tornar realmente sobrescrevível: o `#define` não tinha guarda, então um `-D` de linha de comando era redefinido pelo código e **o A/B mediria o mesmo binário duas vezes**. Com a guarda `#ifndef`, o braço desligado fica 2 048 B menor e carrega um aviso que o outro não tem, que é a prova visível de a macro chegou ao código gerado; o braço ligado reproduz byte a byte o build já registrado. Seis execuções mornas, dezesseis capturas, três execuções do braço ligado dando a mesma mediana.

| Janela | `s` sem cadeia | `s` com cadeia | diferença | como fração do render enviado | grão sem → com |
|---|---|---|---|---|---|
| sala 0 (40–56 s) | 98,8 ms | **119,5 ms** | **+20,6 ms** (+20,9 %) | **17,3 %** | 7,70 → **7,27** (−5,6 %) |
| sala 1 (57–60 s) | 87,1 ms | 106,3 ms | +19,2 ms (+22,1 %) | 18,1 % | 5,46 → 5,49 (não se move) |
| volta (61–70 s) | 101,3 ms | 124,7 ms | +23,4 ms (+23,1 %) | 18,8 % | 2,80 → 2,81 (não se move) |

O quadro inteiro (`f`) absorve menos que o render: 128,7 ms com cadeia contra 112,2 sem, ou **+16,5 ms por quadro**, e o FPS impresso cai de 9 para 8. A cadeia alcança **15 ou 16 das 646 faces** texturizadas deste ângulo, com nível mais fundo 2 e zero níveis recusados pela VRAM. O ganho de grão existe só na sala 0; nas outras duas janelas o painel não se move — e isso aparece na própria imagem: em 7 dos 16 segundos capturados algum recorte do braço desligado é byte a byte igual ao do braço ligado no mesmo segundo, e os sete são justamente a volta e o primeiro segundo da sala 1. Na ida da sala 0 nenhum painel coincide.

A leitura da porta não passa pelo caminho do mip: 16 948 µs contra 16 929 µs nos dois braços, 19 µs de diferença (0,1 %).

**Retratação.** As linhas abaixo ficam registradas como foram medidas, e o número delas não vale mais: **+16,4 ms, 34 % do render e −31 % de grão** foram obtidos num binário cujo render base era 48,8 ms. A diferença absoluta é maior hoje (+20,6 ms) e a fração é menor (17,3 %) porque o render base cresceu. A razão do A/B também não se repetiu: era −31 %/−32 % de grão, hoje é −5,6 %.

### Medição retratada (2026-10-27, `OTD-030`, ângulo preso, render base 48,8 ms)

Ângulo preso, quatro builds na mesma linhagem, duas capturas por build; os dois quadros de cada build dão o mesmo md5. A coluna de custo/ganho é o número que a retratação acima substitui:

| Configuração | Grão do painel | FPS | `f` (ms) | `s` (ms) | `r` | Custo / ganho |
|---|---|---|---|---|---|---|
| cadeia desligada (`MAX 0`) | **4,22** | 20 | 33,4 | 32,4 | 136 | referência |
| **teto 2 (entregue)** | **2,91** | 19 | 50,1 | 48,8 | 247 | −31 % de grão por +16,4 ms (34 % do render) — **retratado** |
| teto 2 + seletor calculado e descartado | 4,22 | 19 | 50,1 | 48,1 | 136 | paga a aritmética, não usa o nível |
| teto 2 + uma troca forçada por triângulo | 2,91 | 19 | 50,1 | 49,1 | 909 | +0,3 ms por 662 trocas, **0 px de diferença** |

O grão absoluto desta tabela **não é comparável** com o da tabela histórica abaixo: o painel deixou de ser pintado de branco quando o build passou a usar a cor de vértice. O que se comparava entre as duas era a razão do A/B (−31 % e −32 %), e é exatamente essa razão que `OTD-044` derruba: refeita no binário atual, ela é −5,6 %.

### Decomposição do custo da cadeia

Medida em 2026-10-27 (`OTD-030`), no render base de 48,8 ms. A leitura qualitative — quem cobra é a conta por face, não a troca — é a que continua valendo; os milissegundos são daquele binário.

| Parcela | Como foi isolada | Valor |
|---|---|---|
| aritmética do seletor | build que calcula o nível e descarta | **15,7 ms** (96 %) |
| escolha do nível, busca no cache e bind | entregue menos o build acima | **0,7 ms** |
| troca de textura | uma ligação forçada por triângulo, mesmo id e mesmos pixels | **0,45 µs por troca**; 662 trocas = 0,3 ms |
| upload de textura | `Tex b0` nos quatro builds em repouso | **0 por quadro** (o cache acerta tudo) |

A conclusão da fase 8 — "a otimização que paga é agrupar primitivos por nível e cortar os re-binds" — **foi refutada**: com 0,45 µs por troca, as 111 trocas de nível por quadro valem no máximo 0,7 ms, 4 % do custo da cadeia. O custo está na conta por face, como a fase 9 já dizia.

### Compromisso em avaliação

O número deixou de ser provisório: `OTD-044` refez o A/B no binário atual, com os dois braços saindo da mesma árvore. A cadeia continua ligada, e o preço vigente é **+20,6 ms de render na sala 0, 17,3 % do que o build envia, por 0,40 unidades de grão (−5,6 %) em 15 ou 16 faces das 646** — e ganho nenhum nas outras duas janelas medidas.

Se ligar ou desligar vale a pena continua sendo decisão aberta, não conclusão: o build sem cadeia (`MAX 0`) fica declarado como alternativa, e a leitura visual acompanha o número (com a cadeia, os tijolos da parede leem-se como tijolos; sem ela, a parede é aresta de ruído). O ganho e o custo aparecem juntos porque um não existe sem o outro.

E a decomposição tem uma consequência medida: se quem cobra é a conta que escolhe o nível, um build sem cadeia não tem por que pagá-la. `OTD-071` tirou o seletor de nível — e os contadores que só ele lia — do build que não tem cadeia, e a vista inicial de Bmori1 ficou **0,9 ms mais leve (38,0 → 37,1 ms, −2,4 %)** sem mudar o quadro fora da silhueta do Link. Isso não barateia a cadeia; barateia o braço desligado, que é a alternativa declarada acima.

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

## Perfil da travessia (novembro, `OTD-065` a `OTD-071`)

A câmera aqui é a de caminhada, e o que se mede é a ida e a volta pela porta de Bmori1 com as duas idades do Link. Cada linha é um build diferente, então a coluna de número **não é uma série acumulativa**: são os valores que cada entrega registrou na sua própria condição.

| Entrega | O que mudou | Número registrado |
|---|---|---|
| `OTD-065` | Link jovem e adulto atravessam a porta de Bmori1 de ponta a ponta | ida e volta concluídas nas duas idades, com cartão e controles prontos; sem número de quadro registrado |
| `OTD-066` | perfil opcional: rotinas em ITCM, modelo de distância original, sem mipmaps, câmera compartilhada | quadro **154,8 → 50,1 ms** na vista comparada; **19–20 FPS** na sala inicial; porta com as duas salas em média **65 ms** |
| `OTD-067` | menos custo no quadro em que as duas salas estão na tela | média da travessia **65 → 55 ms**; picos de 96–97 ms |
| `OTD-068` | pré-carga da sala vizinha | pico do adulto **96 → 81 ms** (jovem: 99 ms) |
| `OTD-069` | matrizes afins no esqueleto do Link | média **53 ms** por quadro; picos de 81 e 82 ms |
| `OTD-070` | índice que valida cada candidato no cache de textura | travessias concluídas; a entrega não registra um número de quadro separado |
| `OTD-071` | o seletor de nível sai do build sem cadeia | render **38,0 → 37,1 ms** (−2,4 %) na vista inicial, 1.100 triângulos por quadro |

O que a série diz: a travessia deixou de ser o evento que quebra o loop — 65 ms de média com as duas salas na tela, depois 53 ms — mas o pico continua em ~81 ms, ou seja **três períodos de VBlank**, e a média é uma janela, não o pior quadro da rotação. O que ela não diz: nenhum valor acima é taxa sustentada do jogo, e `OTD-066` mediu uma comparação entre dois builds na mesma vista, não uma linha de tempo de projeto.

## Custo de um ator (`OTD-072`)

| Medida | Valor | Condição |
|---|---|---|
| triângulos de Saria | 459 | `kokiri_home5`, sala 0, visualizador com o fundo composto |
| render só do ator submetido (`s` da linha do ator) | **39,02 ms** | mesma câmera de levantamento, três capturas |
| quadro com fundo composto + ator (`f`) | **50,1 ms** | três capturas concordantes |
| quadro com o mesmo fundo, sem a geometria do ator | 16,7 ms | referência do fundo |

Os dois números de quadro não se somam: `f` termina em espera de VBlank e fica quantizado em múltiplos de ~16,7 ms, então 16,7 → 50,1 é o loop passando de um período para três. O que diz quanto o ator custa é o `s` dele — **39,02 ms de CPU de render**, que sozinho já estoura o orçamento de 33 ms da meta de 30 FPS. Medido em uma sala, um ator e uma câmera de visualizador, **sem caminhada nesse build**; a pendência está registrada como `OTD-072-P1`.

## A métrica de grão

**Definição.** Média do módulo da diferença de luminância entre pixels vizinhos horizontais no painel 3D. É um indicador de ruído de alta frequência, não de cor.

**Script.** [`ferramentas/speck2.py`](../../ferramentas/speck2.py). Entrada: o recorte `_topbig`. O script remove a duplicação da ampliação 2×, converte cada pixel para luminância `0,30R + 0,59G + 0,11B`, ignora pares em que ambos os pixels têm luminância abaixo de 8 e devolve a média de `|lum(x) − lum(x+1)|`.

**Limitações.**

- só luminância, sem calibração publicada contra referência externa;
- ignora pares escuros, então pode subestimar ruído em áreas escuras;
- textura detalhada e borda de geometria também aumentam o número;
- só é comparável entre capturas do mesmo ângulo, câmera e build;
- não substitui comparação visual nem referência do N64.

Os valores 13,28 / 9,09 / 9,06 (fase 7), 4,22 / 2,91 (`OTD-030`) e 7,70 / 7,27 (`OTD-044`) foram obtidos com o mesmo script, o mesmo recorte e a mesma definição. **Isso não os torna comparáveis entre si**: cada par foi medido num painel diferente (branco pintado, cor de vértice, câmera de caminhada com ator) e a grandeza absoluta acompanha o conteúdo da tela. O que se compara é a razão entre dois builds na mesma janela — e foi justamente essa razão que mudou de −31 % para −5,6 % quando o A/B foi refeito no binário atual. O md5 do painel foi usado como prova de que a diferença vinha da cadeia e não de outra vista.

## O que ainda não foi medido

- tempo de rasterizador e custo de preenchimento de pixel — nenhum `s`, `f` ou tempo de submissão desta página inclui o rasterizador;
- **DSi físico**: a máquina de medição não tem BIOS/NAND de DSi, então 134 MHz não foi medido e qualquer ganho de clock é inferência (declarado em `OTD-071`);
- 20 FPS sustentados durante a rotação e durante a passagem da porta — as tabelas acima dão médias de janela e picos pontuais, não a taxa do pior quadro do gesto;
- varredura de `ROOM_ANGLE` com o instrumento ligado — o custo e o grão só foram medidos nos ângulos declarados linha a linha;
- atores em movimento durante a caminhada — o custo de `OTD-072` é do visualizador com câmera presa, e o de `OTD-044` é de uma sala com uma porta;
- latência de SD e vazão real de leitura — os 16,9 ms de leitura da sala oposta são o cartão emulado pelo DLDI do melonDS, ou seja tempo de emulador;
- taxa de quadros do jogo completo, com câmera, colisão, animação, áudio e HUD juntos, e cobertura além das salas desenhadas — as 388 salas preparadas por `OTD-063` não foram desenhadas nem medidas.

Enquanto esses itens não existirem, os números desta página descrevem **as salas, câmeras e builds declarados em cada tabela**, não o jogo. A meta de 30 FPS (45 onde a cadência de 60 Hz permitir, com a lógica a 20 Hz) é critério de aceite do plano: `OTD-032` chegou a ler FPS 30 e render 27,3 ms na sala 1 de Bmori1, mas isso é uma sala numa câmera de sonda — nenhum build mediu a meta no jogo, e nenhum mediu no hardware.
