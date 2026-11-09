# HyPocket Blocks: ideias de objetivo

Registro das ideias de objetivo propostas para a etapa 6 do épico HPK-10
(tarefa HPK-16), para consulta e comparação quando a história principal do
jogo for escrita. O texto das três propostas está como foi apresentado, sem
edição.

## As três propostas originais

As três usam o que o jogo já tem: receitas (etapa 3), lâmpadas (etapa 1) e
criaturas noturnas (etapa 4).

### 1. Faróis (a recomendada)

5 faróis apagados pelo mundo. Acender cada um pede materiais fabricados; um
farol aceso afasta as noturnas da região e aponta o próximo numa bússola do
HUD. Acender todos encerra o mundo com uma tela final e o tempo gasto. É a
ideia do Jira e reaproveita quase tudo.

### 2. Névoa

Manchas de névoa cobrem partes do mundo e é delas que saem as noturnas. Uma
lâmpada colocada dentro da névoa a dissipa de vez. Limpar toda a névoa vence
o mundo; o HUD mostra a porcentagem limpa. Mais exploração e construção
espalhada.

### 3. Abrigos

A cada noite, as criaturas pacíficas precisam de abrigo: um cômodo fechado e
iluminado que o jogador constrói. Salvar um número de criaturas em 5 noites
vence; quem fica fora some. Foco em construir, com um placar por noite.

## A escolha

Escolhida a ideia 1 (faróis), com elementos da 2 (névoa). A ideia 3 (abrigos)
fica para ser considerada na história principal, em parte.

## O que entrou no jogo (commit ef1bdcf)

- **Faróis:** 5 faróis apagados no alto de pilares de tijolo de pedra, num
  anel em volta do ponto de partida (raio de cerca de 36% do lado do mundo).
  A posição vem da semente do mundo, então o save não precisa guardá-la, e
  mundos salvos antes dos faróis os ganham ao abrir.
- **Acender:** apontar para o farol e usar Colocar, com 2 lâmpadas e 3 vidros
  (de graça no modo criativo). Faróis não podem ser quebrados.
- **Farol aceso:** brilha, ilumina os blocos em volta como uma lâmpada, limpa
  a névoa da região e não deixa criaturas noturnas a menos de 40 blocos.
- **Névoa (da ideia 2):** perto de um farol apagado (forte até 22 blocos,
  some aos 40) o nevoeiro fecha e fica violeta, e as noturnas saem até de
  dia. Diferente da ideia 2 original: a névoa é ligada aos faróis, não a
  manchas soltas, e lâmpadas comuns não a dissipam.
- **HUD:** bússola até o farol apagado mais próximo ("Faróis 2/5 · 46 m") e o
  custo quando a mira está num farol.
- **Final:** acender todos mostra uma tela de conclusão com o tempo jogado; a
  lista de mundos mostra "Terminado em …".
- **Save:** HPBW versão 4 guarda o tempo jogado, se os faróis foram colocados
  e quando o último foi aceso.
- **Benchmark:** o mundo do benchmark não tem faróis, para os números
  continuarem comparáveis.

## O que não entrou

- Da ideia 1: "revela no mapa onde fica o próximo" virou a bússola; não há
  mapa.
- Da ideia 2: as manchas de névoa independentes, a lâmpada que dissipa névoa e
  a porcentagem limpa no HUD.
- Da ideia 3: nada ainda.
