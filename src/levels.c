// The maps, written as text. One character is one 16x16 tile:
//
//   .  grass            #  cliff             T  tree (chop it with the axe)
//   R  rock             ~  water             =  bridge
//   c  chest (herbs)    A  chest (sharper axe)
//   f  campfire (rest; you wake up here after falling in battle)
//   X  path to the next map                  P  start
//
// Monsters, which start a battle when touched:
//   s  one slime        g  two slimes        b  a beetle and a slime
//   v  one wolf         w  two wolves        B  the Elder Stump (boss)
//
// A map needs one 'P' and an 'X' or a 'B', at most 64 rows and 96 columns.
// tests/test_game.c checks that everything can be reached (trees count as
// open, since the axe clears them).
#include "game.h"

// Map 1: the sawmill woods.
static const char *const level1[] = {
    "####################################",
    "#TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT#",
    "#T..P.....T.........T.......c..TTTT#",
    "#T........T....s....T..........T...#",
    "#T...f....T.........TTTTTT.....T...#",
    "#T........TTT..........T.......T...#",
    "#TT..................g.T.......T.X.#",
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

// Map 2: wolf valley.
static const char *const level2[] = {
    "########################################",
    "#P....TT.........RRRRR..........TTTTTTT#",
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
    "#TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT...X#",
    "########################################",
};

// Map 3: the elder clearing.
static const char *const level3[] = {
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
    "#T..P..R..A......TTTTT................T#",
    "########################################",
};

#define ROWS(a) (int)(sizeof(a) / sizeof((a)[0]))

static const struct {
    const char *const *rows;
    int count;
} levels[] = {
    {level1, ROWS(level1)},
    {level2, ROWS(level2)},
    {level3, ROWS(level3)},
};

int level_count(void) { return ROWS(levels); }

const char *const *level_rows(int index, int *row_count)
{
    if (index < 0 || index >= level_count()) index = 0;
    *row_count = levels[index].count;
    return levels[index].rows;
}
