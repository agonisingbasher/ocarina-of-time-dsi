# Site público do projeto

Esta pasta é a raiz do site publicado em <https://agonisingbasher.github.io/ocarina-of-time-dsi/>.
O GitHub Pages só aceita a raiz do repositório ou `docs/` como pasta publicada, por isso o site
mora aqui. Enquanto a pasta publicada for a raiz, o `index.html` do repositório redireciona para
cá; mover a publicação para `/docs` em Settings → Pages dispensa o redirecionamento.

| Arquivo | O que é |
| --- | --- |
| `index.html` | A página do port (anteriormente `oot-dsi.html` no portfólio). |
| `oot.css` | Estilos próprios da página, extraídos do `style.css` do portfólio. |
| `oot.js` | Motor de idioma e os quatro dicionários (PT, EN, ES, FR) da página principal: 404 chaves em cada idioma. |
| `idioma-pagina.js` | Motor de idioma das páginas secundárias. Lê `window.pageTranslations`, definido dentro de cada página. |
| `site-development.html` | Página de autoria do site, em quatro idiomas. |
| `support.html` | Página de apoio (chave Pix), em quatro idiomas. |
| `assets/` | Capturas reais e artes conceituais de IA usadas pela página. |
| `data/` | Recorte público dos dados de progresso, com o esquema próprio. |
| `publico/` | Documentos em Markdown (linha do tempo, desempenho, créditos, resumos OTD-NNN). |

## Como os dados chegam até aqui

A camada de dados é gerada, não copiada à mão:

```
dados/progresso.json
  → ferramentas/gerar_recorte_publico.py  → docs/data/oot-dsi-progress.json
  → ferramentas/gerar_publico.py          → docs/publico/
```

`gerar_recorte_publico.py` faz o corte (campo `resumo_publico` vira `resumo`, `estado` vira
`validacao`, `limitacao` vira `limitacoes`, pastas de evidência viram objetos com
`publicada_no_site`), valida o resultado contra `data/oot-dsi-progress.schema.json`, recusa
escrever se sobrar conteúdo não publicável e é determinístico — duas execuções seguidas dão o
mesmo hash. `gerar_publico.py` lê só o recorte, nunca o JSON interno, para que a camada publicada
não possa divergir do que foi sanitizado.

## Espelhos que precisam continuar sincronizados

- `evidencias/publicas/*.png` → `assets/*.png` (as seis capturas exibidas na galeria).
- `dados/progresso.json` é o espelho do arquivo de trabalho; `data/oot-dsi-progress.json` é o
  recorte público derivado dele, com `ambiente_padrao`, `evidencias_publicadas` e `nota_publicacao`.
- `data/oot-dsi-progress.schema.json` **não** é cópia de `dados/progresso.schema.json`: o esquema
  público tem campos próprios (`validacao`, `limitacoes`, `evidencia_publicada`) e não conhece os
  campos internos (`resumo_publico`, `evidencias_ausentes`). Os dois precisam aceitar o mesmo
  conjunto de indicadores de medição — quando nasce um indicador novo, entra nos dois.
- `assets/oot-conceito-*.webp` são artes conceituais geradas por IA; não há original no restante do
  repositório. Elas nunca representam captura do port, e a legenda na página diz isso.

## Idioma

Quatro idiomas com dicionário próprio — pt-BR, EN, ES e FR. Na página principal o dicionário está
em `oot.js`; em `site-development.html` e `support.html` cada página define
`window.pageTranslations` e carrega `idioma-pagina.js`. O seletor são os quatro botões
`PT / EN / ES / FR`. Não existe link para o Google Translate em nenhuma página, e nada do que
aparece em espanhol ou francês passa por ele: tudo vem dos dicionários escritos aqui.

A ordem de decisão é `?lang=` > valor salvo em `localStorage` com a chave
`agonisingbasher-language` — a mesma usada pelo portfólio, para que o idioma escolhido em um site
continue válido no outro > `navigator.languages` > pt-BR. Sem `?lang=` e sem escolha gravada, o
navegador decide: quem abre com o sistema em espanhol ou francês já cai no idioma dele. Qualquer
`pt-*` resolve em `pt-BR`, única variante portuguesa traduzida.

Paridade de chaves é verificada, não presumida. `oot.js` aplica `content[chave]` sem resgate: uma
chave ausente imprime `undefined` na tela, e não cai no português do HTML. `idioma-pagina.js` tem
guarda e simplesmente não toca no elemento. Antes de mexer nos dicionários, confira que os quatro
têm o mesmo conjunto de chaves e que toda `data-i18n` da página existe nos quatro.

## O que não vai para o site

Nada de cópia de referência do jogo, ROM, assets extraídos, caminhos locais ou aplicadores de
build. O site publica apenas apresentação, medições, limites e as evidências listadas em
`evidencias/publicas/MANIFESTO.csv`. O filtro que garante isso está em
`ferramentas/gerar_recorte_publico.py`, e ele recusa escrever quando encontra o vocabulário
proibido — inclusive no recorte já revisado, que passa pelas mesmas trocas a cada regeneração.
