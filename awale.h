#ifndef AWALE_H
#define AWALE_H

#define AWALE_PITS 12
#define AWALE_SIDE 6

typedef struct {
    int pits[AWALE_PITS];
    int score[2];
    int turn;       /* 0 = player 1, 1 = player 2 */
    int over;
    int winner;     /* -1 draw, 0/1 player index, -2 unfinished */
} AwaleGame;

void awale_init(AwaleGame *g);
int awale_has_seeds(const AwaleGame *g, int player);
int awale_move(AwaleGame *g, int player, int pit);
void awale_print(const AwaleGame *g);

#endif
