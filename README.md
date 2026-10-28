# Ocarina of Time para Nintendo DSi

Projeto experimental de adaptação nativa de *The Legend of Zelda: Ocarina of Time* para o Nintendo DSi, em C/C++ com libnds e integração de componentes da decompilação [zeldaret/oot](https://github.com/zeldaret/oot).

O projeto está em desenvolvimento. **Não é um emulador, não é um jogo completo, não distribui ROM e não possui data de lançamento.** Nenhuma entrega foi validada em DSi físico; as medições registradas usam melonDS 1.1 em modo DS, a 67 MHz.

This is an experimental native adaptation, not an emulator or a finished game. No game files are distributed, and physical-DSi validation remains pending.

## Estrutura

```text
source/              código próprio do port
├── gfx/             decodificação F3DEX2 e tradução para o geometry engine
├── nds/             plataforma DSi: vídeo, entrada, memória, SD, tempo e HUD
└── oot/             integração gradual com tipos e estruturas da decompilação
Makefile             build ARM9 e empacotamento do .nds
tools/               utilitários de análise e geração
ferramentas/         medição, comparação de capturas e validação de dados
dados/               progresso estruturado e esquema JSON versionado
docs/                site público publicado pelo GitHub Pages
docs/publico/        síntese pública por entrega
evidencias/publicas/ seis capturas publicadas na apresentação
HISTORICO_*.md       históricos técnicos completos do motor e do jogo
```

A apresentação pública fica em [`docs/`](docs/), publicada em
<https://agonisingbasher.github.io/ocarina-of-time-dsi/>. O Pages deste repositório publica a raiz,
então o `index.html` e o `.nojekyll` da raiz só servem para redirecionar para `docs/`; se a pasta
publicada mudar para `/docs` em Settings → Pages, os dois podem ser apagados. O portfólio
[agonisingbasher.github.io/personal-website](https://agonisingbasher.github.io/personal-website/index.html#projetos)
mantém apenas o card que aponta para ela. Os espelhos que precisam continuar sincronizados estão
listados em [`docs/README.md`](docs/README.md).

## Dependências

- devkitARM r68-1 (GCC 16.1.0), binutils 2.46.0 e ndstool 2.3.1.
- libnds 2.0.2, libfat e Calico 1.2.0.
- C/C++ para ARM9 sem FPU; aritmética de ponto fixo.
- [zeldaret/oot](https://github.com/zeldaret/oot) no commit `14759abf9abb4295de142cdc1452998630ea70fb`, posicionado em `game/oot`.
- Cópia de referência NTSC-U 1.2 obtida legalmente pelo usuário. Este repositório **não** contém a cópia de referência, assets extraídos nem builds.

A compilação também usa melonDS 1.1 para registrar capturas e telemetria. A validação em hardware físico continua pendente.

## Compilação

```sh
git clone https://github.com/zeldaret/oot.git game/oot
git -C game/oot checkout 14759abf9abb4295de142cdc1452998630ea70fb
make
```

O `Makefile` espera o ambiente do devkitPro e gera `OcarinaDsi.nds` apenas quando os arquivos proprietários necessários já estiverem disponíveis localmente. Artefatos como `build/`, `.elf`, `.nds`, `.map` e `.lst` são ignorados pelo Git.

`ferramentas/validar_progresso.py` valida a árvore completa e espera as evidências locais de todas as entregas. Este repositório publica apenas as seis capturas em `evidencias/publicas/`; as demais permanecem fora do controle de versão.

## Estado e medições

O estado vigente, as limitações e as retratações estão em `docs/publico/` e `dados/progresso.json`. Cada entrega usa um identificador estável `OTD-NNN`; uma conclusão corrigida permanece registrada em vez de ser apagada.

Os indicadores não são intercambiáveis: FPS, taxa de simulação de 20 Hz, tempo de quadro, tempo de render e custo de submissão medem coisas diferentes. As comparações A/B registram cena, câmera, build, configuração, emulador e relógio.

## Créditos e licença

- Lógica, tipos, macros F3DEX2 e tabelas integrados: [zeldaret/oot](https://github.com/zeldaret/oot), sob a licença do repositório de origem.
- Plataforma e bibliotecas: devkitPro, libnds, libfat, Calico e melonDS, cada um sob sua licença.
- O código próprio deste port ainda não declara uma licença neste repositório.
- *The Legend of Zelda* e *Ocarina of Time* são marcas da Nintendo. Este é um projeto independente de engenharia, sem afiliação com a Nintendo.

A apresentação pública fica no portfólio [AgonisingBasher](https://agonisingbasher.github.io/).
