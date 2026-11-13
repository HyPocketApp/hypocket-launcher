# Lore de Timberheart

Este arquivo guarda a história do jogo. O que está em **Definido** foi
escolhido pelo autor e já vale. **Em aberto** lista o que ainda falta
decidir; nada ali deve ir para o jogo sem a escolha do autor.

## Definido

### O começo

O protagonista é um **lenhador** que vive na beira da floresta, numa casa
dele, e luta com um **machado**. Um dia ele decide ir até uma **vila
próxima**.

No caminho, ele encontra uma **mulher majestosa, de cabelos e roupas
brancos**, sendo atacada por **pequenos seres de fogo**. O lenhador a salva.

Depois, ela conta quem é:

- ela é a **Deusa da Lua**;
- ela **perdeu grande parte dos seus poderes**;
- os seres que a atacaram são de uma **raça que quer roubar o poder dela**.

Depois disso, a deusa **acompanha o lenhador** e **luta usando magia**.

### Personagens

| Personagem | O que sabemos |
|---|---|
| O lenhador | Protagonista. Mora na beira da floresta e usa um machado. Está indo até uma vila próxima. |
| A Deusa da Lua | Mulher majestosa de cabelos e roupas brancos. Perdeu grande parte dos poderes. Acompanha o lenhador e luta com magia. |
| Os seres de fogo | Pequenos, de uma raça que quer roubar o poder da deusa. No jogo, cada um se chama **Fagulha** (nome provisório). |

## Como isso aparece no jogo

- **Introdução** (2 telas, depois do título): o lenhador decide ir até a
  vila. O jogo começa na **casa dele**, sem monstros.
- **Bosque da Serraria:** ao entrar pela primeira vez, um grito ecoa. A
  deusa está perto da entrada, cercada por seres de fogo (`M` e `e` no mapa).
- **Luta contra as Fagulhas:** duas Fagulhas, sem opção de fugir. A deusa
  aparece atrás do lenhador durante a luta.
- **Cena da deusa** (5 telas, depois da vitória): ela agradece, diz que é a
  Deusa da Lua, que perdeu grande parte dos poderes, que aqueles seres
  querem roubar o poder dela, e pede para ir junto até a vila.
- **Depois:** ela segue o lenhador pelo mapa e luta nas batalhas com
  **Raio Lunar** e **Luz da Lua** (nomes provisórios).

Os textos ficam em `src/text.c` (`TXT_INTRO_*`, `TXT_GODDESS_*`), em
português e inglês. A ordem e quem fala cada tela ficam em `src/main.c`
(`intro_pages`, `goddess_pages`); o número de telas fica em
`scene_page_count()` em `src/game.c`, e os dois precisam bater.

## Em aberto

- Nomes: do lenhador, da deusa, da raça de fogo e da vila.
- Por que o lenhador vai até a vila.
- Como a deusa perdeu os poderes e para que a raça de fogo quer esse poder.
- Para onde vão depois da vila.
- Como a história se liga ao resto do jogo. Hoje os mapas 2 e 3 ainda são os
  da versão anterior (lobos, besouros e o chefe **Toco Ancião**), e a tela de
  título ainda fala em "acalmar o Toco Ancião". O chefe final pode passar a
  ser alguém da raça de fogo, por exemplo.
