# Lore de Timberheart

Este arquivo guarda a história do jogo. O que está em **Definido** foi
escolhido pelo autor e já vale. **Em aberto** lista o que ainda falta
decidir; nada ali deve ir para o jogo sem a escolha do autor.

## Definido

### Roteiro da introdução

O roteiro completo, escrito pelo autor, está no jogo em `src/story.c`, fala
por fala, em português e inglês. Resumo:

1. **Uma manhã comum.** O lenhador sai de casa, pega o machado e decide
   passar na vila mais próxima, onde costuma comprar mantimentos.
2. **O grito.** Na trilha pela floresta, alguém grita por socorro, um pouco
   afastado do caminho. Ele sai da trilha para ajudar.
3. **Encontro com as criaturas.** Numa pequena clareira, uma mulher de
   cabelos prateados está caída, cercada por **três criaturas de fogo** que
   exigem o que resta do poder dela. O lenhador intervém, e começa a
   batalha.
4. **Depois da batalha.** As criaturas recuam ("Isso não acabou... A deusa
   não vai escapar de nós!"). A mulher conta que eles querem roubar os
   poderes dela e que ela é a **Deusa da Lua**, que perdeu boa parte dos
   poderes. O lenhador decide que a ida à vila vai ter que esperar.

Depois disso, a deusa **acompanha o lenhador** e **luta usando magia**.

### Personagens

| Personagem | O que sabemos |
|---|---|
| O lenhador | Protagonista. Mora numa casa de madeira no meio da floresta e usa um machado. Ia até a vila comprar mantimentos. Calmo e prestativo; "um humano bastante estranho", segundo a deusa. |
| A Deusa da Lua | Cabelos prateados, roupas brancas. Perdeu boa parte dos poderes. Diz que o nome dela "não importa muito agora". Acompanha o lenhador e luta com magia. |
| As criaturas de fogo | Uma raça que quer roubar o poder da deusa e a persegue há algum tempo. Na introdução aparecem três. |

## Como isso aparece no jogo

- **Cena 01** toca depois do título, na casa do lenhador (sem monstros).
- **Cena 02** toca ao entrar no Bosque da Serraria pela primeira vez.
- **A clareira** fica longe da entrada do bosque e fora da trilha: quem
  ouve o grito não vê a mulher; é preciso seguir a trilha e entrar por uma
  abertura entre as árvores. No mapa, ela está caída e as três criaturas de
  fogo estão em volta dela (acima, à esquerda e à direita).
- **Cena 03** toca quando o lenhador chega à entrada da clareira (a até 3
  quadrados das criaturas); a câmera mostra os dois acima da caixa de
  diálogo. A batalha contra as três começa logo depois, sem opção de fugir.
  Na batalha elas "recuam" em vez de cair, e somem as três do mapa.
- **Cena 04** toca depois da vitória, com ela ainda caída; quando a cena
  termina, ela se levanta e passa a seguir o lenhador e a lutar com ele (**Raio Lunar** e **Luz da Lua**, nomes
  provisórios).

Cada fala é uma tela; Enter ou toque avança, X pula a cena. O texto quebra
as linhas sozinho, e o jogo avisa (no registro, ao abrir) se alguma fala não
couber na caixa.

## Em aberto

- **O nome da raça de fogo.** O roteiro diz "Eles são da raça dos [nome]".
  Por enquanto o jogo usa **Fagulhas** (é o mesmo nome dos monstros na
  batalha). Para trocar: `src/story.c` (a fala) e `TXT_EMBER` em
  `src/text.c` (o nome na batalha).
- Os nomes do lenhador, da deusa e da vila.
- Como a deusa perdeu os poderes e para que as criaturas querem esse poder.
- O que acontece depois da introdução. Hoje os mapas 2 e 3 ainda são os da
  versão anterior (lobos, besouros e o chefe **Toco Ancião**), e a tela de
  título ainda fala em "acalmar o Toco Ancião".
