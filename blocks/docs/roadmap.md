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
| 4 | Vozes | HPK-25 | Roteiros prontos, falta gravar |
| 5 | Acabamento | HPK-26 | A fazer |
| 6 | Desempenho no M12 | HPK-27 | Contínuo |

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

Play nos relatos e áudios nos chats (OGG/Opus mono). Banco de vozes
compactado e escolhido pela semente. As gravações ficam com o autor, que pode
gravar ou gerar no Adobe Firefly (story.md, "Nona rodada"). Os arquivos
finais vão para [../assets/](../assets/README.md), com a origem registrada.

- [x] Pasta de arte e áudio, regras e registro de origem.
- [ ] Tocar arquivos OGG/Opus no jogo (hoje os sons são feitos no código).
- [ ] Ler texturas de arquivo, quando chegarem as primeiras.

### 5. Acabamento

Animações e modelos (hoje tudo é caixa: Nocs, criaturas, relatos, fendas,
portal), sons de ambiente, um tutorial leve na primeira noite, conquistas.

### 6. Desempenho no M12

O autor testa cada APK no Galaxy M12 (toque) e no A24 (teclado e mouse).
Pendente: a rodada da história e as correções do primeiro teste do A24.
