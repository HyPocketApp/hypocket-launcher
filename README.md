# Mossling

Um jogo 2D de aventura com **vista de cima**, feito em C com
[raylib](https://www.raylib.com/), que roda em **PC** (Windows, Linux e macOS)
e **Android**.

Você é um pequeno ser de musgo. Em cada mapa, colete todas as **sementes**
para abrir o **portal** e passar para o próximo. Pelo caminho há **chaves**
que abrem **portas**, **corações** que devolvem vida, **espinhos** que
machucam, **besouros** que andam sem rumo e **gosmas** que perseguem você (e
aguentam dois golpes). São 3 mapas.

> Projeto independente. Não tem relação com o HyPocket nem com nenhum outro
> jogo: código, mapas, desenhos e sons são originais (os desenhos e sons são
> gerados pelo próprio código, não há arquivos de imagem ou áudio).

## Controles

| Ação | Teclado | Controle | Celular |
|---|---|---|---|
| Andar | WASD ou setas | Analógico esquerdo ou direcional | Arraste o dedo na metade esquerda da tela |
| Atacar | Espaço (ou J, Z, X) | A ou X | Toque na metade direita da tela |
| Pausar | P, Enter ou Esc | Start | Botão de pausa (canto superior direito) |

O texto do jogo aparece em português ou inglês conforme o idioma do sistema.

## Como compilar

O CMake baixa a raylib 6.2 sozinho na primeira compilação (precisa de
internet nessa hora). Para compilar sem internet, aponte para uma cópia
local: `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/caminho/da/raylib`.

### PC (Windows, Linux, macOS)

Precisa de CMake 3.22+ e um compilador C (Visual Studio, MinGW, GCC ou
Clang). No Linux, instale também as bibliotecas de janela:
`sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/mossling            # no Windows: build\Release\mossling.exe
```

`./build/mossling --touch` mostra os controles de toque no PC, para testar o
layout do celular com o mouse.

### Android

Precisa do Android SDK com a plataforma 36, o NDK 28.2.13676358 e o CMake
3.31.6 (instale pelo SDK Manager do Android Studio), e JDK 17 ou mais novo.

```sh
cd android
./gradlew assembleDebug     # no Windows: gradlew.bat assembleDebug
```

O APK fica em `android/app/build/outputs/apk/debug/app-debug.apk`. Também dá
para abrir a pasta `android/` no Android Studio e rodar direto no celular.
O app é só uma `NativeActivity`: todo o jogo é o código C de `src/`.

### Testes

As regras do jogo não dependem da raylib, então os testes rodam em qualquer
máquina, sem janela:

```sh
cmake -S . -B build-tests -DMOSSLING_GAME=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Eles conferem movimento, colisão, itens, portas, inimigos, ataque, dano e o
fluxo de telas, e também que em cada mapa todas as sementes, chaves e o portal
podem ser alcançados.

## Organização

| Caminho | O que é |
|---|---|
| `src/game.h`, `src/game.c` | As regras do jogo (C puro, sem raylib): mapas, movimento, inimigos, itens, telas. |
| `src/levels.c` | Os mapas, escritos como texto (a legenda está no topo do arquivo). |
| `src/main.c` | Janela, entrada (teclado, controle, toque), desenho e sons, com raylib. |
| `src/text.c`, `src/text.h` | Os textos do jogo em inglês e português. |
| `tests/test_game.c` | Testes das regras. |
| `android/` | Projeto Gradle que empacota o jogo como APK. |
| `CMakeLists.txt` | Compila o jogo para PC e Android, e os testes. |

### Criar ou mudar mapas

Edite `src/levels.c`. Cada caractere é um quadrado de 16x16 pixels:
`#` muro, `T` árvore, `~` água, `^` espinhos, `o` semente, `k` chave,
`D` porta, `h` coração, `X` portal, `P` início, `b` besouro, `s` gosma.
Rode os testes depois: eles avisam se algo ficou inalcançável.

## Pensado para celulares fracos

O jogo desenha numa tela virtual de 320x180 e amplia a imagem para a tela
real, só desenha o que está visível, não carrega imagens nem áudio de
arquivos e usa OpenGL ES 2. O APK tem cerca de 4 MB.

## Licença

Ainda não escolhida. O código de terceiros e suas licenças estão em
[THIRD_PARTY.md](THIRD_PARTY.md).
