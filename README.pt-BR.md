[English](README.md) | **Português (Brasil)**

# HyPocket Launcher

Um launcher de código aberto para jogar Hytale no Android, com foco em
celulares mais simples (pouca RAM, GPU fraca).

> [!IMPORTANT]
> **Projeto não oficial.** O HyPocket não tem ligação com a Hypixel Studios,
> nem é aprovado ou patrocinado por ela. Hytale é uma marca de seu respectivo
> dono.

## Situação

O HyPocket está no começo do desenvolvimento e **ainda não dá para jogar**.
Ainda não há versões publicadas.

## Requisitos

- **Uma conta original e uma cópia legítima do jogo.** Você entra com a sua
  própria conta oficial.
- Um celular Android com processador ARM64.

O HyPocket **não** inclui nem distribui nenhum arquivo do jogo. O launcher
baixa os arquivos usando a sua própria conta. Não existe modo offline nem
pirata, e nunca vai existir.

## Como funciona

O cliente do Hytale é feito para PC (processadores x86-64 e APIs gráficas de
computador). Para rodar no celular, o HyPocket precisa:

1. Traduzir as instruções x86-64 para ARM (com um tradutor como Box64 ou FEX).
2. Traduzir as chamadas gráficas de PC para OpenGL ES ou Vulkan (com uma
   camada como Zink ou gl4es).
3. Manter o uso de memória e de GPU baixo o bastante para celulares simples.

O app em si (login, downloads, configurações) é escrito em Kotlin. O motor
(camadas de tradução) é escrito em C/C++ com o Android NDK.

## Planejamento

- [x] Regras do projeto e guia de contribuição
- [ ] Esqueleto do app Android: login, download do jogo, configurações
- [ ] Pesquisar e integrar as camadas de tradução de CPU e gráficos
- [ ] Otimização para aparelhos simples
- [ ] Primeira versão pública

## Downloads

Os APKs oficiais serão publicados **somente** na
[página de releases](https://github.com/HyPocketApp/hypocket-launcher/releases)
deste repositório. Não instale o HyPocket de outras fontes.

## Repositório oficial e política do nome

[HyPocketApp/hypocket-launcher](https://github.com/HyPocketApp/hypocket-launcher)
é o **único** repositório oficial do HyPocket Launcher.

Você pode fazer um fork deste projeto nos termos da GPL-3.0, mas **forks não
podem usar o nome "HyPocket"** nem se apresentar como o projeto oficial.
Escolha outro nome para o seu fork.

## Como contribuir

Contribuições são bem-vindas. Leia o [CONTRIBUTING.md](CONTRIBUTING.md) (em
inglês) para saber como compilar e testar o projeto, o padrão de commits e as
regras do projeto.

## Licença

O HyPocket Launcher é licenciado sob a
[GNU General Public License v3.0](LICENSE). Qualquer fork ou modificação
distribuída também precisa ser publicada como código aberto, sob a mesma
licença.
