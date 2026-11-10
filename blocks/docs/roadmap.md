# HyPocket Blocks: roteiro de trabalho

O plano do jogo, para qualquer sessão, com ou sem acesso ao Jira. Os épicos
têm cópia no Jira (projeto `HPK`, rótulo `blocks`), mas a fonte é este
arquivo: quem terminar uma parte marca aqui no mesmo commit.

A história e as decisões estão em [story.md](story.md); os roteiros de voz em
[voice-scripts.md](voice-scripts.md).

## Regras para quem pegar uma parte

- Trabalhe só em `blocks/`, na branch combinada. `:blocks` usa só a interface
  pública do motor, nada de `:app` ou `:core`.
- Tudo original: nada de nomes, visual ou mecânicas marcantes de outro jogo.
- Texto de tela em `res/values/strings.xml` e `res/values-pt-rBR/strings.xml`.
- Cabe no Galaxy M12: poucas chamadas de desenho, nada pesado por quadro.
- Save novo pede versão nova do HPBW, e mundos antigos continuam abrindo.
- Antes de cada APK: build, testes e lint. APKs com o nome
  `hypocket-blocks-NN.apk`.
- Uma parte por commit (ou mais, quando ficar melhor), e todas no mesmo APK.

## Épicos, em ordem

| # | Épico | Jira | Estado |
|---|---|---|---|
| 1 | Sobrevivência viva e dinâmica | HPK-22 | Primeira parte no jogo |
| 2 | Sobreviventes nos santuários | HPK-23 | A fazer |
| 3 | O cientista, o laboratório portátil e os finais | HPK-24 | A fazer |
| 4 | Vozes | HPK-25 | Jogo já toca arquivos; sons sendo gerados |
| 5 | Acabamento | HPK-26 | A fazer |
| 6 | Desempenho no M12 | HPK-27 | Contínuo |
| 7 | Base nativa (celular e PC) e mods | ainda sem tarefa | Primeira versão no jogo; falta testar no celular e no Windows |

**Ordem de agora:** terminar o áudio do épico 4 (o jogo tocar os sons
gerados), depois o **7**, depois o 2, o 3 e o resto. O 7 vem antes dos
sobreviventes porque, quanto mais o jogo cresce, mais caro fica trocar a
base. O 1 continua aos poucos, com o retorno dos testes.

### 1. Sobrevivência viva e dinâmica

Criativa e original, sem cópia direta. O mundo tem gosto de vida: o dia é
leve, a noite é pesada, e o contraste equilibra a dificuldade.

As regras estão em [story.md](story.md), "Oitava rodada".

- [x] 1 madeira vira 1 tábua (agora com faca).
- [x] Itens que não são blocos: gravetos, pedra lascada, ferramentas, comida,
  couro, lã, banha e agasalho.
- [x] Ferramentas que tornam coisas possíveis: faca, serra (achada),
  picareta e machado improvisados. Blocos com dureza.
- [x] Gravetos e pedras pelo chão, que mudam a cada dia; faca e serra nas
  ruínas.
- [x] Bancadas pelo lugar: forno (vidro, tijolo, reboco), fogueira (modelo
  baixo com chama, fumaça e faíscas; assar, luz, calor) e fogão (fritar na
  banha; alguns nas ruínas).
- [x] Caça das criaturas do amanhecer: carne, banha, couro e lã.
- [x] Fome e frio, com o agasalho.
- [ ] Criar as criaturas (farm).
- [ ] Mais roupas e utensílios; desgaste das ferramentas improvisadas.

### 2. Sobreviventes nos santuários

Um farol aceso traz sobreviventes. Missões curtas e recompensa emocional (o
reencontro do Caio com a mãe), conquistas como "Retorno ao lar".

### 3. O cientista, o laboratório portátil e os finais

- O covil é uma dimensão portátil, que só abre (numa cutscene) com todos os
  requisitos do final "ajudar". Ele a abriu de propósito e observa o jogador.
- Um laboratório claro. O cientista, meio humano e meio Noc, aguenta mais luz.
  A pista é um meio Noc anterior que sofre com uma luz forte.
- A luta é uma corrida pela sobrevivência até achar a armadilha de luz.
- Depois: o covil vira museu escurecido, com vidros que não quebram. A luta não
  se repete e o jogo entra em modo livre. A produção da substância de
  despetrificação fica na sala do chefe.
- Os dois finais: o museu, ou experimentos eternos e paz para a humanidade.
- A decidir: um jogador só, ou até uns 10 com o chefe se adaptando.

### 4. Vozes

Play nos relatos e áudios nos chats (Ogg Vorbis mono). Banco de vozes
compactado e escolhido pela semente. As gravações ficam com o autor, que pode
gravar ou gerar no Adobe Firefly (story.md, "Nona rodada"). Os arquivos
finais vão para [../assets/](../assets/README.md), com a origem registrada.

- [x] Pasta de arte e áudio, regras e registro de origem.
- [x] Tocar arquivos Ogg Vorbis no jogo: trocam os sons feitos no código,
  somam o estalo da fogueira e o vento, e leem os papéis em voz alta.
- [ ] Áudios dos celulares (balão com play no chat).
- [ ] Ler texturas de arquivo, quando chegarem as primeiras.

### 5. Acabamento

Animações e modelos (hoje tudo é caixa: Nocs, criaturas, relatos, fendas,
portal), sons de ambiente, um tutorial leve na primeira noite, conquistas.

### 7. Base nativa (celular e PC) e mods

O jogo deixa de depender do emulador: o mesmo código em C vira um app
Android nativo e um programa de PC (Windows e Linux). A versão emulada
(x86-64 rodando pelo Box64) continua existindo, para não quebrar os testes
do motor do launcher.

- [x] **Gráficos para os dois:** o jogo roda em OpenGL ES 3 (celular) e
  OpenGL 3.3 (PC), pelo mesmo código (`platform.h`).
- [x] **App Android nativo,** sem Box64 nem VirGL (`platform_android.c`,
  `libblocks.so`). É o padrão; "Rodar emulado" nas opções de teste usa o
  programa x86-64 pelo motor, e o benchmark continua emulado.
- [x] **Versão de PC** com SDL2, Linux e Windows (`platform_sdl.c`,
  `build-pc.sh`): teclado, mouse, controle, som, janela que muda de tamanho
  e tela cheia (F11).
- [x] **Telas dentro do jogo** (`screens.c`): barra de itens, vida, fome e
  calor, avisos, inventário com receitas, menu, ajustes e relatos, em
  português e inglês, e controles de toque. Os textos, as coisas e as
  receitas vêm do lado Android por um gerador (`tools/gen_tables.py`), para
  não haver duas cópias.
- [x] **Mods, pacotes de dados** (`mods.c`): blocos e itens novos, receitas,
  texturas PNG e sons. Guia em [mods.md](mods.md).
- [x] **Mods, scripts em Lua** (`scripts.c`): eventos e chamadas para o
  mundo, o inventário, vida, fome, calor e hora; afinar e trocar a fome e o
  frio do jogo.
- [x] **Mods em qualquer linguagem (WebAssembly)** (`scripts_wasm.c`, com
  wasm3): as mesmas chamadas do Lua.
- [x] **Perfil do jogador:** nome e voz (feminina ou masculina); o jogo toca
  os sons da protagonista da pasta da voz escolhida.
- [ ] Testar no Galaxy M12 (nativo) e numa máquina Windows.
- [ ] Modelos do Blockbench nos pacotes de dados (épico 5, com os modelos).
- [ ] Trocar pelos mods mais sistemas do jogo (criaturas, geração do mundo,
  telas) e dar limite de tempo ao WebAssembly.
- [ ] No celular, as telas do jogo como padrão em todo mundo (hoje só nos
  mundos com mods ou com a opção de teste), e as configurações delas salvas.
- [ ] Na versão de PC, uma tela de mundos (hoje um mundo só, ou `--world`).

Como os mods funcionam (decidido com o autor):

- **Sem limite dentro do jogo, sem pesar na batata.** O jogo oficial
  continua em C, rápido, mas cada sistema dele (fome, frio, criaturas,
  receitas, geração do mundo, telas) pode ser trocado por um mod. Se nenhum
  mod troca, roda o C puro, sem custo de Lua. O conteúdo base (blocos,
  itens, receitas) continua no código, e os pacotes dos mods somam a ele e
  tiram partes dele. Um mod pode tirar ou refazer qualquer coisa, até virar
  outro jogo, e só paga desempenho pelo que ele mesmo põe em Lua. Cada passo
  é medido no M12.
- **O limite é só fora do jogo:** um mod não lê nem apaga arquivos fora da
  pasta dele e do mundo, não acessa a internet sozinho e não roda programas.
  É isso que protege o celular de quem entra num mundo com mods.
- **Os mods moram no mundo.** O mundo guarda os mods que usa; quem entra num
  mundo (ou servidor) baixa os mods sozinho e joga, sem instalar nada antes.
  Isso só é seguro e portável porque os mods são dados, Lua ou Wasm, e não código
  compilado para uma máquina (`.dll`, `.so`): o mesmo mod, em Lua ou Wasm,
  roda no PC e no celular.
- **Servidores grandes:** o autor mira 1000 a 3000 jogadores num servidor
  oficial de mod (mapa fixo, com menos sistemas). Isso pede servidor
  dedicado (PC ou nuvem, não celular), servidor que decide tudo e manda a
  cada jogador só o que está perto dele, e, acima de algumas centenas,
  dividir o mapa entre vários processos. Fica para o épico do multiplayer,
  mas a rede e o formato dos mods já nascem pensando nisso.

### 6. Desempenho no M12

O autor testa cada APK no Galaxy M12 (toque) e no A24 (teclado e mouse).
Pendente: a rodada da história e as correções do primeiro teste do A24.
