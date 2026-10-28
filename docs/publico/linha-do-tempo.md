# Linha do tempo

> Gerado de `dados/progresso.json`. Não editar à mão.

O `OTD-NNN` é atribuído na ordem de registro do histórico, não pela ordem de execução. Quase todas as entregas foram registradas em 2026-10-27; a data identifica o dia do registro, e a coluna *Fase* identifica a área de trabalho. Uma correção (`OTD-025`) aparece sob a entrega que corrige e não conta como avanço independente.

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
