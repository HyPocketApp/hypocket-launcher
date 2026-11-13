# Timberheart

Um **RPG de turno** em 2D, com vista de cima, feito em C com
[raylib](https://www.raylib.com/). Roda em **PC** (Windows, Linux e macOS) e
**Android**.

Você é um **lenhador** e sua arma é o **machado**. A caminho de uma vila
próxima, ele salva uma mulher de branco atacada por pequenos seres de fogo:
ela é a **Deusa da Lua**, que perdeu grande parte dos seus poderes, e esses
seres querem roubar o que resta. A história está em
[docs/LORE.md](docs/LORE.md).

- **Explorando o mapa:** ande livremente, **derrube árvores** com o machado
  para abrir caminhos, abra **baús** (ervas ou uma lâmina mais afiada) e
  **descanse nas fogueiras** para recuperar tudo. Se você cair numa batalha,
  acorda na última fogueira em que descansou, sem perder nada.
- **Batalhas por turno:** ao tocar num monstro, começa uma batalha. Você
  escolhe uma ação e depois cada monstro age:
  - **Machadada:** um golpe simples (às vezes crítico).
  - **Técnicas:** gastam fôlego (FÔ). **Golpe Rachador** é um golpe forte
    num alvo; **Corte Giratório** atinge todos os monstros.
  - **Erva:** recupera 30 de vida.
  - **Defender:** reduz o dano pela metade e recupera fôlego.
  - **Fugir:** nem sempre funciona, e do chefe não dá para fugir.
- **Evolução:** cada vitória dá experiência. Ao subir de nível, vida,
  fôlego, ataque e defesa aumentam. O machado corta mais forte em coisas de
  madeira.
- **3 mapas:** Bosque da Serraria, Vale dos Lobos e Clareira Ancestral, onde
  o Toco Ancião espera. Os monstros são fagulhas (os seres de fogo), gosmas,
  besouros e lobos.
- **Cenas de história:** telas de texto entre a ação. Enter ou toque
  avança; X pula a cena.

> Projeto independente. Não tem relação com o HyPocket nem com nenhum outro
> jogo: código, mapas, desenhos e sons são originais. Os desenhos e sons são
> gerados pelo próprio código, sem arquivos de imagem ou áudio. O nome do
> jogo é provisório.

## Controles

| Ação | Teclado | Controle | Celular |
|---|---|---|---|
| Andar / mover no menu | WASD ou setas | Analógico ou direcional | Arraste o dedo na metade esquerda da tela |
| Machado / escolher | Espaço, Enter ou Z | A | Toque na metade direita da tela (no menu, toque na opção ou no monstro) |
| Voltar | X ou Backspace | B | Toque no painel de baixo ao escolher o alvo, ou em "Voltar" |
| Pausar | P ou Esc | Start | Botão de pausa (canto superior direito) |

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
./build/timberheart            # no Windows: build\Release\timberheart.exe
```

`./build/timberheart --touch` mostra os controles de toque no PC, para testar
o layout do celular com o mouse.

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
cmake -S . -B build-tests -DTIMBERHEART_GAME=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Eles conferem a exploração (cortar árvores, pedras, baús, fogueiras, saída),
as batalhas (menus, alvos, técnicas, ervas, defesa, fuga, vitória, derrota,
subida de nível, chefe) e se todo mapa pode ser completado. Também simulam
centenas de batalhas para checar o equilíbrio: um lenhador novo vence as
gosmas, os lobos pedem alguns níveis, e o chefe vence quem chega despreparado.

## Organização

| Caminho | O que é |
|---|---|
| `src/game.h`, `src/game.c` | As regras do jogo (C puro, sem raylib): mapa, batalha, atributos, níveis, telas. |
| `src/levels.c` | Os mapas, escritos como texto (a legenda está no topo do arquivo). |
| `src/main.c` | Janela, entrada (teclado, controle, toque), desenho e sons, com raylib. |
| `src/text.c`, `src/text.h` | Os textos do jogo em inglês e português. |
| `tests/test_game.c` | Testes das regras. |
| `docs/LORE.md` | A história do jogo: o que já está decidido e o que falta decidir. |
| `android/` | Projeto Gradle que empacota o jogo como APK. |
| `CMakeLists.txt` | Compila o jogo para PC e Android, e os testes. |

### Criar ou mudar mapas

Edite `src/levels.c`. Cada caractere é um quadrado de 16x16 pixels:
`#` penhasco, `T` árvore (dá para cortar), `R` pedra, `~` água, `=` ponte,
`c` baú com ervas, `A` baú com lâmina afiada, `f` fogueira, `X` saída,
`P` início, `M` a Deusa da Lua. Monstros: `s` gosma, `g` duas gosmas,
`b` besouro e gosma, `v` lobo, `w` dois lobos, `e` seres de fogo (ficam
parados; não dá para fugir), `B` o chefe. Rode os testes depois: eles avisam se
algo ficou inalcançável.

### Mudar o equilíbrio

Os atributos dos monstros estão na tabela `species_table` em `src/game.c`; os
do lenhador, em `game_new()` e `finish_victory()`. O teste "balance" avisa se
uma mudança deixar o jogo fácil ou difícil demais.

## Pensado para celulares fracos

O jogo desenha numa tela virtual de 320x180 e amplia a imagem para a tela
real, só desenha o que está visível, não carrega imagens nem áudio de
arquivos e usa OpenGL ES 2. O APK tem cerca de 4 MB.

## Licença

Ainda não escolhida. O código de terceiros e suas licenças estão em
[THIRD_PARTY.md](THIRD_PARTY.md).
