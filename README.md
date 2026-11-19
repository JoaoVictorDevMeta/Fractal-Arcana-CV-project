# 🧙 Duelo de Magos

Jogo de duelo de magos para **dois jogadores**, controlado por **visão computacional** com **C++17** e **OpenCV**.

Cada jogador segura uma varinha e desenha uma forma geométrica no ar. Ao reconhecer a forma, o jogo lança o feitiço correspondente em direção ao oponente. Vence quem zerar a vida do adversário primeiro.

---

## Como funciona

A tela é **única**, dividida ao meio:

```
┌──────────────────────┬──────────────────────┐
│                      │                      │
│     JOGADOR 1        │     JOGADOR 2        │
│    (metade esq.)     │    (metade dir.)     │
│                      │                      │
│   ✨ ──► feitiço ──────────────► 💥         │
│                      │                      │
└──────────────────────┴──────────────────────┘
        uma única câmera, imagem espelhada
```

- Uma única câmera captura os dois jogadores lado a lado.
- A imagem é espelhada e dividida verticalmente: o Jogador 1 joga na metade esquerda e o Jogador 2 na metade direita.
- Cada varinha tem uma cor de ponta diferente (ex.: verde e magenta) e é procurada apenas na metade do respectivo jogador.
- Os feitiços atravessam a tela horizontalmente até atingir o oponente.

Pipeline por frame:

```
Câmera → Rastreamento da varinha → Trajetória → Segmentação do gesto
      → Classificação da forma → Feitiço → Lógica de jogo → Renderização
```

## Feitiços

| Forma | Feitiço | Efeito |
|---|---|---|
| Triângulo | Lança de Gelo | Projétil rápido, dano baixo, retarda o alvo |
| Círuclo | Escudo | Bloqueia o próximo projétil |
| Raio (zigue-zague) | Relâmpago | Projétil muito rápido, dano alto, custo alto de mana |
| Linha horizontal | Rajada Arcana | Projétil básico, custo baixo |
| Estrela | Especial | Projétil mais poderoso do jogo |

Os valores de dano, custo de mana e cooldown ficam em `config/config.yaml` e podem ser balanceados sem recompilar.

## Requisitos

**Software**

- Compilador com suporte a C++17 (GCC 9+, Clang 10+ ou MSVC 2019+)
- CMake 3.16 ou superior
- OpenCV 4.x (módulos `core`, `imgproc`, `highgui`, `videoio`, `video`, `imgcodecs`)

**Hardware**

- 1 webcam (720p / 60 fps recomendado)
- 2 varinhas com ponta luminosa (LED) de cores bem diferentes
- Ambiente com iluminação estável e fundo sem objetos nas cores das varinhas
- Dica: reduza a exposição da câmera para que o LED apareça como um ponto brilhante e o fundo escureça

## Instalação e build

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential cmake libopencv-dev
git clone <url-do-repositorio> duelo-magos
cd duelo-magos
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Windows

1. Instale o OpenCV e defina a variável de ambiente `OpenCV_DIR`.
2. Gere o projeto e compile:

```bash
cmake -S . -B build -DOpenCV_DIR=<caminho-do-opencv>/build
cmake --build build --config Release
```

## Executando

```bash
./build/duelo_magos            # câmera padrão (índice 0)
./build/duelo_magos --camera 1 # outra câmera
```

### Primeira execução: calibração

1. Ao iniciar, o jogo entra no modo de calibração.
2. O Jogador 1 posiciona a varinha na área indicada na metade esquerda; o Jogador 2 faz o mesmo na metade direita.
3. O sistema coleta a faixa de cor HSV de cada varinha e salva em `config/config.yaml`.
4. Se necessário, ajuste as faixas em tempo real pelas barras de controle (trackbars) do modo debug.

## Como jogar

1. Fique na sua metade da tela, com a varinha visível para a câmera.
2. **Pare a varinha por um instante**, desenhe a forma no ar e **pare novamente** para finalizar o gesto.
3. Se a forma for reconhecida, o feitiço é lançado contra o oponente (desde que haja mana e o cooldown permita).
4. Use o **Escudo** (quadrado) para se defender.
5. Zere a vida do adversário para vencer. Partidas são melhor de 3 rodadas (configurável).

Cada jogador começa com **100 de vida** e **100 de mana** (regeneração de 8 por segundo).

### Atalhos de teclado

| Tecla | Ação |
|---|---|
| `C` | Recalibrar cores das varinhas |
| `D` | Ativar/desativar modo debug |
| `R` | Reiniciar partida |
| `Espaço` | Pausar |
| `ESC` | Sair |

## Estrutura do projeto

```
duelo-magos/
├── CMakeLists.txt
├── README.md
├── assets/            # sprites PNG com alpha (feitiços, HUD)
├── config/            # config.yaml (cores HSV, limiares, balanceamento)
├── src/
│   ├── main.cpp
│   ├── core/          # game loop, configuração, timer
│   ├── vision/        # captura, rastreamento da varinha, trajetória
│   ├── gesture/       # pré-processamento, segmentação, classificador de formas
│   ├── game/          # jogador, feitiços, projéteis, estado e regras
│   └── render/        # renderização, sprites, HUD
└── tests/             # testes unitários e dataset de gestos
```



## Como o reconhecimento funciona

**Rastreamento:** flip do frame, conversão para HSV, `inRange` por jogador (dentro da sua metade), limpeza morfológica, maior contorno e centroide via `moments`. Um filtro de Kalman suaviza o movimento e preenche pequenas perdas de detecção.

**Gesto:** a trajetória é reamostrada em 64 pontos equidistantes e normalizada em escala e posição. O gesto começa e termina por pausas da varinha.

**Classificação:** combina um classificador geométrico (`approxPolyDP`, circularidade, `fitEllipse`, `fitLine`, ângulos entre segmentos) com comparação por templates. Gestos abaixo do limiar de confiança são rejeitados.

Detalhes completos estão no documento de especificação do projeto.

## Metas de desempenho

| Métrica | Meta |
|---|---|
| Taxa de quadros | ≥ 30 FPS |
| Latência do fim do gesto até o feitiço | ≤ 150 ms |
| Acurácia de reconhecimento | ≥ 90% por forma |
| Falsos positivos | < 5% |

## Solução de problemas

| Problema | O que tentar |
|---|---|
| A varinha não é detectada | Recalibre (`C`), reduza a exposição da câmera, evite fundos com a mesma cor da ponta |
| As varinhas se confundem | Use cores mais distantes no matiz (ex.: verde e magenta) |
| Feitiços disparando sem querer | Aumente o tempo de pausa e o limiar de confiança no `config.yaml` |
| Formas não reconhecidas | Desenhe maior e mais devagar; confira a trajetória no modo debug (`D`) |
| FPS baixo | Compile em Release, reduza a resolução de processamento, feche outros apps que usem a câmera |

## Roadmap

- [+-] M1: captura e rastreamento de uma varinha com calibração
- [ ] M2: duas varinhas, ROI por jogador, filtro de Kalman
- [x] M3: segmentação de gestos e classificador de formas básicas
- [ ] M4: raio, reforço por template e matriz de confusão
- [ ] M5: lógica de jogo (vida, mana, projéteis, colisão, escudo)
- [ ] M6: renderização completa (sprites, HUD, menus)
- [ ] M7: polimento, playtests e balanceamento

## Ideias futuras

- Multiplayer em rede com uma câmera por jogador
- Novos feitiços e combos de formas
- Rastreamento sem LED (varinha reflexiva/IR)
- Efeitos sonoros e partículas

## Licença

Defina a licença do projeto (ex.: MIT) e adicione o arquivo `LICENSE`.
