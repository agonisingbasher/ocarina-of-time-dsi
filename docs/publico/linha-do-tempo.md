# Linha do tempo

> Gerado de `docs/data/oot-dsi-progress.json`. Não editar à mão.

O `OTD-NNN` é atribuído na ordem de registro do histórico, não pela ordem de execução. As 72 entregas vão de 2026-10-26 a 2026-11-17; a data identifica o dia do registro, e a coluna *Fase* identifica a área de trabalho. Uma correção (`OTD-025`) aparece sob a entrega que corrige e não conta como avanço independente.

| Data | ID | Frente | Fase | Entrega | Estado |
|---|---|---|---|---|---|
| 2026-10-26 | [OTD-001](resumos/OTD-001.md) | jogo | 0 | Sonda de hardware: o DSi emulado entrega DSi mode, RAM e duas telas | Testado em emulador |
| 2026-10-26 | [OTD-002](resumos/OTD-002.md) | jogo | 1 | Base do loop: tempo, escalonador de 20 Hz, memória e telemetria | Testado em emulador |
| 2026-10-26 | [OTD-003](resumos/OTD-003.md) | motor gráfico | 2 | Decodificador F3DEX2: ler display lists do decomp no computador | Testado no computador |
| 2026-10-26 | [OTD-004](resumos/OTD-004.md) | motor gráfico | 2 | Primeira display list F3DEX2 desenhada pelo geometry engine | Testado em emulador |
| 2026-10-27 | [OTD-005](resumos/OTD-005.md) | motor gráfico | 2 | Matriz da display list aplicada no geometry engine | Testado em emulador |
| 2026-10-27 | [OTD-006](resumos/OTD-006.md) | motor gráfico | 2 | Projeção vinda da display list e pilha de matrizes por tipo | Testado em emulador |
| 2026-10-27 | [OTD-007](resumos/OTD-007.md) | motor gráfico | 2 | Texturas CI4 e CI8 com paleta por slot de TMEM | Testado em emulador |
| 2026-10-27 | [OTD-008](resumos/OTD-008.md) | motor gráfico | 2 | Cache de textura com substituição LRU | Testado em emulador |
| 2026-10-27 | [OTD-009](resumos/OTD-009.md) | motor gráfico | 2 | Combiner do ciclo 0 e viewport do N64 | Testado em emulador |
| 2026-10-27 | [OTD-010](resumos/OTD-010.md) | motor gráfico | 2 | Alpha do texel como recorte | Testado em emulador |
| 2026-10-27 | [OTD-011](resumos/OTD-011.md) | motor gráfico | 2 | OtherMode e blending translúcido por polígono | Testado em emulador |
| 2026-10-27 | [OTD-012](resumos/OTD-012.md) | motor gráfico | 2 | Recorte por alpha de paleta CI e disciplina de VRAM | Testado em emulador |
| 2026-10-27 | [OTD-013](resumos/OTD-013.md) | motor gráfico | 2 | Cache de 64 slots: o despejo a meio do frame não existe | Testado em emulador |
| 2026-10-27 | [OTD-014](resumos/OTD-014.md) | jogo | 3 | Camada Ultra64: o primeiro arquivo do decomp compila para ARM9 | Testado em emulador |
| 2026-10-27 | [OTD-015](resumos/OTD-015.md) | jogo | 3 | Display list montada pelo código do jogo pinta pixels | Testado em emulador |
| 2026-10-27 | [OTD-016](resumos/OTD-016.md) | jogo | 3 | sys_matrix.c gera a modelview do jogo | Testado em emulador |
| 2026-10-27 | [OTD-017](resumos/OTD-017.md) | motor gráfico | 3 | Viewport de dois cantos e FORCE_BL | Testado em emulador |
| 2026-10-27 | [OTD-018](resumos/OTD-018.md) | motor gráfico | 3 | Profundidade pelo menor z/w e coluna de translação | Testado em emulador |
| 2026-10-27 | [OTD-019](resumos/OTD-019.md) | motor gráfico | 2 | Recorte de polígono no plano perto | Testado em emulador |
| 2026-10-27 | [OTD-020](resumos/OTD-020.md) | jogo | 1 | Entrada: teclas do DS e toque viram o contrato BTN_* | Testado em emulador |
| 2026-10-27 | [OTD-021](resumos/OTD-021.md) | jogo | 1 | fs: assets lidos do cartão SD em streaming | Testado em emulador |
| 2026-10-27 | [OTD-022](resumos/OTD-022.md) | motor gráfico + jogo | 6 | A primeira sala real do jogo na tela de cima | Testado em emulador |
| 2026-10-27 | [OTD-023](resumos/OTD-023.md) | motor gráfico + jogo | 6 | Sala texturizada: o confete era minificação, não paleta | Testado em emulador |
| 2026-10-27 | [OTD-024](resumos/OTD-024.md) | motor gráfico + jogo | 7 | Câmera presa como régua e seleção de mip por triângulo | Testado em emulador |
| 2026-10-27 | [OTD-025](resumos/OTD-025.md) | motor gráfico | 7b | *(correção de OTD-024)* Correção de escala da sonda de mip (fator 4096) | Testado em emulador |
| 2026-10-27 | [OTD-026](resumos/OTD-026.md) | motor gráfico | 8 | O custo do mip era aritmética por face; recíproco por tabela devolve 2,1 ms | Testado em emulador |
| 2026-10-27 | [OTD-027](resumos/OTD-027.md) | motor gráfico | 9 | A área em espaço de olho empatou; o ganho estava no tamanho do inteiro | Testado em emulador |
| 2026-10-27 | [OTD-028](resumos/OTD-028.md) | motor gráfico | 10 | A sala estava branca porque o port a pintava de branco | Testado em emulador |
| 2026-10-27 | [OTD-029](resumos/OTD-029.md) | motor gráfico | 11 | O lerp de duas texturas virou segunda passada, e o alpha da segunda imagem quase o engoliu | Testado em emulador |
| 2026-10-27 | [OTD-030](resumos/OTD-030.md) | motor gráfico + jogo | 12 | A troca de textura custa 0,45 µs; quem cobra é a aritmética do mip | Testado em emulador |
| 2026-10-27 | [OTD-031](resumos/OTD-031.md) | jogo | 13 | A fatia caminhável existe: o marcador para em 856, desliza na parede e a câmera não atravessa | Testado em emulador |
| 2026-10-28 | [OTD-032](resumos/OTD-032.md) | jogo | 14 | A porta leva à segunda sala e traz de volta, sem alocar nada no caminho | Testado em emulador |
| 2026-10-28 | [OTD-033](resumos/OTD-033.md) | jogo | 15 | A camera encurralada sobe em vez de entrar no marcador | Testado em emulador |
| 2026-10-28 | [OTD-034](resumos/OTD-034.md) | jogo | 16 | O modo jogavel espera entrada, e a sonda prova que a porta nao repete a travessia | Testado em emulador |
| 2026-10-28 | [OTD-035](resumos/OTD-035.md) | motor gráfico + jogo | 17 | O Link do proprio cartucho entra em cena: esqueleto de 21 juntas, tunica com o ENVCOLOR reposto depois do reset e as duas animacoes contadas pelo arquivo | Testado em emulador |
| 2026-10-28 | [OTD-036](resumos/OTD-036.md) | motor gráfico + jogo | 18 | A porta do jogo abre pelos seis estados do En_Door e atravessa duas vezes: 52 ticks travados por passagem, apoio a 22 unidades do plano da porta e altura preservada | Testado em emulador |
| 2026-10-28 | [OTD-037](resumos/OTD-037.md) | motor gráfico + jogo | 19 | A cor estava no bit errado: RGBA16 e paleta do N64 sao R5G5B5A1, a paleta CI reaponta no LOADTLUT e o frame se divide em 46 ms de sala, 7 ms de cinematica e 45 ms de lista do Link | Testado em emulador |
| 2026-10-28 | [OTD-038](resumos/OTD-038.md) | motor gráfico + jogo | 20 | Levantamento de cenario por linha de comando: quatro cenarios reais medidos, o teto de 8 entradas exposto como e8/19 no hylia_labo e a sala image do kakariko recusada em stage 2 | Testado em emulador |
| 2026-10-29 | [OTD-039](resumos/OTD-039.md) | motor gráfico + jogo | 21 | Salas RoomShapeImage carregadas: um par de listas nas variantes SINGLE e MULTI, 28 salas e 24228 triangulos sem textura no censo, kakariko 702 e shrine 1162 no emulador; backdrop 2D nao desenhado | Testado em emulador |
| 2026-10-31 | [OTD-040](resumos/OTD-040.md) | motor gráfico + jogo | 22 | Primeiro ator nao-jogador desenhado pela lista da sala: object_ta no cartao, 422 faces batendo com o arquivo, o painel identico explicado por oclusao e a cabeca pedindo dois segmentos que o port nao registra | Testado em emulador |
| 2026-10-31 | [OTD-041](resumos/OTD-041.md) | motor gráfico + jogo | 23 | Os dois segmentos de textura da cabeca de Talon sao registrados a partir do proprio object_ta; o painel nao tem piso de ruido nenhum, mas o quadro impresso tambem nao casa a execucao -- e o bit de ordem de bytes do segmento 9, que se julgava sustentando o ator, nao sustenta nada | Testado em emulador |
| 2026-11-02 | [OTD-042](resumos/OTD-042.md) | motor gráfico + jogo | 18 | A porta carrega a proxima sala durante a transicao: a leitura de Bmori1_room_1 dispara no mesmo tick do A, completa em 16,8 ms dentro da abertura, e a travessia encontra a sala residente -- o ciclo de OTD-036 fecha com a sala trocada no tick da passagem | Testado em emulador |
| 2026-11-02 | [OTD-043](resumos/OTD-043.md) | motor gráfico + jogo | 24 | Estado local reproduzivel: a arvore de hoje compila do zero e produz byte a byte os ROMs que estavam em disco, e a sonda reexecutada sobre esse ROM refaz o ciclo da porta (leitura de 16,9 ms no tick do A, sala 1 com 362 triangulos, arena em 441 KB, sem releitura no retorno) | Testado em emulador |
| 2026-11-02 | [OTD-044](resumos/OTD-044.md) | motor gráfico + jogo | 25 | Preco da cadeia de mip medido entre dois binarios da mesma arvore: +20,6 ms de render na sala 0 (17,3 % do enviado) por 15 ou 16 faces acima do nivel 0 em 646 e 0,40 unidades de grao; os materiais da sala 1 revalidados contra o build sem cadeia, a leitura da porta inalterada (19 us) e a execucao divergente estreitada para o assentamento no chao | Testado em emulador |
| 2026-11-02 | [OTD-045](resumos/OTD-045.md) | motor gráfico + jogo | 26 | Causa da divergencia da primeira execucao fechada por experimento: um unico frame de touch sintetico, injetado dentro do proprio ROM no frame 120, desliga o roteiro da sonda e reproduz as duas corridas reprovadas das entregas anteriores; a consola ganha traco de arming nas linhas 12 e 15 e a sonda ganha crivo na linha 11 | Testado em emulador |
| 2026-11-04 | [OTD-046](resumos/OTD-046.md) | motor gráfico + jogo | 27 | Limites de segmentos e salas, câmera 16.16 protegida e cartão verificável por comando | Testado no computador |
| 2026-11-04 | [OTD-047](resumos/OTD-047.md) | motor gráfico + jogo | 28 | Seleção estável das partes cullable integrada à câmera e roteiro de progresso até o jogo completo | Testado no computador |
| 2026-11-04 | [OTD-048](resumos/OTD-048.md) | motor gráfico + jogo | 29 | Limites de texturas e paletas, aliases faciais e correção das listas em fronteiras adjacentes | Testado no computador |
| 2026-11-06 | [OTD-049](resumos/OTD-049.md) | jogo | 30 | Porta de madeira animada entre as duas salas de Bmori1 | Testado no computador |
| 2026-11-06 | [OTD-050](resumos/OTD-050.md) | motor gráfico | 2 | Auditoria de texturas por sala e aceitação de texturas retangulares até 256 texels | Testado no computador |
| 2026-11-06 | [OTD-051](resumos/OTD-051.md) | motor gráfico | 2 | Recuperação de falhas de envio de textura e VRAM no cache | Testado no computador |
| 2026-11-06 | [OTD-052](resumos/OTD-052.md) | motor gráfico | 31 | Preparação dos fundos de sala com câmera fixa em bitmaps RGB555 | Testado no computador |
| 2026-11-06 | [OTD-053](resumos/OTD-053.md) | motor gráfico | 31 | Visualizador dos fundos originais no build de mapas | Testado no computador |
| 2026-11-06 | [OTD-054](resumos/OTD-054.md) | motor gráfico | 31 | Câmera fixa original e composição de atores sobre os fundos | Testado no computador |
| 2026-11-06 | [OTD-055](resumos/OTD-055.md) | jogo | 30 | Porta de madeira exercitada de ponta a ponta no emulador com o cartão DLDI montado | Testado em emulador |
| 2026-11-06 | [OTD-056](resumos/OTD-056.md) | jogo | 30 | Onde fica a colisão: 101 cenas com um cabeçalho e 388 salas sem nenhum | Testado no computador |
| 2026-11-06 | [OTD-057](resumos/OTD-057.md) | jogo | 30 | Câmera da porta: Camera_Special9 e o letterbox no build de caminhada | Testado em emulador |
| 2026-11-10 | [OTD-058](resumos/OTD-058.md) | motor gráfico + jogo | 30 | Iluminação das salas e listas nativas dos personagens | Testado em emulador |
| 2026-11-10 | [OTD-059](resumos/OTD-059.md) | motor gráfico + jogo | 31 | Descarte de faces conforme as listas originais | Testado em emulador |
| 2026-11-10 | [OTD-060](resumos/OTD-060.md) | motor gráfico + jogo | 32 | Descarte de listas pelo frustum original | Testado em emulador |
| 2026-11-10 | [OTD-061](resumos/OTD-061.md) | motor gráfico + jogo | 33 | Plano de cor e profundidade para fundos de salas | Testado em emulador |
| 2026-11-10 | [OTD-062](resumos/OTD-062.md) | motor gráfico + jogo | 33 | Geração da profundidade dos 35 fundos originais | Testado no computador |
| 2026-11-10 | [OTD-063](resumos/OTD-063.md) | motor gráfico + jogo | 33 | Preparação unificada de salas, fundos e profundidade | Testado no computador |
| 2026-11-13 | [OTD-064](resumos/OTD-064.md) | motor gráfico + jogo | 33 | Troca de câmeras originais no visualizador de salas | Testado em emulador |
| 2026-11-13 | [OTD-065](resumos/OTD-065.md) | motor gráfico + jogo | 33 | Caminhada e travessia com Link jovem e adulto | Testado em emulador |
| 2026-11-13 | [OTD-066](resumos/OTD-066.md) | motor gráfico + jogo | 34 | Perfil de desempenho para 20 FPS em Bmori1 | Testado em emulador |
| 2026-11-14 | [OTD-067](resumos/OTD-067.md) | motor gráfico + jogo | 35 | Menos custo ao atravessar a porta do Templo da Floresta | Testado em emulador |
| 2026-11-14 | [OTD-068](resumos/OTD-068.md) | motor gráfico + jogo | 36 | Pré-carga da sala vizinha em Bmori1 | Testado em emulador |
| 2026-11-14 | [OTD-069](resumos/OTD-069.md) | motor gráfico + jogo | 37 | Matrizes do Link e perfil do pior quadro | Testado em emulador |
| 2026-11-14 | [OTD-070](resumos/OTD-070.md) | motor gráfico + jogo | 38 | Busca mais barata de texturas no motor | Testado em emulador |
| 2026-11-14 | [OTD-071](resumos/OTD-071.md) | motor gráfico | 39 | O seletor de MIP sai do build que não tem cadeia | Testado em emulador |
| 2026-11-17 | [OTD-072](resumos/OTD-072.md) | motor gráfico | 40 | A raiz do NPC volta a ter intervalo dono: Saria desenha sobre o fundo de kokiri_home5 | Testado em emulador |
