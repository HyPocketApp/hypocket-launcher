# HyPocket Blocks: lista de sons para gerar

Para gerar tudo de uma vez no Adobe Firefly (por exemplo, nos 7 dias de
teste), sem gastar crédito testando pedido. Regras em
[../assets/README.md](../assets/README.md); falas em
[voice-scripts.md](voice-scripts.md).

## Como usar

- **Ordem:** gere por prioridade. A 1 já toca no jogo assim que chegar; a 2
  é da história e eu ligo no jogo quando chegar; a 3 é para depois.
- **Pedido:** copie o texto da coluna "Pedido". Se não gostar de nenhuma
  variação, mude uma palavra só (mais baixo, mais curto, mais longe), em vez
  de reescrever tudo.
- **Variações:** baixe as que gostar (até 2 por som). Eu escolho pelos
  números e você decide ouvindo no jogo.
- **Envio:** mande o arquivo como vier (WAV) aqui na conversa, dizendo o
  nome da coluna "Arquivo" e se mudou o pedido. Eu converto, junto em mono e
  acerto a emenda dos loops.
- **Duração:** use a da coluna. Loop pede começo e fim parecidos.
- **Sempre:** sem música, sem voz, sem outros sons junto, a não ser que o
  pedido diga.

## Prioridade 1: já toca no jogo

| Arquivo | O que é | Pedido | Duração |
|---|---|---|---|
| `ambient/wind` | Vento de fundo, o tempo todo (loop) | Vento noturno baixo e contínuo em campo aberto, sem rajadas fortes, sem assobio, sem outros sons | 15 s |
| `sfx/step` | Passo na grama | Um único passo de bota em grama baixa, curto e suave | 1 s |
| `sfx/step2` | Outro passo (alterna com o primeiro) | Um único passo de bota em terra e grama, curto, um pouco mais grave | 1 s |
| `sfx/break` | Quebrar um bloco | Terra e pedra se esfarelando de uma vez, curto e seco | 1 s |
| `sfx/place` | Colocar um bloco, abrir um papel | Uma batida seca e curta de madeira em madeira | 1 s |
| `sfx/jump` | Pulo | Deslocamento de ar curto de alguém pulando, roupa mexendo, sem voz | 1 s |
| `sfx/splash` | Cair na água | Uma pessoa caindo num lago raso, respingo médio | 2 s |
| `sfx/chirp` | Passarinho, de vez em quando | Passarinho pequeno ao longe, três pios rápidos e agudos, de manhã | 2 s |

## Prioridade 2: a história (eu ligo no jogo quando chegar)

| Arquivo | O que é | Pedido | Duração |
|---|---|---|---|
| `ambient/hum` | O zumbido da volta, do outro lado (loop) | Zumbido grave, contínuo e misterioso, tom estável e quente, como uma luz que chama | 10 s |
| `ambient/mist` | A névoa chegando (loop) | Sussurro de vento muito baixo e frio, quase como vozes distantes sem palavras, inquietante | 15 s |
| `ambient/other_side` | O fundo do outro lado (loop) | Ambiente escuro e vazio, um grave fundo e constante, eco distante, sem melodia | 15 s |
| `sfx/noc_breath` | Um Nocturne perto | Respiração lenta e funda de uma criatura grande bem perto, sem rosnado, sem rugido | 4 s |
| `sfx/noc_notice` | Um Nocturne nota você | Um estalo seco e curto, como um galho grosso quebrando, assustador | 1 s |
| `sfx/noc_drag` | Ser arrastado | Algo pesado sendo arrastado devagar pela terra e pelo mato | 3 s |
| `sfx/noc_fade` | Um Nocturne se desfaz na luz | Chiado de algo se desfazendo em fumaça, curto, como brasa apagada com água | 2 s |
| `sfx/beacon_on` | Acender um farol | Um zumbido luminoso que sobe e se firma, sensação de alívio, sem música | 3 s |
| `sfx/rift_close` | A fenda fechando | Um rasgo no ar se fechando, um vácuo curto e um baque grave | 2 s |

## Prioridade 3: sobrevivência (para depois)

| Arquivo | O que é | Pedido | Duração |
|---|---|---|---|
| `sfx/eat` | Comer | Uma mordida e mastigação curta de pão ou carne, sem voz | 1 s |
| `sfx/craft` | Fazer algo no inventário | Gravetos e pedras sendo amarrados com corda, curto | 1 s |
| `sfx/hit` | Bater numa criatura | Um golpe abafado num corpo de animal, curto | 1 s |
| `sfx/pickup` | Pegar um graveto ou pedra do chão | Pegar um graveto seco do chão, folhas mexendo, curto | 1 s |
| `sfx/cook` | Assar carne na fogueira | Carne chiando numa grelha sobre o fogo | 3 s |
| `sfx/fry` | Fritar no fogão | Carne fritando na banha numa panela de ferro | 3 s |

## Vozes

As falas estão em [voice-scripts.md](voice-scripts.md), com o nome de cada
arquivo e a direção. Para gerar fala:

- **Só voz de modelo da Adobe.** Se a voz vier de modelo marcado como de
  outra empresa, não use (regras em ../assets/README.md).
- **Cada personagem com a mesma voz em todas as falas.** Anote qual voz
  escolheu para cada um e me diga.
- **Gaste com cuidado:** voz costuma custar mais crédito que efeito. A ordem
  que eu seguiria:
  1. **Lucas** (4 falas): é o relato mais importante da história.
  2. **Caio e a mãe** (2 + 2 falas).
  3. **Seu Jorge, Marta e Bia** (1 fala cada).
  4. **Papéis lidos em voz alta:** Dona Lurdes (`note1_voz`), Ana
     (`note5_voz`, voz de criança), seguidor da névoa (`note7_voz`), o que
     fugiu (`note8_voz`, sussurro) e o autor do relato 0 (`note0_voz`, o
     texto mais longo). O texto a ler é o do papel no jogo; eu te passo
     cada um quando chegar a vez.
- **Protagonista:** são gemidos, sustos e respiração, não frases. Gerador de
  fala costuma fazer isso mal; esses ficam melhores gravados por você, no
  celular, como explica o roteiro.
