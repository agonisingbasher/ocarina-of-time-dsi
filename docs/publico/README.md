# Camada pública — pronta, não publicada

Esta pasta contém a documentação preparada para uma integração posterior em site. **Nada aqui foi publicado ou alterado em ambiente público.** O site é uma etapa separada.

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
3. **Evidência técnica** — arquivos, funções, builds, capturas, hashes e procedimento de reprodução. Fica nos históricos e em [`evidencias/`](../../evidencias/).

Os três níveis são gerados ou conferidos a partir de `dados/progresso.json`; a página de desempenho e os textos editoriais são escritos à mão, mas os valores vêm das mesmas medições registradas.

## Regras de publicação

- **Nada de ROM ou asset extraído.** Nenhum arquivo de ROM, tabela extraída ou asset do jogo entra nesta pasta. As capturas são evidência de execução, não assets redistribuíveis.
- **Nada de percentual global.** Não existe metodologia verificável para um número único de conclusão; o estado é declarado por entrega.
- **Emulador não é hardware.** Todo resultado desta camada é de melonDS. Nenhuma entrega foi validada em DSi físico.
- **Ganho e custo juntos.** Toda melhoria visual ou de qualidade vem com o custo medido, e vice-versa.
- **Condição declarada.** Nenhum número aparece sem cena, câmera, build e método.
- **Correção não é apagamento.** Quando uma conclusão muda, a anterior fica registrada e a nova aponta para a evidência.
- **Arquivos gerados não são editados à mão.** `linha-do-tempo.md`, `estado-e-limites.md`, `galeria.md` e `resumos/` são derivados de `dados/progresso.json`.

## Como regenerar

Na raiz do projeto:

```
python ferramentas/gerar_publico.py
python ferramentas/validar_progresso.py
```

O gerador reescreve os resumos, a linha do tempo, o estado e a galeria. O validador confere o esquema, a existência das pastas de evidência, o `MANIFESTO.csv`, a consistência com as seções dos históricos e a ausência de caminhos locais ou referências de ROM nos documentos públicos.

Quando novas capturas forem adicionadas a `evidencias/`, rode antes:

```
python ferramentas/regerar_manifesto.py
```

Ele recalcula tamanho e SHA-256 de cada arquivo e reconcilia o manifesto com o disco.

## Estrutura

```
dados/
  progresso.json           dados estruturados, esquema 1.0.0
  progresso.schema.json    esquema versionado
ferramentas/
  validar_progresso.py     validação do JSON e das consistências
  gerar_publico.py         geração desta pasta
  regerar_manifesto.py     reconciliação de evidencias/MANIFESTO.csv
  speck2.py                medidor de grão do painel
  crop_rect.ps1            recorte e ampliação de captura
docs/publico/              esta camada
evidencias/                capturas permanentes + manifesto
```

## Esquema dos dados

`dados/progresso.json` está na versão `1.0.0`. Cada entrega registra: identificador, tipo, data, frente, fase, estado, build, resumo público, estudo de engenharia, medições, testes, problemas, retratações, dependências, próximos passos, evidências e referências aos históricos. O vocabulário de estado separa explicitamente implementação de validação:

`planejado` → `em_implementacao` → `implementado` → `testado_computador` → `testado_emulador` → `testado_hardware`, além de `parcial_bloqueado_regressao`.

Uma correção (`OTD-025`) referencia a entrega que corrige e não conta como avanço independente.
