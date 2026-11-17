# Camada pública

Esta pasta é a documentação pública em markdown do port. Ela **está publicada**: é acompanhada pelo Git, fica sob a raiz do site (`docs/`) e a página `docs/index.html` aponta para ela no fim da linha do tempo.

A fonte é sempre a mesma, e o caminho é mecânico:

```
dados/progresso.json                os 72 registros, espelho da árvore de trabalho
  └─ ferramentas/gerar_recorte_publico.py   recorte público + sanitização + validação de esquema
       └─ docs/data/oot-dsi-progress.json   o que a página HTML lê
            └─ ferramentas/gerar_publico.py  esta pasta (resumos/, linha-do-tempo, estado, galeria)
```

Quem altera os arquivos gerados são os dois geradores, não a mão. Os três textos editoriais — [visao-geral.md](visao-geral.md), [desempenho.md](desempenho.md) e [creditos.md](creditos.md) — são escritos aqui, mas todo número que aparece neles vem das mesmas medições registradas em `dados/progresso.json`.

## Índice

| Página | Nível | Conteúdo |
|---|---|---|
| [visao-geral.md](visao-geral.md) | resumo público | o que é o projeto, as duas frentes, o que está demonstrado e o que não está |
| [estado-e-limites.md](estado-e-limites.md) | resumo + estado | estado por entrega, pendências e o que ainda não existe |
| [desempenho.md](desempenho.md) | estudo + evidência | definição de cada indicador, condição de medida, A/B com ganho e custo, limites da métrica de grão |
| [linha-do-tempo.md](linha-do-tempo.md) | resumo | entregas na ordem de registro |
| [galeria.md](galeria.md) | evidência | capturas reais do emulador |
| [resumos/](resumos/) | resumo + estudo | um arquivo por `OTD-NNN` |
| [creditos.md](creditos.md) | referência | origem do código, papel da decompilação, dependências e licenças |

## Os três níveis

1. **Resumo público** — o que mudou e por que importa, em linguagem simples. É o texto de `resumos/` e das páginas de visão geral e linha do tempo.
2. **Estudo de engenharia** — problema, investigação, decisão, resultado e limitação. Fica nos resumos e, com detalhe maior, nos históricos técnicos.
3. **Evidência técnica** — arquivos, funções, builds, capturas, hashes e procedimento de reprodução. Fica nos históricos da árvore de trabalho e, para as capturas publicadas, em [`evidencias/publicas/`](../../evidencias/publicas/).

Os três níveis saem dos mesmos registros; só os textos editoriais são escritos à mão, e mesmo assim com valores transcritos das medições.

## Regras de publicação

- **Nada de ROM ou asset extraído.** Nenhum arquivo de ROM, tabela extraída ou asset do jogo entra nesta pasta. As capturas são evidência de execução, não assets redistribuíveis.
- **Nada de percentual global.** Não existe metodologia verificável para um número único de conclusão; o estado é declarado por entrega.
- **Emulador não é hardware.** 59 entregas terminaram em melonDS 1.1 (modo DS, 67 MHz) e 13 só em validação de computador; nenhuma passou de um DSi físico, e o vocabulário de estado não permite escrever essa afirmação.
- **Captura publicada é escolha, não subproduto.** Das centenas de evidências da árvore de trabalho, 6 estão no site, listadas uma a uma em `evidencias_publicadas` no recorte. Uma entrega nova nunca publica imagem sozinha.
- **Ganho e custo juntos.** Toda melhoria visual ou de qualidade vem com o custo medido, e vice-versa.
- **Condição declarada.** Nenhum número aparece sem cena, câmera, build e método.
- **Correção não é apagamento.** Quando uma conclusão muda, a anterior fica registrada e a nova aponta para a evidência — ver a retratação do mip em [desempenho.md](desempenho.md).
- **Arquivos gerados não são editados à mão.** `linha-do-tempo.md`, `estado-e-limites.md`, `galeria.md` e `resumos/` são derivados de `docs/data/oot-dsi-progress.json`, que é derivado de `dados/progresso.json`.

## Como regenerar

Na raiz do projeto, nesta ordem:

```
python ferramentas/gerar_recorte_publico.py   # dados -> docs/data/oot-dsi-progress.json
python ferramentas/gerar_publico.py           # recorte -> docs/publico/
```

O primeiro é o portão de publicação: ele corta os campos que só a árvore de trabalho tem, troca o vocabulário que o projeto não publica, valida o recorte contra `docs/data/oot-dsi-progress.schema.json` e **recusa escrever** se sobrar no texto qualquer referência à cópia de referência, extensão de imagem de console, caminho de usuário ou endereço de e-mail. Ele é determinístico: rodar duas vezes seguidas devolve o mesmo md5 do JSON.

O segundo só lê o recorte, nunca o `dados/progresso.json` direto — é assim que a camada publicada não pode divergir do que foi sanitizado. Ele reescreve `resumos/`, `linha-do-tempo.md`, `estado-e-limites.md` e `galeria.md`.

O gerador de resumos também é a razão pela qual estes arquivos não se editam à mão:

```
docs/publico/resumos/*.md      72 arquivos
docs/publico/linha-do-tempo.md
docs/publico/estado-e-limites.md
docs/publico/galeria.md
```

## O que `validar_progresso.py` verifica aqui

`ferramentas/validar_progresso.py` é a ferramenta da **árvore de trabalho**, não um portão deste repositório. Ele confere o esquema de `dados/progresso.json`, a existência de cada arquivo de evidência citado, o `evidencias/MANIFESTO.csv`, a correspondência com as seções dos históricos técnicos e a limpeza dos documentos públicos.

Neste repositório ele falha por construção, e o total esperado é **313 erros**: 242 de evidência (`evidencias/` aqui só tem as 6 capturas publicadas em `publicas/`, não as centenas de trabalho), 70 de seção de histórico (os `HISTORICO_*.md` desta árvore param na fase 12) e a ausência de `evidencias/MANIFESTO.csv` — que é da árvore de trabalho; o das capturas publicadas existe em `evidencias/publicas/`. As duas verificações que importam para o site — links internos de `docs/publico` e conteúdo não publicável — não produzem nenhuma linha. Se o total mudar de categoria, ou aparecer `link quebrado` ou uma linha de conteúdo proibido, aí sim algo quebrou aqui.

Quando novas capturas forem adicionadas a `evidencias/`, rode antes:

```
python ferramentas/regerar_manifesto.py
```

Ele recalcula tamanho e SHA-256 de cada arquivo e reconcilia o manifesto com o disco.

## Estrutura

```
dados/
  progresso.json           os 72 registros, esquema 1.0.0
  progresso.schema.json    esquema da árvore de trabalho
docs/
  data/oot-dsi-progress.json        recorte público, o que a página lê
  data/oot-dsi-progress.schema.json esquema do recorte
  publico/                                      esta camada
  assets/otd-*.png                              as 6 capturas publicadas
ferramentas/
  gerar_recorte_publico.py  dados -> recorte público, com validação e filtro
  gerar_publico.py          recorte -> esta pasta
  validar_progresso.py      validação da árvore de trabalho (ver acima)
  regerar_manifesto.py      reconciliação de evidencias/MANIFESTO.csv
  speck2.py                 medidor de grão do painel
  crop_rect.ps1             recorte e ampliação de captura
evidencias/
  publicas/                 as capturas que saem no site + MANIFESTO.csv
```

## Esquema dos dados

`dados/progresso.json` está na versão `1.0.0`. Cada entrega registra: identificador, tipo, data, frente, fase, estado, build, resumo público, estudo de engenharia, medições, testes, problemas, retratações, dependências, próximos passos, evidências e referências aos históricos. O vocabulário de estado separa explicitamente implementação de validação:

`planejado` → `em_implementacao` → `implementado` → `testado_computador` → `testado_emulador` → `testado_hardware`, além de `parcial_bloqueado_regressao`.

Nenhuma entrega deste registro passou de `testado_emulador`: 59 terminaram em emulador e 13 em validação de computador. Uma correção (`OTD-025`) referencia a entrega que corrige e não conta como avanço independente.
