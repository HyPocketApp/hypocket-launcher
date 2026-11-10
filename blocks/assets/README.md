# HyPocket Blocks: arte e áudio

Os arquivos de arte e som do jogo. Hoje o jogo desenha as texturas e faz os
sons no próprio código (`game/blocks.c`). O que chegar aqui vai substituindo
ou somando a isso, aos poucos.

- **Áudio:** os `.ogg` de `audio/` entram no APK sozinhos (só eles, não os
  `.gitkeep` nem estes textos) e o jogo os toca. Ler textura de arquivo
  ainda falta.

A decisão que criou esta pasta está em [../docs/story.md](../docs/story.md),
"Nona rodada".

## Regras

1. **Tudo original.** Nada pedido "no estilo de" outro jogo, filme ou
   artista. Nada de nome, marca, personagem ou voz de gente real.
2. **Todo arquivo tem origem registrada.** Uma linha em
   [SOURCES.md](SOURCES.md) no mesmo commit, dizendo quem fez, com o quê e
   quando. Arquivo sem linha não entra.
3. **Adobe Firefly pode.** O que ele gera é conteúdo de quem gerou (termos de
   IA generativa da Adobe, §1), não arquivo da Adobe. Mas:
   - **só modelos da própria Adobe.** Se a ferramenta mostrar um modelo
     marcado como de outra empresa ("Non-Adobe Model", §7), não use: valem os
     termos dessa empresa, e pede 18 anos ou mais;
   - **não apague os Content Credentials** nem esconda que foi feito com IA
     (§3.1). Converter para Ogg perde os metadados do original, então o
     link deles vai em SOURCES.md e no comentário `PROVENANCE` do `.ogg`;
   - **sem pedido com marca, obra ou pessoa real** (§2 e as diretrizes de
     uso). Isso inclui imitar a voz de alguém.
4. **Nada da biblioteca da Adobe.** Adobe Stock, modelos e templates do
   Express, Substance 3D Assets, arquivos de exemplo e de tutorial: a
   licença deles proíbe redistribuir e sublicenciar (termos gerais, §3.5 e
   §3.6), e o repositório é aberto. Fontes da Adobe Fonts também não.
5. **Crianças:** adulto não faz voz de criança (story.md). Voz de criança
   gerada por IA pode.

## Pastas e formatos

| Pasta | O quê | Formato |
|---|---|---|
| `art/textures/` | Texturas de blocos, itens e criaturas | PNG, 16×16 (o tamanho do jogo; 32×32 só se o jogo mudar), sem perfil de cor |
| `audio/voice/` | Falas de [voice-scripts.md](../docs/voice-scripts.md) | Ogg Vorbis mono, uns 48 kbps |
| `audio/sfx/` | Efeitos (passo, quebra, criaturas, zumbido) | Ogg Vorbis mono, até uns 2 s |
| `audio/ambient/` | Fundos em loop (vento, névoa) | Ogg Vorbis mono, loop sem corte |

- **Nomes:** em inglês, minúsculas e `_`. As falas usam o nome da coluna
  "Arquivo" do roteiro (`note2_caio_1.ogg`, `player_grab_2.ogg`).
- **Leve:** o alvo é o Galaxy M12. O original em WAV, o projeto do editor e
  as variações descartadas ficam com o autor; aqui só a versão final.

## Nomes que o jogo já usa

| Arquivo | O que faz |
|---|---|
| `audio/sfx/step.ogg`, `step2.ogg` | Passos (os dois se alternam) |
| `audio/sfx/break.ogg` | Quebrar um bloco |
| `audio/sfx/place.ogg` | Colocar um bloco, abrir um papel |
| `audio/sfx/jump.ogg` | Pulo |
| `audio/sfx/splash.ogg` | Cair na água |
| `audio/sfx/chirp.ogg` | Passarinho, de vez em quando |
| `audio/sfx/campfire.ogg` | Estalo da fogueira, em loop, mais alto quanto mais perto |
| `audio/ambient/wind.ogg` | Vento, em loop, o tempo todo |
| `audio/voice/note<N>_voz.ogg` | Lido quando o papel N é pego no mundo (os nomes de [voice-scripts.md](../docs/voice-scripts.md)) |

Arquivo que falta não quebra nada: o jogo usa o som feito no código, ou
fica sem aquele som. Os outros nomes do roteiro (celulares, protagonista)
ainda não tocam.

A lista do que gerar, com os pedidos prontos, está em
[../docs/sound-list.md](../docs/sound-list.md).

## Como mandar

Pelo celular, dá de dois jeitos:

- mandar os arquivos na conversa com a sessão do Claude, dizendo de onde
  vieram; a sessão converte, coloca na pasta certa e preenche SOURCES.md;
- ou, no site do GitHub, abrir a pasta na branch, "Add file" → "Upload
  files", e escrever a linha em SOURCES.md.

## Licença

O código do projeto é GPL-3.0. Os arquivos desta pasta estão sob a
**CC BY-SA 4.0** (Creative Commons Atribuição-CompartilhaIgual), que pode ser
usada junto com a GPL-3.0. Créditos em [SOURCES.md](SOURCES.md).

Aviso sobre os arquivos marcados como "Adobe Firefly" em SOURCES.md: quem os
gerou aceitou os termos da Adobe, que proíbem usar o que sai do Firefly para
criar, treinar ou testar modelos de IA (§3.3). Pedimos que ninguém use esses
arquivos para isso. Pode ser que esses arquivos nem tenham proteção de
direito autoral (§3.2). Este é um aviso, não uma condição a mais da licença.
É uma zona cinzenta conhecida, registrada em story.md.
