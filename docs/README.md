# Site público do projeto

Esta pasta é a raiz do site publicado em <https://agonisingbasher.github.io/ocarina-of-time-dsi/>.
O GitHub Pages só aceita a raiz do repositório ou `docs/` como pasta publicada, por isso o site
mora aqui. Enquanto a pasta publicada for a raiz, o `index.html` do repositório redireciona para
cá; mover a publicação para `/docs` em Settings → Pages dispensa o redirecionamento.

| Arquivo | O que é |
| --- | --- |
| `index.html` | A página do port (anteriormente `oot-dsi.html` no portfólio). |
| `oot.css` | Estilos próprios da página, extraídos do `style.css` do portfólio. |
| `oot.js` | Traduções PT/EN e alternância de idioma, extraídas do `script.js` do portfólio. |
| `assets/` | Capturas reais e artes conceituais de IA usadas pela página. |
| `data/` | Espejo público dos dados de progresso. |
| `publico/` | Documentos em Markdown (linha do tempo, desempenho, créditos, resumos OTD-NNN). |

## Espelhos que precisam continuar sincronizados

O site é publicado e o repositório é a fonte. Cada arquivo abaixo tem um original no repositório,
e atualizar só um dos dois deixa o site desalinhado do projeto:

- `evidencias/publicas/*.png` → `assets/*.png` (as seis capturas exibidas na galeria).
- `dados/progresso.json` → `data/oot-dsi-progress.json`. Este último é o recorte público: tem
  `ambiente_padrao`, `evidencias_publicadas` e `nota_publicacao`, e não reproduz o documento interno
  na íntegra.
- `data/oot-dsi-progress.schema.json` é cópia de `dados/progresso.schema.json`.
- `assets/oot-conceito-*.webp` são artes conceituais geradas por IA; não há original no restante do
  repositório. Elas nunca representam captura do port, e a legenda na página diz isso.

## Idioma

A página lê `?lang=pt-BR` ou `?lang=en`, depois o valor salvo em `localStorage` com a chave
`agonisingbasher-language` — a mesma usada pelo portfólio, para que o idioma escolhido em um site
continue válido no outro.

## O que não vai para o site

Nada de `baserom`, ROM, assets extraídos do jogo, caminhos locais ou aplicadores de build. O site
publica apenas apresentação, medições, limites e as evidências listadas em
`evidencias/publicas/MANIFESTO.csv`.
