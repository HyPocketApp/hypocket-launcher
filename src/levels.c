// The areas of the world, written as text. One character is one 16x16 tile:
//
//   .  grass            #  deep forest       T  tree (chop it with the axe)
//   R  rock             ~  water             =  bridge
//   o  bush (cut it)    ,  flowers
//   c  chest (herbs)    A  chest (sharper axe)
//   f  campfire (rest; you wake up here after falling in battle)
//   H  house: a block of 4x3, with the door 'D' second from the left in
//      the bottom row (the picture is drawn for that shape)
//   D  its door (rest)  W  firewood          :  dirt path
//   M  the Moon Goddess, waiting for help    P  start (only the home has one)
//   1-9  path to another area: walking onto it leads to the area with the
//        same digit, next to that digit. Each digit is in exactly two areas.
//
// Monsters, which start a battle when touched:
//   s  one slime        g  two slimes        b  a beetle and a slime
//   v  one wolf         w  two wolves        B  the Elder Stump (boss)
//   e  a fire creature around the goddess: put three 'e' next to her 'M';
//      they stay put and are one battle against all three
//
// An area needs a path or a 'B', at most 64 rows and 96 columns.
// tests/test_game.c checks that everything can be reached (trees and bushes
// count as open, since the axe clears them) and that the paths pair up.
#include "game.h"

// The lumberjack's home: no monsters. The path east leads into the woods.
static const char *const home[] = {
    "################################",
    "##########TT####################",
    "#####TTT......T####TT###########",
    "####T....,,.........T#....######",
    "###T....HHHH........o.,,....####",
    "###.....HHHH...WW..........T.###",
    "##o..,..HDHH................,.##",
    "##.......:P.......TT......c...##",
    "##.....o.:::::::::.T...........#",
    "###...............:::::::::::::1",
    "###T.......,,..............o..##",
    "####T.....,,,,....TT..........##",
    "#####..o.........TTT.....TT..###",
    "######TT.....T..........########",
    "#########TTT######TTTT##########",
    "################################",
};

// The sawmill woods. The trail runs east; off it, in a clearing far from
// the entrance, the goddess lies surrounded by three fire creatures.
static const char *const woods[] = {
    "############################################",
    "######TT#############TTT####################",
    "####T......T#####T........T#######TT########",
    "###....f.....###......c.....T###.......#####",
    "##..,,.........T..o.........TT#.....o.....##",
    "1::::::::...............s......#..........##",
    "##..o...:::::::::.....,,....T..............#",
    "###.........TT..::::::::::.........::::::::2",
    "####T....g...T.........o..::::::::::...T####",
    "####TT.........,,..........T.....TT.....####",
    "###~~~.....T.......TT####......TTTT##...####",
    "##~~~~~~.......T...######TT.o..T######..####",
    "##~~~~~~~~=~~~.....######........T###...T###",
    "###~~~~~~~=~~~~~...T##TT.....R......##...###",
    "####..~~~~=~~~~~~~...T#T..........b..T....##",
    "####......~~~~~~~~~...#.....,,.....R.T....##",
    "###..c......~~~~~~~.......,,..........T..###",
    "###....o.....~~~~~....T####...TT.....o...###",
    "####.....s.........TTT#######..T########.###",
    "#####T.........R......T####.....T######..###",
    "######......RR....b...TT##T.......##.....###",
    "#######T.....A........###T..e..R..#....g..##",
    "########TT.......o..TT###..eMe...T#.......##",
    "###########......TTT#####.........##...TTT##",
    "#################T##########TT##############",
    "############################################",
};

// Wolf valley.
static const char *const valley[] = {
    "#################################################",
    "2:::::#..##..................##~~~~##....########",
    "#####:..........o........RR....~~~~.......#######",
    "####.:::::::::...........RRRR...~~~~....v......##",
    "###.f........:...v..............~~~~o........c..#",
    "###...TT.g...::::::::::.........~~~~......RR....#",
    "##.....T..,,..........:....o...~~~~...,....R....#",
    "###........,.....TT...:.,,.....~~~~..,......R..##",
    "###.......~~~~~~.T....=~~~~~...~~~~~...........##",
    "###..T...~~~~~~~~~~~~~=~~~~~~~~~~~~~..TT.b....###",
    "###.T...~~~~~~~~~~~~~~=~~~~~~~~~~~~~...TT.....###",
    "###.....~~~~...R~~~~~~:.TT..~~~~~~..........TT###",
    "##.RR....~~~~..RRR....:......................####",
    "###.R....~~~~.,.......:::::::::...........,,.####",
    "###.g....~~~~..,.c....:....w..:..............####",
    "####.....~~~~.........:...,...:::::::::::.....###",
    "####::::====:::::::::::.................:...o.###",
    "###.....~~~~..............RR.........w..:....####",
    "##.c..o.~~~~......TT.......R.......R....::::::::3",
    "#.T.....~~~~..b....T..............RR.........####",
    "#.....###~~~~########.........TT....R......A.####",
    "#...#####~~~~#########........#...........#..####",
    "#################################################",
};

// The elder clearing.
static const char *const clearing[] = {
    "############################################",
    "#####TTTT#########TTTT######TTTTT###########",
    "####T......T#####.......RRR........T########",
    "###.....,,......T.....RR...RR........T######",
    "##....w.........o....R.......R.....v...#####",
    "##..T........,,.....R....B....R..........###",
    "##..TT..c............R.......R...........###",
    "###.......TT..........RR...RR.....o...TT..##",
    "####.........T..........R:R..............###",
    "#####....g.....TT........:..........TT...###",
    "####........TTT.........::.....,,...T###.###",
    "###...o..........:::::::::....,,,..T###..###",
    "###.......:::::::....f.........w.....TT..###",
    "##...::::::.......,,...TT..........b.....###",
    "##..::.......T....,,..TTT....~~~~........###",
    "#..::....RR.......o...T....~~~~~~~~~..c..###",
    "3::.....RRR..b...........~~~~~~~~~~~...TT###",
    "##.......R.....T.....A.....~~~~~~~.......###",
    "###..TT.......TTT..........TT.........T..###",
    "#####TT...T..TTTTT.....TTTTTTT....TT###..###",
    "###########TTT######TTTT######TT############",
    "############################################",
};

#define ROWS(a) (int)(sizeof(a) / sizeof((a)[0]))

static const struct {
    const char *const *rows;
    int count;
} levels[] = {
    {home, ROWS(home)},
    {woods, ROWS(woods)},
    {valley, ROWS(valley)},
    {clearing, ROWS(clearing)},
};

int area_count(void) { return ROWS(levels); }

const char *const *area_rows(int index, int *row_count)
{
    if (index < 0 || index >= area_count()) index = 0;
    *row_count = levels[index].count;
    return levels[index].rows;
}
