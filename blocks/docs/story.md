# HyPocket Blocks: história principal

Documento de trabalho para a história principal do jogo, escrito a partir do
debate com o autor da ideia. Cada parte diz o que já existe no jogo, o que
entra, o que sai e o que ainda está em aberto.

Referência: as ideias de objetivo propostas antes e o que delas já está no
jogo estão em [goal-ideas.md](goal-ideas.md).

## Regras do documento

- A história é original: nada de nomes, personagens, visual ou mecânicas
  marcantes de outro jogo.
- O que for decidido aqui precisa caber no celular mais fraco (Galaxy M12:
  perto de 60 fps no modo pesado, poucas chamadas de desenho).
- Mudanças no save pedem versão nova do formato HPBW, e os mundos antigos
  continuam abrindo.

## Estado

Segunda rodada implementada e testada por um jogador (Galaxy A24, teclado e
mouse): o retorno foi positivo, e os bugs que ele achou (menu com o mouse,
sair da água, buraco na água) foram corrigidos. Planejamento em épicos no
Jira (`HPK`, rótulo `blocks`), na ordem combinada em "Sexta rodada":
HPK-22 sobrevivência, HPK-23 sobreviventes, HPK-24 cientista e finais,
HPK-25 vozes, HPK-26 acabamento, HPK-27 desempenho no M12. O mesmo plano,
para quem trabalha sem o Jira, está em [roadmap.md](roadmap.md).
A oitava rodada decidiu a sobrevivência (épico 1): fome, frio, ferramentas,
bancadas e caça, no jogo desde então.

## A história, como foi contada

_(o texto do autor, sem edição, para consulta)_

> Vamos prosseguir com a história, eu imaginei um mundo num cenário
> catastrófico com vilas e até cidades destruídas pelos monstros, os Nocturnes
> ou coisa parecida, aí que entra os abrigos você tem que se refugiar em locais
> que não só eles não consigo entrar, mas locais que tbm não consigam ser
> cobertos pela névoa, a névoa vai fazer parte dos nocturnes, eles vão ser
> monstros que andam junto com a névoa, ressalva textos de sobreviventes,
> "eles vem com a névoa e a escuridão, se são um só com a névoa ou aproveitam
> ela a noite para fugir de fontes de luz não sabemos, a única informação que
> aqueles que ainda estão vivos sabem, é que eles não vão parar depois de
> encontrar você, criaturas espertas que farão de tudo para te arrastar, para
> onde? Também não fazemos ideia, tudo que sabemos é que não parecem comer
> carne humana, mas por que pegar sobreviventes e arrasta-los pra onde quer
> que seja o covil dessas coisas? Mais uma vez, não sabemos, o porque fazem
> isso é um mistério assustador, mas eles pegam qualquer ser vivo que
> encontrem e os arrastam pela névoa, criaturas difíceis de se matar, se vir
> uma não atire ou tente lutar, apenas corra, esse é o último ato de
> desespero que pode fazer para poupar sua vida, dessa forma, pude escrever
> esse relato para aqueles que virão até aqui. E saiba se estiver lendo isso,
> provavelmente, eu já estou morto."
>
> Aí tipo o jogo vai funcionar em duas frentes, a manhã vai ser responsável
> por sobrevivência e tocar o mundo, e a noite o jogo viraria praticamente um
> terror, não um sistema de zumbis como no mine por exemplo, que vc tem chance
> de vencer, aqui, vc deve fugir. As únicas coisas que os jogadores
> conseguiriam fazer farm seria nos mobs do amanhecer, criaturas bonitinhas em
> geral, mas não nos Nocturnes. A névoa meio que vem junto com eles, mas apesar
> do relato do sobrevivente, não penso em os dois serem uma coisa, a névoa é os
> Nocturnes estão relacionados, mas eu pensei deles vierem de outra dimensão
> até a nossa, e quando isso acontece a névoa do mundo deles escapa pra nossa,
> e daí começou o desastre. Pensei tbm nos faróis iluminarem pontos do mapa,
> como a névoa e escuridão, mas não sei se eles meio que fechariam os portais
> pra dimensão deles, tem que ver isso certinho. E sim, a dimensão deles é
> entravel, da pra vc até sobreviver lá, mas a dificuldade é insana e não sei
> o que fazer lá pra falar a verdade, não sei nem se vai ter desfecho com
> bosses tbm. Bom creio que esse é o panorama geral, oq acha?

## A segunda rodada, como foi contada

> Gostei, a 6 mesmo ficou incrível, da pra colocar como cada casa preenche uma
> parte do quebra cabeça, são pessoas com ideias diferentes supondo coisas
> diferentes e tendo crenças diferentes, e o mais importante, são indivíduos,
> alguns mesmo nem relato vai ficar, só a casa pra trás com móveis quebrados,
> talvez os relatos não precisem ser só papéis, telefone cll, com um chat de
> desespero para as pessoas próximas antes de sumir, e por aí vai.
> Pode implementar os nocturnes puxando, mas oque eu recomendo pra ficar melhor
> e eu testar só um apk, é fazer todos de uma vez, um por commit, ou mais de um
> commit quando ficar melhor, pra organizar certinho.
> A primeira ideia de o pq as pessoas são arrastadas é interessante, pq seguem
> duas coisas diferentes, um, mundo normal fuja, mundo dos Nocturnes, tbm fuja,
> mas correr já não é uma opção, até pq vc não sabe como voltar e se tem como.
> O inventário de quem é arrastado pode ficar por enquanto.
> Isso tudo que falou, pra fazer agasalhos roupas e utensílios improvisados pra
> sobrevivência, banha pra fritar os alimentos e por aí vai. Dá pra fazer um
> negócio bem maneiro aqui.
> O nome Nocturne é um que me veio a cabeça do nada, não tem significado que eu
> conheça, ent pode sugerir outros nomes. Na verdade eles podem ser conhecidos
> por nomes diferentes dependendo do local ou da pessoa se pá, até nós relatos
> pode conter isso, alguns chamando de coisas sombrias, demônios da névoa, que
> assim reflete oq eu falei, são pessoas diferentes e individuais, que vão
> tratar eles de jeitos diferentes.
> O jogador não tem nome e passado, pelo menos não pré-definido, ou até pode
> criar um md de past of the player pra deixar em aberto, agora o nome é
> customizavel, pq pretendo colocar multiplayer,apesar de que multiplayer pra
> brasileiro faz todo o terror sumir kkkkkkk.
> Quanto a última coisa, eu pretendo fazer as animações em outra conta,
> modelagens tbm usando ia, já tô vendo na outra conta qual é o melhor agente
> pra isso e tals, isso vai deixar sua carga bem leve e direta.

## Decisões da segunda rodada

- **Tudo entra agora**, num APK: Nocturnes que perseguem e arrastam, abrigos,
  ruínas com relatos, fendas nos faróis e a dimensão.
- **Relatos são de indivíduos.** Cada casa é uma peça do quebra-cabeça: gente
  diferente, com suposições e crenças diferentes. Há casas sem relato nenhum,
  só os móveis quebrados. Os relatos podem ser papéis ou **celulares** com a
  conversa de desespero mandada às pessoas próximas antes de sumir.
- **Os nomes variam.** Cada pessoa chama as criaturas do seu jeito ("coisas
  sombrias", "demônios da névoa", "os de lá"...). "Nocturnes" é só o nome de
  trabalho; o jogo não dá nome oficial a elas. Sugestões para um nome de
  trabalho: **Velados**, **Brumosos**, **Arrastadores**, **Os da Névoa**.
- **Por que arrastam:** fica o mistério, e a dimensão é para onde levam. As
  duas frentes: no nosso mundo, fuja; no mundo deles, também, mas correr já
  não adianta, porque lá eles são mais rápidos e você não sabe como voltar.
- **Inventário de quem é arrastado:** fica com o jogador, por enquanto.
- **O que as criaturas do amanhecer dão:** couro, lã, carne e banha, para
  agasalhos, roupas, utensílios improvisados e comida frita. Fica para a
  próxima rodada, porque pede decidir antes fome e frio (ver "Em aberto").
- **O jogador** não tem nome nem passado definidos; o nome será escolhido
  pelo jogador (pensando no multiplayer). O passado fica em aberto em
  [player-past.md](player-past.md).
- **Animações e modelos** serão feitos em outra conta, com IA. Aqui o jogo
  continua com caixas simples e deixa os pontos de troca claros.

## O mundo

- Um mundo depois de uma catástrofe: vilas e até cidades destruídas pelos
  Nocturnes, hoje em ruínas.
- A causa: os Nocturnes vieram de outra dimensão. Quando atravessam, a névoa
  do mundo deles vaza para o nosso. Foi assim que o desastre começou.
- A névoa e os Nocturnes estão ligados, mas não são a mesma coisa. Os
  sobreviventes não sabem disso (o relato diz "se são um só com a névoa...
  não sabemos"), e o jogador descobre aos poucos.
- Os sobreviventes deixaram relatos escritos espalhados pelo mundo. O texto
  acima é o primeiro deles.

**Proposta (a debater):** cada farol apagado fica ao lado de uma fenda, um
portal pequeno por onde a névoa vaza. Hoje a névoa já é mais forte perto do
farol apagado; na história, é a fenda que a solta. Acender o farol sela a
fenda e a névoa da região some. A história já explica o que o jogo faz hoje,
e quase nada do código muda.

## Personagens e criaturas

- **O jogador:** um sobrevivente que chega depois do desastre ("para aqueles
  que virão até aqui"). Nome e passado ainda em aberto.
- **Criaturas do amanhecer:** bonitinhas e pacíficas. São as únicas que dá
  para caçar e criar ("farm").
- **Nocturnes:** vêm com a névoa e com a escuridão. São espertos, não param
  depois que acham alguém e são difíceis de matar. Não comem quem pegam: pegam
  qualquer ser vivo e o arrastam pela névoa até o covil. Ninguém sabe por quê.
  A regra do jogo é **fugir, não lutar**.

**Proposta (a debater):** quando um Nocturne pega o jogador, ele não tira vida
e pronto: ele agarra e arrasta. Na tela, a visão escurece, o jogador é puxado
na direção da névoa e pode se soltar apertando Pular repetidas vezes ou
acendendo luz (uma lâmpada na mão ou colocada no chão). Se não se soltar a
tempo, acorda **na dimensão deles**. Isso responde, no próprio jogo, à
pergunta do relato ("para onde?") e dá uma porta de entrada para a dimensão.

## Objetivo e etapas

As duas frentes:

1. **De dia, sobreviver e tocar o mundo:** explorar as ruínas, juntar
   materiais, fabricar, criar as criaturas do amanhecer, achar relatos e
   construir abrigos.
2. **De noite, terror:** a névoa avança, os Nocturnes saem e o jogador foge
   para um abrigo.

**Proposta de ordem (a debater):**

1. Chegar ao mundo, achar o primeiro relato numa ruína perto do início.
2. Construir o primeiro abrigo antes da primeira noite.
3. Achar e acender os faróis, um por um. Cada um sela uma fenda e devolve uma
   região do mundo. Os relatos dão pistas de onde ficam.
4. Com todas as fendas seladas, sobra o portal de onde a névoa vem, que um
   farol não fecha. É a entrada para a dimensão.
5. A dimensão (ver "Em aberto").

## Abrigos (da ideia 3, em parte)

Um abrigo é um lugar onde os Nocturnes não entram **e** a névoa não cobre.

**Proposta (a debater):** um cômodo fechado de blocos, com pelo menos uma
lâmpada dentro. O jogo confere de vez em quando (cerca de uma vez por
segundo, contando no máximo algumas centenas de blocos de ar a partir do
jogador) se o ar à volta está todo fechado. Se estiver:

- os Nocturnes não entram, e os que estão fora não atravessam a parede;
- a névoa fica do lado de fora, mesmo perto de uma fenda;
- o HUD mostra "Abrigado".

Uma porta aberta, ou um bloco quebrado por fora, desfaz o abrigo. Da ideia 3
original saem o placar por noite e as criaturas pacíficas que precisam ser
salvas, mas podem voltar mais tarde (por exemplo, levar criaturas do
amanhecer para dentro para elas não serem arrastadas).

## Final

Ainda sem final fechado. Primeira proposta: acender todos os faróis encerra o
primeiro ato, como já acontece hoje, e mostra o tempo. O mundo continua, a
névoa só volta à noite e longe dos faróis, e o portal principal fica aberto
para quem quiser entrar na dimensão.

## Como ficou no jogo

| Parte | Como funciona |
|---|---|
| Nocturnes | Não morrem: um golpe só atordoa por 2,5 s. Quem acha o jogador (22 blocos) não para até ele se afastar 48 blocos ou se abrigar. Andam a 3,6 (o jogador anda a 4,3), então dá para fugir. Também levam criaturas pacíficas, que voltam de dia. |
| Ser arrastado | Encostar agarra e arrasta para a névoa. Para se soltar: apertar Pular várias vezes, golpear quem segura, ou luz perto (fora da névoa). Preso 9 s, o jogador é levado para o outro lado. A tela escurece e uma barra mostra o quanto falta para se soltar. |
| Lâmpadas e névoa | Fora da névoa as lâmpadas afastam os Nocturnes. Dentro da névoa, não: por isso existem os abrigos. |
| Abrigos | Um cômodo fechado (a névoa passa por folhas e água); a lâmpada dentro é só para enxergar. O jogo confere 4 vezes por segundo, até 600 blocos de ar. Dentro, a névoa não aparece, os Nocturnes perdem o jogador e não agarram, e o HUD mostra "Abrigado". |
| Ruínas | Vilas (2, 3 ou 4, conforme o tamanho do mundo) de 4 casas entre o começo e os faróis, a partir da semente: paredes quebradas, restos de telhado, mesa caída, estante vazia, cama, entulho. Mundos antigos ganham as ruínas ao abrir, só onde não há nada construído. |
| Relatos | 9, de pessoas diferentes, cada uma com seu nome para as criaturas: o texto do autor perto do começo; a Dona Lurdes ("demônios da névoa"); o celular do Caio com a mãe ("uma coisa"); o técnico das torres, que descobriu as fendas ("os de lá"); o grupo da rua, com as dicas de abrigo ("sombras"); o bilhete da Ana sobre o cachorro; o celular do Lucas, que voltou do outro lado e ensina a fugir de lá; o folheto de quem acha que eles são mensageiros; e um rabisco no outro lado sobre a "fila" dos levados. Três casas ficam vazias. Aponte e use Colocar para ler; o menu de pausa guarda os encontrados. |
| Fendas | Ao lado de cada farol apagado, um rasgo escuro com bordas violeta que solta névoa. Acender o farol sela a fenda. |
| Santuário do farol | Até 40 blocos de um farol aceso: sem névoa, iluminado à noite, e as criaturas param na borda sem entrar. Um feixe de luz sobe do farol. |
| Portal | Entre dois faróis, um rasgo alto e irregular no ar, sem moldura, com pedras girando em volta, por onde eles vieram. Nenhum farol fecha. Entrar nele leva ao outro lado de propósito. |
| O outro lado | Mundo pequeno (6x6 chunks) de pedra-sombra, sempre noite e com névoa fechada, com pedaços do nosso mundo largados. Lá os Nocturnes são mais (12), mais rápidos (4,9), procuram o jogador e luz não os afasta: só abrigo. A volta é uma luz branca que zumbe em algum lugar da névoa. Pego de novo lá, o jogador acorda no começo do seu mundo. É refeito a cada visita. |
| Save | HPBW versão 6: o mesmo formato, com bits novos para as ruínas, para "salvo no outro lado" e para os relatos encontrados (versão 5), e depois do inventário a fome, o calor, a vida e o que foi pego do chão (versão 6). |
| Pedra-sombra | Bloco novo, de lá. Pode ser quebrado e usado para construir (um abrigo, por exemplo). |
| Inventário | Fica com o jogador quando é arrastado. |
| Itens | Além dos blocos: graveto, pedra lascada, faca, serra, machado, picareta, frutinhas, carne crua, assada e frita, banha, couro, lã e agasalho. Ficam no inventário (bolinhas, não quadrados); só blocos e comida vão ao hotbar. |
| Ferramentas | Servem carregadas, sem gastar. Sem picareta, pedra e o que é feito dela não saem (o app diz "Precisa de: Picareta"); com ela, dois golpes. A pedra-sombra cede à mão, devagar (6 golpes), para que um abrigo do outro lado seja sempre possível. Madeira leva 5 golpes à mão e 1 com machado. Uma barrinha sob a mira mostra o quanto falta. |
| Chão | Por dia, de 24 a 64 gravetos (perto das árvores) e pedras lascadas, em lugares novos a cada dia; 3 gravetos e 3 pedras sempre perto do começo. A faca e a serra ficam no chão de duas casas, uma vez só. Folhas soltam às vezes graveto (35%) ou frutinhas (17%); pedra, pedra lascada (25%). |
| Bancadas | Forno, fogão e fogueira: a receita confere se há uma a até 3 blocos. A fogueira ilumina (e espanta os Nocturnes fora da névoa, como a lâmpada). Mais da metade das casas em ruína tem um fogão. |
| Caça | 3 golpes (2 com faca ou machado): 2 carnes cruas; com faca, também banha e couro ou lã, conforme a criatura. Ela volta de dia. |
| Fome | Cheia, dura 18 minutos (pouco mais que um dia); com frio, esvazia mais rápido. Vazia, tira 1 de vida a cada 8 s. A vida só volta com a barriga pelo menos meio cheia. Frutinha +1, carne crua +1,5, assada +4, frita +6 (de 10). |
| Frio | De noite fora de abrigo (e mais na névoa) esfria; na água e do outro lado também. De dia, no abrigo, no santuário à noite ou perto de uma bancada (fogo) esquenta. Gelado, tira 1 de vida a cada 10 s. O agasalho segura 65% da perda. |
| Desmaio | Por fome ou frio, como por queda: acorda no começo, com um pouco de comida e calor. |

## Terceira rodada, como foi contada

> Opa man, pq tem um portal do Nether ali kkkkkk?
> Pq dentro da névoa a lanterna não espanta as criaturas? Ele é bem densa e a
> luz não penetra direito certo? Mas se tiver só um quarto fechado, isso não
> seria um abrigo já?
> Caramba essa parte da cidade ficou top.
> Opa man, inclusive, o farol não só sela os portais, mas permite que uma
> certa área se limpe das névoas, e que possa ter uma área iluminada e livre
> que eles nunca podem entrar.

## Decisões da terceira rodada

- **Portal:** a primeira versão (moldura retangular escura com luz roxa)
  lembrava demais o portal de outro jogo e saiu. Agora não tem moldura: é um
  rasgo alto e irregular no ar, preto por dentro, com rachaduras claras nas
  bordas, pedras arrancadas girando em volta e névoa saindo.
- **Lâmpada na névoa:** a névoa é densa e a luz não atravessa, então lá
  dentro a lâmpada não espanta as criaturas.
- **Abrigo:** basta um cômodo fechado. A lâmpada dentro é só para enxergar.
- **Farol aceso:** além de selar a fenda, abre um santuário de 40 blocos em
  volta: sem névoa, iluminado à noite (uma luz fria, mais fraca que o dia), e
  as criaturas param na borda olhando, sem nunca entrar. Um feixe de luz sobe
  do farol para que o santuário seja visto de longe.

## Quarta rodada, como foi contada

> Cara, a 1 com a 2 fica simplesmente genial. Mas aí a fase como ce falou cria
> 2 cenários ambíguos, um é voltar pro mundo e livrar apenas os humanos,
> fodasse o resto de seres de outra dimensão, e a segunda é entender e tentar
> ajudar, mas oferenda eu não sei man, talvez esse seja o lance, não é uma
> oferenda, eles querem ajuda de alguma forma, por mais aterrorizantes que
> sejam suas aparências, eles tbm são indivíduos que tbm estão completamente
> desesperados e perdidos, mas oq seria essa coisa? Talvez um humano? Como um
> cientista louco que descobriu a maneira de abrir portais e agora escraviza e
> faz estudos com elas? Pode ficar muito mandado assim ,então tem várias
> hipóteses pra nois pensar certinho.
> Gostei da função do santuário,sim man kkkkk, poderia até meio q ter uma
> missão entre aspas, tipo não literal, mas se vc recupera a mãe do Caio te da
> até uma emoção no coração ver os dois juntos ali chorando depois de se
> reencontrar não tem recompensa de item, é mais recompensa de eu juntei essa
> família, podia até ter conquistas, tipo, retorno ao lar.
> (...) Caso o jogo cresça muito, os personagens aleatórios que surgem podem
> ter vozes aleatórias, tipo, vamos supor que eu chegue a uma base de dados de
> um milhão de vozes, pra todo mundo ter sua vez, daria pra rodar determinado
> número de vozes desse banco escolhido aleatoriamente por seed, ou até ter
> baixado na máquina de jeito compactado centenas ou milhares de vozes
> diferentes, e aí elas podem ser extraídas e usadas conforme a necessidade.

## Decisões da quarta rodada

- **Por que vieram (hipóteses 1 e 2 juntas):** o mundo delas está morrendo, e
  elas também estão desesperadas e perdidas. Não é oferenda: elas querem
  **ajuda**, de algum jeito que ainda não sabemos. São indivíduos, por mais
  aterrorizante que seja a aparência delas.
- **O que é "essa coisa" que precisa de ajuda:** em aberto. Hipóteses:
  - algo do mundo delas (um núcleo, um ser, a própria névoa) que só se mantém
    com seres vivos por perto, e os levados ficam na "fila" sem morrer;
  - um humano: alguém que abriu os portais (um cientista, talvez o construtor
    das torres) e que escraviza e estuda as criaturas, que arrastam gente
    para... pedir socorro? Cuidado para não ficar óbvio demais;
  - outras, a pensar.
- **Dois finais ambíguos:**
  1. **Voltar e salvar só os humanos:** fechar tudo e deixar o outro lado ao
     próprio destino.
  2. **Entender e tentar ajudar:** descobrir o que elas querem e o que
     está por trás.
- **Sobreviventes no santuário:** quando um farol é aceso, sobreviventes
  escondidos chegam e moram ali. Cada um com personalidade e crença
  próprias: pedem coisas, trocam, contam boatos. Gente dos relatos pode
  estar viva.
- **"Missões" que não são missões:** sem recompensa de item. A recompensa é
  o momento, como trazer a mãe do Caio de volta e ver os dois se
  reencontrando. Com **conquistas**, por exemplo "Retorno ao lar".
- **Vozes:** o autor grava o protagonista e alguns personagens; se aparecer
  um dublador, melhor. Os áudios são gravados antes e tocados pelo app
  (OGG/Opus, leve no M12). Roteiros em [voice-scripts.md](voice-scripts.md).
- **Vozes aleatórias (se o jogo crescer):** personagens que surgem ao acaso
  escolhem uma voz de um banco, pela semente. Um milhão de gravações não cabe
  no celular, mas há dois caminhos que cabem: poucas vozes-base com variações
  (tom, velocidade, timbre) feitas no próprio jogo; ou uma voz sintética com
  muitos "falantes", em que cada voz é um vetor de poucos KB, com um modelo
  de dezenas de MB baixado à parte.

## Quinta rodada: decisões

- **Banco de vozes:** não é um milhão no celular. Na instalação vão algumas
  centenas ou milhares de vozes compactadas (ou dezenas, se pesar demais), e
  cada personagem que surge ao acaso pega uma delas pela semente.
- **O cientista (final "ajudar"):** quem escolhe ajudar acha uma passagem
  secreta até ele. Na cutscene, um monstro gigante, forte como um brutamontes,
  de jaleco, toca piano de costas para o jogador; ele se vira com um sorriso
  macabro. Os experimentos eram para transformar a si mesmo numa daquelas
  criaturas. Ele é a causa do sofrimento delas.
- **Luta contra o chefe:** estratégia, conversa e muita sobrevivência. Ele se
  vence com uma **armadilha de luz**.
- **A luz e as criaturas:** elas são invencíveis a tudo, menos à luz do nosso
  plano, que reage quimicamente com o corpo delas. Não queima: luz comum as
  afasta e as desfaz em fumaça (o que o jogo já faz), e só uma armadilha de
  luz concentrada as **petrifica**. Em aberto: só paralisa, ou mata.
- **Depois do chefe:** petrificado e selado, o lugar vira uma exposição de
  arte para as criaturas verem enquanto estão na luz. Armadilha de luz é o
  único jeito de petrificar, então não é algo que o jogador repita no dia a
  dia; no máximo, para fazer uma escultura.
- **Crianças:** não serão dubladas por adulto forçando a voz.
- **Fabricação mais realista:**
  - 1 madeira vira 1 tábua (é a mesma madeira, descascada). **Já no jogo.**
  - Gravetos caem das árvores e ficam pelo chão, para fazer ferramentas.
  - Bancadas: algo que esquente materiais (vidro sai daí, não do
    inventário), fogão nas casas para esquentar comida, e outras. Fabricar
    passa a depender de onde se está.

## Sexta rodada: decisões

### Ordem do trabalho

1. **Sobrevivência viva e dinâmica.** Dá para crescer muito. Duas regras:
   nada de falta de criatividade e nada de cópia direta. O mundo precisa ter
   gosto de vida: quando der, a atmosfera fica mais leve, para não pesar só
   dificuldade. O dia leve e a noite pesada fazem contraste e equilíbrio.
2. **Sobreviventes nos santuários.** O autor: "meu coração se enche de
   determinação só de escutar isso".
3. **O cientista e o laboratório** (abaixo).
4. **Vozes:** o autor ajeita as gravações depois.
5. **Acabamento:** animações e modelos em outra conta, em partes.
6. **Desempenho:** o autor testa no Galaxy M12 a cada rodada.

Planejamento aqui; o trabalho pesado pode ir para outra conta.

### Os Nocturnes são um povo

Chamar de "monstro" é demais: eles são um povo. Apelido de trabalho: **Nocs**.

### O covil: uma dimensão portátil

- O covil é **inacessível** até o jogador cumprir todos os requisitos do
  segundo final (ajudar). Aí vem a cutscene: um **portal portátil** surge do
  nada e leva para dentro.
- É uma **dimensão portátil** que o cientista aprendeu a fazer, entre as
  várias criações dele. Ele abriu a passagem **de propósito**: ele observa os
  passos do jogador. Por isso a armadilha de luz vai beirar o impossível.
- Por dentro é um **laboratório claro**, cheio de experimentos.

### Por que a luz não derruba o cientista de cara

- Ele é **meio humano, meio Noc**: aguenta muito mais luz que um Noc puro.
  Os Nocs puros do laboratório sofrem com essa luz; ele não.
- Como cientista, ele fez experimentos e sabe o quão forte a luz precisa ser
  para petrificar alguém na forma dele.
- **A pista:** entre os experimentos há um humano que ele transformou em
  meio Noc antes de si mesmo, preso e sofrendo com uma luz forte. Se ele
  sofre com aquela quantidade de luz, sendo quase a mesma coisa que o
  cientista, então o cientista também é afetado.
- **A luta:** uma corrida, lutando pela sobrevivência enquanto se causa dano
  no cientista, explorando a instalação até achar o jeito de vencê-lo com
  aquela luz.

### Um jogador ou vários

- A cutscene, a cena e o laboratório: a ideia é **um jogador só**, mesmo com
  o multiplayer.
- Em estudo: deixar vários jogadores com um teto (por exemplo, 10 ao mesmo
  tempo) e o cientista ficar mais difícil conforme o número na sala,
  soltando experimentos e usando certas criações de acordo com ele.

### Os dois finais

- **Ajudar (o do museu):** o cientista é petrificado e o laboratório vira a
  exposição de arte para os Nocs verem na luz.
- **Salvar só a humanidade:** os experimentos com os Nocs seguem, infinitos e
  eternos, e a humanidade fica em paz. Mas até quando? Até o cientista não
  querer mais os "brinquedos" de sempre.
- **Paralisar ou matar:** a luz paralisa, mas não para sempre. Uma
  **solução química** específica acorda o petrificado; voltar ao escuro não
  basta. Essa solução pode estar no próprio laboratório do cientista.

## Sétima rodada: depois do chefe

- **A solução química é loot do covil.** O experimento estava nas anotações
  do cientista: a curiosidade o levava a testar os limites da sanidade e do
  que ele podia fazer.
- **De laboratório a museu:** o covil vira uma atração turística, escurecido
  com uma luz que não afeta os Nocs.
- **Vidros especiais**, inventados pelo cientista ao estudar os Nocs e a
  dimensão deles: não quebram. Por isso a luta contra o chefe não se repete,
  e o jogo entra em **modo livre**.
- **O que fica na sala do chefe:** uma ou algumas coisas foram mantidas. Uma
  delas é a produção da substância de despetrificação, que o jogador pode
  coletar e usar nos Nocs petrificados.

## Oitava rodada, como foi contada

> Boas decisões, pras perguntas, sim fome entra, se não seria em vão a
> comida. Sim frio entra, calor tbm entraria, mas não faz muito sentido pq não
> tem nada pro calor. Ferramentas não só agilizam, elas vão ser oq torna
> possível certas coisas, achou uma faca ou criou uma improvisada, nesse caso
> da pra descascar a madeira. Serra encontrada, dá pra dividir em 2 tábuas
> menores ou mais dependendo se a tábua for um ingrediente pra craft, pode ser
> até uns 8 ou 10 nesse caso. E existem várias outras coisas, a sobrevivência
> tem que ficar dinâmica e legal.

## Oitava rodada: decisões (a sobrevivência, épico 1)

- **Fome entra.** Sem ela a comida não teria sentido. A barriga esvazia aos
  poucos (cheia dura um pouco mais que um dia do jogo); vazia, a vida cai
  devagar até o desmaio. A vida só volta com a barriga pelo menos meio cheia.
- **Frio entra; calor não**, porque não há nada que esquente demais. À noite,
  na água, na névoa e do outro lado o corpo esfria; de dia, num abrigo ou
  perto de fogo, esquenta. Gelado, a vida cai e a fome aperta. Um agasalho de
  couro e lã segura o calor.
- **Ferramentas tornam coisas possíveis**, não só aceleram:
  - **Faca** (achada numa casa ou improvisada com graveto e pedra lascada):
    descasca a madeira (1 madeira, 1 tábua) e tira couro, lã e banha da caça.
  - **Serra** (só achada, nas ruínas): corta 1 madeira em 2 tábuas e 1 tábua
    em 8 gravetos, quando a tábua vira ingrediente.
  - **Picareta improvisada:** sem ela a pedra não sai. Com ela também se
    lavra a pedra em pedra lavrada.
  - **Machado improvisado:** derruba madeira com um golpe; sem ele, a mão
    leva vários.
- **Gravetos e pedras lascadas** ficam pelo chão (gravetos perto das árvores)
  e mudam de lugar a cada dia. As folhas às vezes soltam gravetos e frutinhas.
  Quebrar pedra com a picareta às vezes solta uma pedra lascada.
- **Bancadas que dependem do lugar:** a fabricação confere o que está perto
  (até 3 blocos).
  - **Forno:** areia vira vidro, terra e areia viram tijolo, areia e pedra
    viram reboco. O vidro deixa de sair do inventário.
  - **Fogueira:** assa a carne, ilumina e aquece.
  - **Fogão:** frita a carne na banha. Algumas casas em ruína ainda têm um.
- **Criaturas do amanhecer:** dá para caçá-las (alguns golpes). Dão carne;
  com uma faca, também banha e couro ou lã, conforme a criatura. Voltam de dia,
  como já faziam depois de levadas. Criar (farm) fica para depois.
- **Comida:** frutinhas e carne crua matam pouco a fome; carne assada mata
  mais, e a frita na banha mais ainda. Come-se com Colocar, com a comida
  escolhida no hotbar.
- **O começo:** perto do início sempre há gravetos e pedras lascadas, para a
  primeira faca e a primeira picareta.
- **Save:** HPBW versão 6, com fome, calor, vida, as ferramentas já achadas e
  os gravetos e pedras pegos no dia. Mundos antigos abrem de barriga cheia e
  aquecidos.

## Nona rodada: arte e áudio com o Adobe Firefly

O autor quer usar o Adobe Firefly para a arte e o áudio do jogo. A leitura
dos termos (gerais, de 3/10/2025; de IA generativa, de 23/4/2026; leitura do
texto, não parecer de advogado):

- **Pode.** O que o Firefly gera é conteúdo de quem gerou, não arquivo da
  Adobe (termos de IA, §1), e não há limite de uso comercial. Pode ir para o
  repositório aberto e para o APK.
- **Não pode:** Adobe Stock, templates, Substance 3D Assets, exemplos e
  Adobe Fonts. A licença deles é pessoal e proíbe redistribuir (termos
  gerais, §3.5 e §3.6).
- **Só modelos da Adobe.** Modelo marcado como de outra empresa segue os
  termos dela e pede 18 anos ou mais (§7). Cuidado maior nas vozes.
- **Manter os Content Credentials** e não esconder que é IA (§3.1).
- **Zona cinzenta:** os termos proíbem usar o que sai do Firefly para
  treinar IA e "permitir que terceiros" o façam (§3.3), e uma licença aberta
  não pode proibir isso. Saída: arte e áudio ficam em `blocks/assets/` sob
  CC BY-SA 4.0 (compatível com a GPL-3.0), com origem de cada arquivo
  registrada e um aviso pedindo que não se treine IA com os do Firefly.
- **Vozes:** as dublagens de [voice-scripts.md](voice-scripts.md) podem ser
  gravadas pelo autor ou geradas. Nada de imitar voz de gente real.

Regras, pastas e formatos em [../assets/README.md](../assets/README.md).

## O que entra (primeira rodada)

| Parte | Já existe no jogo? | Observação |
|---|---|---|
| Ciclo de dia e noite | Sim | Base das duas frentes. |
| Criaturas pacíficas | Sim, só andam | Falta poder caçar e criar. Pede algo para fazer com o que se ganha (comida, couro, lã). |
| Nocturnes noturnos | Sim, como "criaturas noturnas" | Mudam: mordem e morrem com 3 golpes hoje. Passam a ser quase impossíveis de matar, perseguem e arrastam. |
| Névoa ligada aos Nocturnes | Em parte | Já fica violeta e solta noturnas perto dos faróis apagados. Falta avançar à noite. |
| Faróis | Sim | Ganham sentido na história: selam as fendas. |
| Fendas ao lado dos faróis | Não | Um bloco ou efeito novo junto ao farol. Barato de desenhar. |
| Abrigos | Não | Conferência de cômodo fechado e iluminado. Cabe no M12 se for feita poucas vezes por segundo. |
| Ruínas de vilas e cidades | Não | Estruturas no gerador do mundo, a partir da semente (o save não muda). O mundo é pequeno, então "cidade" é um grupo de casas em ruína. |
| Relatos de sobreviventes | Não | Item ou bloco nas ruínas que, ao ser usado, abre o texto na tela. Os textos ficam em `strings.xml`, em inglês e português. |
| Ser arrastado | Não | Substitui o desmaio quando o culpado é um Nocturne. |
| Dimensão dos Nocturnes | Não | A maior parte do trabalho. É um segundo mundo, que pede HPBW versão 5. |

## O que sai

| Parte | Por quê |
|---|---|
| Matar os Nocturnes para farmar | O autor quer terror e fuga, não combate. Golpear pode só empurrar ou atordoar. |
| Lâmpada comum dissipar a névoa de vez | Já tinha ficado de fora. Na história, só o farol sela uma fenda; a lâmpada só segura a névoa por perto. |
| Placar de criaturas salvas por noite | Da ideia 3; fica para depois, se fizer falta. |

## Em aberto

- O covil é a dimensão portátil do cientista (sexta rodada). E a "fila" dos
  levados, que o Lucas viu: fica lá ou do outro lado?
- Qual dos dois finais é o "verdadeiro", ou se os dois valem.
- O laboratório: um jogador só, ou até uns 10 com o cientista se adaptando?
- Despetrificar um Noc no modo livre: o que acontece com ele depois (fica
  grato, vai embora, volta para casa)? E o cientista: pode ser acordado?
- A revelação de que a névoa e os Nocturnes são coisas diferentes: em qual
  relato, ou num relato novo do outro lado?
- Nome oficial dos Nocturnes; o apelido de trabalho é "Nocs". No jogo eles
  seguem sem nome.
- Criar as criaturas do amanhecer (farm): cercado, filhotes, ração.
- Mais roupas e utensílios além do agasalho (cantil, mochila, tocha...).
- Nome do jogador, quando houver multiplayer.
- Animações e modelos (outra conta, com IA): hoje tudo é caixa; os pontos de
  troca são os Nocturnes, as criaturas, os relatos, as fendas e o portal.
- Voz de criança gerada por IA: vale, ou essas falas ficam sem voz?
- O jogo tocar arquivos de áudio (hoje os sons são feitos no código) e ler
  texturas de arquivo, quando chegarem os primeiros em `blocks/assets/`.
