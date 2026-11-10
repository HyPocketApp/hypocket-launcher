# HyPocket Blocks: lista de sons para gerar

Para gerar tudo de uma vez no Adobe Firefly (por exemplo, nos 7 dias de
teste), sem gastar crédito testando pedido. Regras em
[../assets/README.md](../assets/README.md); falas em
[voice-scripts.md](voice-scripts.md).

## Como usar

- **Ordem:** gere por prioridade. A 1 já toca no jogo assim que chegar; a 2,
  o protagonista e a 3 eu ligo no jogo quando chegarem; os ambientes e os
  próximos épicos vêm depois. A lista cresce junto com o jogo.
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

## Protagonista (efeitos, não fala)

Gerados como efeito, não como voz: um gerador de fala faz mal susto, esforço
e respiração. **Duas vozes:** tudo o que sai da boca do protagonista é gerado
duas vezes, trocando "[pessoa]" por "Uma mulher" e por "Um homem"; os
arquivos vão para `player/f/` e `player/m/` (por exemplo `player/f/grab_1`).
O que não sai da boca (barriga, coração) tem uma versão só, em `player/`.
Cada voz sempre com a mesma pessoa. Várias versões de cada um, para o jogo
variar.

As falas curtas de dica ("tá esfriando", "preciso comer") são fala, não
efeito: estão em [voice-scripts.md](voice-scripts.md), "Falas curtas de
dica", também nas duas vozes.

| Arquivo | Quando | Pedido | Duração |
|---|---|---|---|
| `player/grab_1..3` | Agarrado por um Nocturne | [pessoa] solta um susto curto e cortado, um puxão de ar de medo, sem palavras | 1 s |
| `player/struggle_1..4` | Cada vez que aperta Pular preso | [pessoa] fazendo força com os dentes cerrados, se debatendo, sem palavras | 1 s |
| `player/free_1..2` | Se soltou | [pessoa] ofegante de alívio e pânico depois de se soltar, respiração rápida | 2 s |
| `player/taken` | Levado | Grito abafado de [pessoa] se afastando e sumindo | 3 s |
| `player/wake_other` | Acorda do outro lado | [pessoa] puxando o ar de uma vez, como quem sai da água, respiração tremendo | 4 s |
| `player/back` | Volta pela luz | Suspiro longo de alívio de [pessoa], quase chorando | 3 s |
| `player/run_breath` | Correndo à noite com eles perto (loop) | Respiração ofegante e ritmada de [pessoa] correndo, sem passos | 6 s |
| `player/shelter` | Entra num abrigo | Respiração de [pessoa] desacelerando até ficar calma | 4 s |
| `player/hurt_1..3` | Perde vida | [pessoa] solta um gemido curto de dor, sem palavras | 1 s |
| `player/faint` | Desmaia | [pessoa] perdendo as forças, um suspiro fraco que some | 2 s |
| `player/wake` | Acorda depois de desmaiar | [pessoa] acordando devagar, um gemido baixo e uma tosse | 3 s |
| `player/hungry` (uma só) | Barriga vazia | Barriga roncando alto e longo, sem voz | 2 s |
| `player/cold_1..2` | Com frio | [pessoa] tremendo de frio, dentes batendo e respiração tremida | 2 s |
| `player/warm` | Esquenta perto do fogo | [pessoa] suspirando de alívio ao se aquecer, esfregando as mãos | 2 s |
| `player/eat_1..2` | Come | Mordida e mastigação curta de [pessoa] com fome, sem palavras | 1 s |
| `player/low_health` (uma só) | Vida baixa (loop) | Batimento cardíaco lento e forte, abafado, sem outros sons | 4 s |

## Prioridade 3: sobrevivência

| Arquivo | O que é | Pedido | Duração |
|---|---|---|---|
| `sfx/craft` | Fazer algo no inventário | Gravetos e pedras sendo amarrados com corda, curto | 1 s |
| `sfx/craft_fail` | Falta algo para fazer | Um toque abafado e curto de madeira oca, sem música | 1 s |
| `sfx/pickup` | Pegar um graveto ou pedra do chão | Pegar um graveto seco do chão, folhas mexendo, curto | 1 s |
| `sfx/pick_stone_1..2` | Golpe de picareta na pedra | Uma picaretada de pedra lascada contra rocha, curta e seca | 1 s |
| `sfx/axe_wood_1..2` | Golpe de machado na madeira | Uma machadada num tronco, curta e oca | 1 s |
| `sfx/hand_hit` | Bater com a mão sem ferramenta | Um soco abafado contra madeira, curto | 1 s |
| `sfx/no_tool` | Bloco duro demais sem ferramenta | Uma mão batendo em pedra, abafado e sem efeito | 1 s |
| `sfx/knife_wood` | Descascar madeira com a faca | Uma faca raspando casca de tronco, curto | 2 s |
| `sfx/saw` | Serrar | Uma serra de mão cortando uma tábua, três idas e vindas | 2 s |
| `sfx/hit` | Bater numa criatura | Um golpe abafado num corpo de animal, curto | 1 s |
| `sfx/eat_berries` | Comer frutinhas | Mastigar frutinhas suculentas, curto, sem voz | 1 s |
| `sfx/cook` | Assar carne na fogueira | Carne chiando numa grelha sobre o fogo | 3 s |
| `sfx/fry` | Fritar no fogão | Carne fritando na banha numa panela de ferro | 3 s |
| `sfx/kiln` | Forno aceso, de perto (loop) | Forno de barro com fogo forte e abafado, um ronco grave e constante | 8 s |
| `sfx/coat` | Vestir o agasalho | Casaco de couro e lã sendo vestido, roupa farfalhando | 2 s |
| `sfx/creature_1..3` | As criaturas mansas do dia | Um animal manso de pasto soltando um som curto, grave e tranquilo, ao ar livre | 2 s |
| `sfx/creature_hurt` | Criatura mansa atingida | Um animal manso de pasto assustado, um som curto e agudo | 1 s |
| `sfx/glass_break` | Quebrar vidro | Uma vidraça pequena quebrando, curto | 1 s |
| `sfx/door` | Porta (quando houver) | Uma porta velha de madeira abrindo devagar com rangido | 2 s |

## Ambientes do mundo

| Arquivo | O que é | Pedido | Duração |
|---|---|---|---|
| `ambient/night_insects` | Noite comum, sem névoa (loop) | Grilos e insetos noturnos ao longe, calmo, sem vento | 15 s |
| `ambient/dawn` | Amanhecer (loop) | Vários passarinhos cantando ao longe ao amanhecer, calmo | 15 s |
| `ambient/water` | Perto de um lago (loop) | Água de lago batendo de leve na margem, calmo | 10 s |
| `ambient/leaves` | Perto de árvores com vento (loop) | Folhas de árvores farfalhando com uma brisa leve | 10 s |
| `ambient/ruins` | Dentro de uma casa em ruína (loop) | Casa velha e vazia, madeira rangendo de leve, vento passando por frestas | 15 s |
| `ambient/sanctuary` | Perto de um farol aceso (loop) | Zumbido quente e calmo de luz, uma sensação de abrigo, sem música | 15 s |

## Para os próximos épicos (gere quando chegar a vez)

Já anotados para não esquecer; o jogo ainda não tem essas partes.

| Arquivo | Épico | O que é | Pedido | Duração |
|---|---|---|---|---|
| `sfx/phone_buzz` | 4, vozes | Celular vibrando ao achar um relato | Celular vibrando duas vezes em cima de uma mesa de madeira | 2 s |
| `sfx/phone_audio` | 4, vozes | Abrir um áudio no chat | Um clique curto de tela de celular | 1 s |
| `sfx/paper` | 4, vozes | Abrir um papel | Uma folha de papel velho sendo desdobrada | 1 s |
| `ambient/survivors` | 2, sobreviventes | Gente morando no santuário (loop) | Algumas pessoas conversando baixo ao redor de uma fogueira, palavras impossíveis de entender | 15 s |
| `sfx/survivor_arrives` | 2, sobreviventes | Um sobrevivente chega | Passos cansados se aproximando na terra e um suspiro de alívio | 3 s |
| `sfx/achievement` | 5, acabamento | Uma conquista ("Retorno ao lar") | Um tom suave e caloroso que sobe, curto, como um sino de vidro | 2 s |
| `ambient/lab` | 3, cientista | O laboratório (loop) | Máquinas antigas zumbindo, vidro tilintando, borbulhas, ambiente fechado | 15 s |
| `sfx/piano` | 3, cientista | O cientista ao piano na cutscene | Piano antigo e desafinado tocando notas graves e lentas, sombrio | 15 s |
| `sfx/light_trap` | 3, cientista | Armadilha de luz carregando | Um zumbido elétrico que sobe e estala no final | 3 s |
| `sfx/petrify` | 3, cientista | Petrificar | Pedra rachando e se fechando sobre algo vivo, grave | 3 s |
| `sfx/boss_breath` | 3, cientista | O cientista transformado, perto | Respiração enorme e úmida de uma criatura gigante, lenta | 4 s |

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
- **Protagonista:** as reações são efeitos (seção "Protagonista"). As
  falas curtas de dica são voz, nas duas vozes (feminina e masculina), e
  vêm depois do Lucas na ordem acima.
