// The script of the introduction, written by the game's author. Each page
// fits in the dialog box (main.c wraps the text): two lines when someone
// speaks, three for narration.
#include "story.h"

#define N SPEAKER_NONE
#define H SPEAKER_HERO
#define U SPEAKER_UNKNOWN
#define W SPEAKER_WOMAN
#define C1 SPEAKER_CREATURE_1
#define C2 SPEAKER_CREATURE_2
#define C3 SPEAKER_CREATURE_3

// Scene 01: an ordinary morning, at home.
static const StoryPage intro[] = {
    { N, "The sun was starting to light up the treetops.",
         "O sol começava a iluminar as copas das árvores." },
    { N, "In the middle of the forest, a small wooden house stood wrapped in silence.",
         "No meio da floresta, uma pequena casa de madeira permanecia cercada pelo silêncio." },
    { N, "The door opens.",
         "A porta se abre." },
    { H, "Hm... Looks like it's going to be sunny today.",
         "Hm... Parece que vai fazer sol hoje." },
    { N, "He looks at the sky, then at the axe leaning by the entrance.",
         "Ele olha para o céu e depois para o machado encostado próximo à entrada." },
    { H, "Right. Time to work.",
         "Certo. Vamos trabalhar." },
    { N, "The lumberjack takes his axe, tidies a few things and closes the door.",
         "O lenhador pega seu machado, ajeita algumas coisas e fecha a porta de casa." },
    { N, "The nearest village was a good walk from there.",
         "A vila mais próxima ficava a uma boa caminhada dali." },
    { N, "He visited it every now and then to buy supplies and take care of a few things.",
         "Ele costumava visitá-la de vez em quando para comprar mantimentos e resolver algumas coisas." },
    { H, "I think I do need to stop by the village.",
         "Acho que estou precisando passar na vila mesmo." },
    { N, "He starts walking down the trail between the trees.",
         "Ele começa a caminhar pela trilha entre as árvores." },
    { N, "The forest was calm. The sound of birds and swaying leaves followed his steps.",
         "A floresta estava tranquila. O som dos pássaros e das folhas balançando acompanhava seus passos." },
};

// Scene 02: the cry, on entering the woods.
static const StoryPage cry[] = {
    { N, "After a few minutes of walking, a cry breaks the silence of the forest.",
         "Depois de alguns minutos caminhando, um grito rompe o silêncio da floresta." },
    { U, "AAAAAH! HELP!",
         "AAAAAH! SOCORRO!" },
    { N, "The lumberjack stops at once. The cry seemed to come from somewhere close, a little off the trail.",
         "O lenhador para imediatamente. O grito parecia ter vindo de algum lugar próximo, um pouco afastado da trilha." },
    { H, "Hm? What was that?",
         "Hm? O que foi isso?" },
    { N, "He looks toward where the sound came from.",
         "Ele olha na direção de onde veio o som." },
    { H, "Sounds like someone needs help.",
         "Parece que alguém está precisando de ajuda." },
    { N, "Without wasting time, he leaves the trail and heads toward the cry.",
         "Sem perder tempo, ele sai da trilha e segue em direção ao grito." },
};

// Scene 03: the creatures, on reaching the spot. The battle follows.
static const StoryPage creatures[] = {
    { N, "Pushing through some trees, the lumberjack finally reaches a small clearing.",
         "Ao atravessar algumas árvores, o lenhador finalmente chega a uma pequena clareira." },
    { N, "In the middle of it, a woman with silver hair lies on the ground. Three creatures made of fire surround her.",
         "No centro dela, uma mulher de cabelos prateados está caída no chão. Três criaturas feitas de fogo a cercam." },
    { C1, "It's no use resisting! Hand over what's left of your power!",
          "Não adianta resistir! Entregue o que resta do seu poder!" },
    { W, "I already said I won't hand over anything!",
         "Eu já disse que não vou entregar nada!" },
    { C2, "Then we'll have to tear it out by force.",
          "Então teremos que arrancar à força." },
    { N, "The lumberjack watches for a few seconds, gripping his axe.",
         "O lenhador observa a situação por alguns segundos, segurando o machado." },
    { H, "Hey! What are you doing?",
         "Ei! O que vocês estão fazendo?" },
    { C1, "Huh? Who are you?",
          "Hã? Quem é você?" },
    { H, "I should be asking that. Three against one? Doesn't seem very fair.",
         "Eu que deveria perguntar. Três contra uma? Não parece muito justo." },
    { C2, "Get out of here, human. This is none of your business.",
          "Saia daqui, humano. Isso não é da sua conta." },
    { H, "Well, now that I've heard someone crying for help, it kind of is.",
         "Bom, agora que ouvi alguém pedindo socorro, meio que virou." },
    { C3, "Another fool who wants to die...",
          "Mais um idiota querendo morrer..." },
    { W, "Wait! You shouldn't get involved! They're dangerous!",
         "Espere! Você não deveria se envolver! Eles são perigosos!" },
    { H, "Yeah, I noticed.",
         "É, percebi." },
    { N, "He grips the axe with both hands.",
         "Ele segura o machado com as duas mãos." },
    { H, "But I think I can handle this.",
         "Mas acho que consigo lidar com isso." },
    { C1, "Finish him!",
          "Acabem com ele!" },
};

// Scene 04: after the battle. She joins him at the end.
static const StoryPage after[] = {
    { N, "The fire creatures fall back, realizing they could not win.",
         "As criaturas de fogo recuam, percebendo que não conseguiriam vencer." },
    { N, "One of them looks at the lumberjack before vanishing among the trees.",
         "Uma delas olha para o lenhador antes de desaparecer entre as árvores." },
    { C1, "This isn't over... The goddess won't escape us!",
          "Isso não acabou... A deusa não vai escapar de nós!" },
    { N, "Silence returns to the clearing. The lumberjack rests the axe on his shoulder and looks at the fallen woman.",
         "O silêncio retorna à clareira. O lenhador apoia o machado no ombro e olha para a mulher caída." },
    { H, "Are you alright?",
         "Você está bem?" },
    { W, "I think so... Thank you for helping me.",
         "Acho que sim... Obrigada por me ajudar." },
    { H, "It was nothing. But... what were those things?",
         "Não foi nada. Mas... o que eram aquelas coisas?" },
    // The race has no name yet (see docs/LORE.md); "Embers" stands in for it.
    { W, "They are of the race of the Embers. They've been after me for some time.",
         "Eles são da raça das Fagulhas. Estão atrás de mim há algum tempo." },
    { H, "And why is that?",
         "E por quê?" },
    { W, "Because they want to steal my powers.",
         "Porque querem roubar meus poderes." },
    { H, "Your powers?",
         "Seus poderes?" },
    { N, "The woman hesitates for a moment before going on.",
         "A mulher hesita por um instante antes de continuar." },
    { W, "I suppose I owe you an explanation.",
         "Acho que devo uma explicação." },
    { W, "My name doesn't matter much right now, but... I am the Goddess of the Moon.",
         "Meu nome não importa muito agora, mas... sou a Deusa da Lua." },
    { H, "...",
         "..." },
    { H, "The Goddess of the Moon?",
         "A Deusa da Lua?" },
    { W, "Yes. Or at least it was supposed to be more impressive...",
         "Sim. Ou pelo menos era para ser mais impressionante..." },
    { N, "She looks at her own hands.",
         "Ela olha para as próprias mãos." },
    { W, "I lost a good part of my powers.",
         "Perdi boa parte dos meus poderes." },
    { W, "And those creatures are trying to use this chance to take them for good.",
         "E aquelas criaturas estão tentando aproveitar essa oportunidade para tomá-los de vez." },
    { H, "Right... I guess my trip to the village will have to wait.",
         "Certo... Acho que minha ida à vila vai ter que esperar." },
    { W, "You don't need to get any more involved. You've already done a lot for me.",
         "Você não precisa se envolver mais nisso. Já fez muito por mim." },
    { H, "After seeing all this, it's hard to just pretend nothing happened.",
         "Depois de ver tudo isso, acho difícil simplesmente fingir que nada aconteceu." },
    { N, "The woman looks at him for a few seconds, surprised by his answer.",
         "A mulher olha para ele por alguns segundos, surpresa com sua resposta." },
    { W, "You are quite a strange human...",
         "Você é um humano bastante estranho..." },
    { H, "I've been told.",
         "Já me disseram isso." },
};

#define COUNT(a) (int)(sizeof(a) / sizeof((a)[0]))

static const struct {
    const StoryPage *pages;
    int count;
} scenes[SCENE_COUNT] = {
    [SCENE_INTRO] = { intro, COUNT(intro) },
    [SCENE_WOODS] = { cry, COUNT(cry) },
    [SCENE_CREATURES] = { creatures, COUNT(creatures) },
    [SCENE_GODDESS] = { after, COUNT(after) },
};

int story_page_count(Scene scene)
{
    return (scene >= 0 && scene < SCENE_COUNT) ? scenes[scene].count : 0;
}

const StoryPage *story_page(Scene scene, int page)
{
    if (page < 0 || page >= story_page_count(scene)) return 0;
    return &scenes[scene].pages[page];
}

const char *story_speaker_name(Speaker who, bool portuguese)
{
    static const char *const names[SPEAKER_COUNT][2] = {
        [SPEAKER_NONE] = { "", "" },
        [SPEAKER_HERO] = { "Lumberjack", "Lenhador" },
        [SPEAKER_UNKNOWN] = { "???", "???" },
        [SPEAKER_WOMAN] = { "Woman", "Mulher" },
        [SPEAKER_CREATURE_1] = { "Fire Creature 1", "Criatura de Fogo 1" },
        [SPEAKER_CREATURE_2] = { "Fire Creature 2", "Criatura de Fogo 2" },
        [SPEAKER_CREATURE_3] = { "Fire Creature 3", "Criatura de Fogo 3" },
    };
    if (who < 0 || who >= SPEAKER_COUNT) return "";
    return names[who][portuguese ? 1 : 0];
}
