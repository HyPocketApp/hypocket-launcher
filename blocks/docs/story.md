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

Segunda rodada: o autor aprovou as propostas (ver "Decisões da segunda
rodada") e pediu tudo de uma vez, um commit por parte, num APK só.

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

## O que entra

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

- Os faróis **selam** as fendas, ou só **seguram** a névoa enquanto estão
  acesos? (Proposta: selam.)
- Golpear um Nocturne faz alguma coisa? (Proposta: empurra e atordoa por
  alguns segundos, sem matar.)
- Ser arrastado leva sempre para a dimensão, ou só no fim de um tempo? E o
  que acontece com o inventário?
- O que fazer na dimensão: fugir de volta por um portal de saída, achar o
  covil e descobrir para que servem os seres arrastados (talvez resgatá-los)?
  Vai ter chefes?
- Quantos relatos, e qual a ordem deles? Eles contam a história aos poucos
  até a revelação de que a névoa e os Nocturnes são coisas diferentes?
- Nome final: "Nocturnes" fica?
- O jogador tem nome e passado?
- O que se ganha das criaturas do amanhecer, e para quê: fome, fabricação,
  ou os dois?
