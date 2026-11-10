# HyPocket Blocks: como fazer mods

Um mod muda um mundo: blocos e itens novos, receitas, texturas, sons e
scripts. O jogo base continua em C; o mod soma e troca partes dele. O mesmo
mod roda no celular e no PC, sem mudar nada.

Os detalhes técnicos estão no código: `blocks/game/mods.c` (arquivos de
dados), `blocks/game/scripts.c` (Lua) e `blocks/game/scripts_wasm.c`
(WebAssembly). Dois mods de exemplo estão em `blocks/mods/`.

## Onde os mods ficam

Os mods moram no mundo, numa pasta ao lado do save: `<save>.mods/`. Cada mod
é uma pasta dentro dela, e o jogo carrega todos em ordem alfabética.

- **Celular:** na lista de mundos, botão **Mods** do mundo → **Adicionar
  .zip**. O .zip pode ter os arquivos do mod direto ou uma pasta com eles.
  Um mundo com mods usa as telas do próprio jogo (as mesmas do PC), para
  mostrar as coisas e receitas novas.
- **PC:** a pasta fica ao lado do save do mundo. Sem `--world`, o save fica
  na pasta de dados do usuário (`world.hpw`), e os mods em `world.hpw.mods/`.
  `--world` escolhe outro save, ou `HYPOCKET_BLOCKS_MODS` aponta outra pasta.

Copiar um mundo copia os mods; apagar o mundo apaga os mods dele.

## O que vai num mod

| Arquivo | O que é |
|---|---|
| `pack.txt` | Nome, versão e descrição (opcional, para ler) |
| `things.txt` | Blocos e itens novos |
| `recipes.txt` | Receitas novas, e receitas do jogo tiradas |
| `textures/` | Imagens PNG 16×16 dos blocos |
| `sounds/` | Sons Ogg que trocam ou somam aos do jogo (`sounds/sfx/break.ogg`, `sounds/player/f/grab_1.ogg`...) |
| `init.lua` | Script em Lua |
| `*.wasm` | Scripts compilados para WebAssembly, de qualquer linguagem |

### things.txt

```
[block pedra_brilhante]
name = Glowing stone
name.pt = Pedra brilhante
color = #E0C860
accent = #8A6A20
texture = textures/pedra.png
light = 1
blows = 0
tool = pick
tool_blows = 2

[item queijo]
name = Cheese
name.pt = Queijo
kind = food
food = 3
color = #F0D040
```

Chaves dos blocos: `texture` (ou `texture.top`, `texture.side`,
`texture.bottom`), `light` (ilumina como lâmpada), `transparent`, `solid`
(0: atravessa), `blows` (golpes sem ferramenta; 0: não quebra na mão),
`tool` e `tool_blows`, `drops` (o que dá ao quebrar, por exemplo
`stone 2`). Itens: `kind` (`material`, `tool`, `food`, `clothing`) e
`food` (quanto enche, de 0 a 10). Os dois: `name`, `name.pt`, `color`,
`accent`.

As coisas do jogo têm nome em minúsculas: `planks`, `stone`, `wood`,
`stick`, `flint`, `knife`, `raw_meat`, `campfire` e as outras de
`BlocksPalette.kt`, mais `grass`, `water` e `air`.

Cabem uns 40 blocos e 18 itens novos por mundo, e até 48 coisas no
inventário ao todo. Cada coisa nova guarda o número dela para sempre
(`ids.txt` na pasta de mods), para o mundo continuar abrindo. Se um mod for
tirado, os blocos dele viram pedra.

### recipes.txt

```
stone 2, lamp 1 -> pedra_brilhante 2 | tool pick
berries 3 -> roasted_berries 1 | near campfire
remove planks
```

Até duas entradas, uma saída, e se quiser uma ferramenta (`tool`) ou uma
bancada por perto (`near`: `kiln`, `stove` ou `campfire`). `remove` tira as
receitas do jogo que fazem aquela coisa.

## Scripts

Lua e WebAssembly enxergam o mesmo jogo.

**Eventos** (funções que o script define, todas opcionais): `on_start()`,
`on_tick(dt)`, `on_break(x, y, z, coisa)`, `on_place(x, y, z, coisa)`,
`on_eat(coisa)`, `on_craft(coisa, quantas)`.

**Chamadas** (em Lua, `blocks.nome(...)`):

| Chamada | O que faz |
|---|---|
| `get_block(x, y, z)`, `set_block(x, y, z, coisa)` | Ler e trocar blocos |
| `player()`, `teleport(x, y, z)` | Onde o jogador está, e mudar |
| `count(coisa)`, `give(coisa, n)`, `take(coisa, n)` | O inventário |
| `health([v])`, `food([v])`, `warmth([v])` | Vida, fome e calor (0 a 10); com `v`, muda |
| `time([v])`, `day()` | Hora do dia (0 a 1, 0,5 é meio-dia) e dias passados |
| `say(texto)` | Uma mensagem na tela |
| `set(ajuste, valor)` | Afinar o jogo: `hunger`, `cold`, `walk`, `jump` (1 é o normal) |
| `replace(sistema)` | Desligar a `hunger` ou o `cold` do jogo, para o script cuidar |

Exemplo em Lua:

```lua
function on_eat(coisa)
    if coisa == "queijo" then
        blocks.warmth(blocks.warmth() + 1)
        blocks.say("Que queijo bom")
    end
end
```

Em WebAssembly as coisas são números (`thing_id("stone")`), os textos vão
como ponteiro e tamanho, e cada chamada é um import do módulo `blocks`. Veja
`blocks/mods/example-wasm/src/script.c`, que compila com
`clang --target=wasm32`.

## O que um mod pode e não pode

Dentro do jogo, tudo: o jogo foi feito para um mod poder mudar qualquer
parte, até virar outro jogo. Fora do jogo, nada: um mod não lê nem apaga
arquivos, não acessa a internet e não roda programas. É isso que deixa
seguro entrar num mundo com mods de outra pessoa.

- **Lua:** só as bibliotecas base, `table`, `string`, `math`, `utf8` e
  `coroutine`; nada de carregar código; até 32 MB; uma chamada que demora
  demais (um laço infinito) para aquele mod, não o jogo.
- **WebAssembly:** roda dentro da memória dele (até 64 MB). Ainda não tem
  limite de tempo: um laço infinito trava o jogo.

## Testar

Quando um mod tem erro, o jogo escreve o motivo no relatório da sessão
(linhas `blocks: [nome do mod] ...`) e mostra "nome: error" na tela; os
outros mods continuam.
