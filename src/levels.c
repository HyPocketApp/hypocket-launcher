// The areas of the world, written as text. One character is one 16x16 tile:
//
//   .  grass            #  cliff             T  tree (chop it with the axe)
//   R  rock             ~  water             =  bridge
//   c  chest (herbs)    A  chest (sharper axe)
//   f  campfire (rest; you wake up here after falling in battle)
//   H  house            D  its door (rest)   W  firewood     :  dirt path
//   M  the Moon Goddess, waiting for help    P  start (only the home has one)
//   1-9  path to another area: walking onto it leads to the area with the
//        same digit, next to that digit. Each digit is in exactly two areas.
//
// Monsters, which start a battle when touched:
//   s  one slime        g  two slimes        b  a beetle and a slime
//   v  one wolf         w  two wolves        B  the Elder Stump (boss)
//   e  fire beings attacking the goddess (they stay put; no running away)
//
// An area needs a path or a 'B', at most 64 rows and 96 columns.
// tests/test_game.c checks that everything can be reached (trees count as
// open, since the axe clears them) and that the paths pair up.
#include "game.h"

// The lumberjack's home: no monsters. The path east leads into the woods.
static const char *const home[] = {
    "########################",
    "#TTTTTTTTTTTTTTTTTTTTTT#",
    "#T........TT.........cT#",
    "#T..HHHH..TT..........T#",
    "#T..HHHH..............T#",
    "#T..HDHH.W.......TT...T#",
    "#T...P...........TT...T#",
    "#T...::::::::::::::::::1",
    "#T....................T#",
    "#T...TT.......TT......T#",
    "#T...TT.......TT......T#",
    "#TTTTTTTTTTTTTTTTTTTTTT#",
    "########################",
};

// The sawmill woods. The goddess is near the start, under attack.
static const char *const woods[] = {
    "####################################",
    "#TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT#",
    "1:::::::::T.........T.......c..TTTT#",
    "#T........T....s....T..........T...#",
    "#T...f....T.........TTTTTT.....T...#",
    "#T......e.TTT..........T.......T...#",
    "#TT.....M............g.T.......T...2",
    "#T....RR......T........T.......T...#",
    "#T....RR.....TTT...............T...#",
    "#T............T.....TTT........TTTT#",
    "#TTTT...~~~~~~~~~~~~=~~~~~~~~~TTTTT#",
    "#T......~~~~~~~~~~~~=~~~~~~~~~TTTTT#",
    "#T..c..............................#",
    "#T..........b.........TTTT.........#",
    "#TTTTT................T..T....A....#",
    "#T...T......s.....RRR.T..T.........#",
    "#T.c.T............R.R..........g...#",
    "#T...T............RRR..............#",
    "#TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT#",
    "####################################",
};

// Wolf valley.
static const char *const valley[] = {
    "########################################",
    "2.....TT.........RRRRR..........TTTTTTT#",
    "#.....TT....v....R...R....w.....T.....T#",
    "#..f..............................c...T#",
    "#.....TT.........R...R..........T.....T#",
    "#TT..TTT.........RR.RR..........TTT.TTT#",
    "#~~~~~~~~~=~~~~~~~~~~~~~~~~~~~~~~~~.~~~#",
    "#~~~~~~~~~=~~~~~~~~~~~~~~~~~~~~~~~~.~~~#",
    "#.........................b............#",
    "#..TTT.......RR........TTT.......g.....#",
    "#..T.T..c....RR........T.T.............#",
    "#..TTT.................TTT.....RRRRRRR.#",
    "#..........v.......................f.R.#",
    "#RRRRR.........TTTTT...........R.....R.#",
    "#R...R....w....T...T.....v.....R..A..R.#",
    "#R.c...........T...T...........R.....R.#",
    "#RRRRR.........TTTTT...........RRRR.RR.#",
    "#......................................#",
    "#TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT....3",
    "########################################",
};

// The elder clearing.
static const char *const clearing[] = {
    "########################################",
    "#TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT#",
    "#T..................RRRRR.............T#",
    "#T.......TT.........R...R......TT.....T#",
    "#T.......TT.........R.B.R......TT.....T#",
    "#T..................R...R.............T#",
    "#T...w..............RR.RR.......v.....T#",
    "#T....................................T#",
    "#TTTTTTTTTTTTTTTTT.......TTTTTTTTTTTTTT#",
    "#T.....T..........f..........T.......cT#",
    "#T..c..T.....................T..w....TT#",
    "#T.....T.....RR......RR......T.......TT#",
    "#TTT.TTT.....RR..g...RR......TTTTT.TTTT#",
    "#T...........................T.........#",
    "#T.......b.......TTTTT.......T....v....#",
    "#T...............T...T................T#",
    "#T.....RRRR......T.c.T......~~~~~=~~~~T#",
    "#T.....R..R......T...T......~~~~~=~~~~T#",
    "3......R..A......TTTTT................T#",
    "########################################",
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
