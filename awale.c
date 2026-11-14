/*include necessary libraries*/
#include "awale.h"
#include <stdio.h>
#include <string.h>

/*initialise all the pits for the game so they have 4 seeds*/
void awale_init(AwaleGame *g)
{
    memset(g, 0, sizeof(*g));
    for (int i = 0; i < AWALE_PITS; ++i) g->pits[i] = 4;
    g->turn = 0;
    g->winner = -2;
}
/*check every pit has seeds*/
int awale_has_seeds(const AwaleGame *g, int player)
{
    int first = player == 0 ? 0 : 6;
    for (int i = first; i < first + 6; ++i)
        if (g->pits[i] != 0) return 1;
    return 0;
}

/* Return 1 if the move was accepted, 0 if illegal. Pit is 0..5
   relative to the current player's own row. */
int awale_move(AwaleGame *g, int player, int pit)
{
    if (!g || g->over || player != g->turn || pit < 0 || pit >= 6)
        return 0;

    int start = player == 0 ? 0 : 6;
    int opponent_start = player == 0 ? 6 : 0;
    int selected = start + pit;
    int seeds = g->pits[selected];
    if (seeds <= 0) return 0;

    /* A player must feed the opponent when possible. */
    if (!awale_has_seeds(g, 1 - player)) {
        int can_feed = 0;
        for (int i = start; i < start + 6; ++i) {
            int n = g->pits[i];
            if (!n) continue;
            int pos = i;
            while (n--) {
                do { pos = (pos + 1) % 12; } while (pos == i);
            }
            if (pos >= opponent_start && pos < opponent_start + 6) {
                can_feed = 1;
                break;
            }
        }
        if (can_feed) {
            int pos = selected;
            int n = seeds;
            while (n--) {
                do { pos = (pos + 1) % 12; } while (pos == selected);
            }
            if (pos < opponent_start || pos >= opponent_start + 6)
                return 0;
        }
    }

    g->pits[selected] = 0;
    int pos = selected;
    while (seeds > 0) {
        pos = (pos + 1) % 12;
        if (pos == selected) continue;
        ++g->pits[pos];
        --seeds;
    }

    /* Capture backwards through consecutive opponent pits holding 2 or 3. */
    int captured[6], values[6], count = 0, total = 0;
    int p = pos;
    while (p >= opponent_start && p < opponent_start + 6 &&
           (g->pits[p] == 2 || g->pits[p] == 3) && count < 6) {
        captured[count] = p;
        values[count] = g->pits[p];
        total += values[count++];
        p = (p + 11) % 12;
    }

    /* Grand-slam rule: cancel a capture that takes every opponent seed. */
    if (count > 0) {
        int remaining = 0;
        for (int i = opponent_start; i < opponent_start + 6; ++i)
            remaining += g->pits[i];
        if (remaining == 0) {
            for (int i = 0; i < count; ++i)
                g->pits[captured[i]] = values[i];
            total = 0;
        } else {
            for (int i = 0; i < count; ++i)
                g->pits[captured[i]] = 0;
        }
    }

    g->score[player] += total;
    g->turn = 1 - player;

    /* End if a player has no seeds, or a score reaches the usual 25-point threshold. */
    if (!awale_has_seeds(g, 0) || !awale_has_seeds(g, 1) ||
        g->score[0] >= 25 || g->score[1] >= 25) {
        g->over = 1;
        g->winner = g->score[0] > g->score[1] ? 0 :
                    g->score[1] > g->score[0] ? 1 : -1;
    }
    return 1;
}

/*prints the current game state*/
void awale_print(const AwaleGame *g)
{
    printf("       Player 2\n");
    for (int i = 11; i >= 6; --i) printf(" %2d", g->pits[i]);
    printf("\n");
    for (int i = 0; i < 6; ++i) printf(" %2d", g->pits[i]);
    printf("\n       Player 1\n");
    printf("Score P1=%d  P2=%d  Turn=P%d\n",
           g->score[0], g->score[1], g->turn + 1);
}
