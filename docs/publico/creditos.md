# Créditos, referências e dependências

## Origem do código

| Componente | Origem | Papel |
|---|---|---|
| `game/oot` | decompilação **zeldaret/oot**, commit `14759abf9abb4295de142cdc1452998630ea70fb` (main, 2026-10-18) | lógica do jogo, tipos, macros F3DEX2 e tabelas; é código integrado, não código próprio do port |
| `source/gfx` | código próprio do port | decodificador F3DEX2, tradução para o geometry engine, texturas, cache, combiner, viewport, recorte e seleção de nível de detalhe |
| `source/nds`, `source/ui`, `source/main.c` | código próprio do port | plataforma, tempo, escalonador, memória, entrada, leitura de assets, telemetria, interface das duas telas |
| `ferramentas/` | código próprio do port | validação dos dados, geração da documentação, medidor de grão e recorte de captura |
| `game/oot` adaptado | trechos do decomp com camada de compatibilidade própria | código do jogo compilado para ARM9, com as chamadas de plataforma substituídas |

O projeto **integra** a decompilação; não a reescreve. As adaptações próprias estão nas camadas de plataforma e no motor gráfico. Onde uma conclusão técnica muda, o histórico registra a correção em vez de apagar a conclusão anterior.

## Ferramentas e bibliotecas

| Nome | Versão | Papel |
|---|---|---|
| devkitARM | r68-1 (GCC 16.1.0) | compilador e binutils (2.46.0) para ARM9 |
| libnds | 2.0.2 | acesso a vídeo, interrupções, timers e console do DS |
| libfat / DLDI | pacote dkp | leitura do cartão SD no DS e o `[DLDI] FolderSync` no emulador |
| maxmod | pacote dkp | áudio (ainda não integrado ao jogo) |
| Calico | 1.2.0 | runtime de suporte do devkitPro |
| ndstool | 2.3.1 | empacotamento do `.nds` |
| melonDS | 1.1 | emulador usado em **todas** as validações; não é distribuído com o projeto |

## Assets, ROM e imagens

- Este repositório de documentação **não distribui** ROM, arquivos extraídos do jogo nem tabelas de assets. Nenhum arquivo de publicação contém esses dados.
- Para reproduzir a extração, é necessário usar uma cópia de referência obtida legalmente pelo próprio usuário. As ferramentas de build do decompilador não acompanham essa cópia.
- As capturas em `evidencias/` são **fotos do port em execução** no emulador, feitas como evidência técnica. Elas mostram conteúdo do jogo e não devem ser reutilizadas fora deste contexto.
- *The Legend of Zelda* e *Ocarina of Time* são marcas da Nintendo. Este é um projeto de engenharia independente, sem afiliação com a Nintendo.

## Licenças

As licenças do código da decompilação, do devkitPro e do melonDS são as dos respectivos repositórios de origem. O código próprio deste port segue a licença declarada na raiz do projeto, quando houver; a documentação e os dados estruturados foram escritos para este projeto.

## Como citar uma entrega

Cada entrega tem um identificador estável no formato `OTD-NNN`, um resumo público em [`resumos/`](resumos/) e uma seção correspondente nos históricos técnicos. Ao citar um número, cite o identificador, a condição de medição e o fato de o resultado ser de emulador:

> OTD-026, `Bmori1_room_0`, câmera em `ROOM_ANGLE 0`, melonDS 1.1 em modo DS a 67 MHz: tempo de render de 47,5 ms com a tabela de recíprocos, contra 49,6 ms com a divisão exata.

Sem a condição, o número não significa nada.
