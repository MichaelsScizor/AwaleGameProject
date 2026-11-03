#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define PITS_PER_PLAYER 6
#define TOTAL_PITS (PITS_PER_PLAYER * 2)
#define INITIAL_SEEDS 4

   // function to print the board (layout)
void print_board(int board[TOTAL_PITS], int score1, int score2) {
    printf("\nPlayer 2 side:\n");
    for (int i = TOTAL_PITS - 1; i >= PITS_PER_PLAYER; i--) {
        printf("[%2d] ", board[i]);
    }
    printf("\nPlayer 1 side:\n");
    for (int i = 0; i < PITS_PER_PLAYER; i++) {
        printf("[%2d] ", board[i]);
    }
    printf("\nScores: P1 = %d, P2 = %d\n", score1, score2);
}

// Function to check if a player has no seeds in their pits
bool no_seeds(int board[TOTAL_PITS], int player) {
    int start = player == 1 ? 0 : PITS_PER_PLAYER;
    int end = start + PITS_PER_PLAYER;
    for (int i = start; i < end; i++) {
        if (board[i] > 0) return false;
    }
    return true;
}

// Function to perform a move
bool play_move(int board[TOTAL_PITS], int pit, int player, int *score1, int *score2) {
    int start = player == 1 ? 0 : PITS_PER_PLAYER;
    int end = start + PITS_PER_PLAYER;

    // Validate pit selection
    if (pit < start || pit >= end || board[pit] == 0) {
        printf("Invalid move.\n");
        return false;
    }

    int seeds = board[pit];
    board[pit] = 0;
    int pos = pit;

    // Sow seeds counterclockwise
    while (seeds > 0) {
        pos = (pos + 1) % TOTAL_PITS;
        board[pos]++;
        seeds--;
    }

    // Capture seeds if last seed lands in opponent's pits with 2 or 3 seeds
    int opponent_start = player == 1 ? PITS_PER_PLAYER : 0;
    int opponent_end = opponent_start + PITS_PER_PLAYER;

    while (pos >= opponent_start && pos < opponent_end &&
           (board[pos] == 2 || board[pos] == 3)) {
        if (player == 1) {
            *score1 += board[pos];
        } else {
            *score2 += board[pos];
        }
        board[pos] = 0;
        pos = (pos - 1 + TOTAL_PITS) % TOTAL_PITS;
    }

    return true;
}

int main() {
    int board[TOTAL_PITS];
    int score1 = 0, score2 = 0;
    int player = 1;

    // Initialize board
    for (int i = 0; i < TOTAL_PITS; i++) {
        board[i] = INITIAL_SEEDS;
    }

    printf("=== Awale Game ===\n");

    while (!no_seeds(board, 1) && !no_seeds(board, 2) && (score1 < 25 && score2 < 25)) {
        print_board(board, score1, score2);
        printf("Player %d, choose a pit (0-%d for P1, %d-%d for P2): ",
               player,
               PITS_PER_PLAYER - 1,
               PITS_PER_PLAYER,
               TOTAL_PITS - 1);

        int pit;
        if (scanf("%d", &pit) != 1) {
            printf("Invalid input.\n");
            return 1;
        }

        if (!play_move(board, pit, player, &score1, &score2)) {
            continue; // Retry same player
        }

        player = (player == 1) ? 2 : 1; // Switch player
    }

    // Add remaining seeds to respective players
    //for (int i = 0; i < PITS_PER_PLAYER; i++) {
      //  score1 += board[i];
    //}
    //for (int i = PITS_PER_PLAYER; i < TOTAL_PITS; i++) {
        //score2 += board[i];
    //}


    printf("\nFinal Scores: P1 = %d, P2 = %d\n", score1, score2);
    if (score1 > score2) {
        printf("Player 1 wins!\n");
    } else if (score2 > score1) {
        printf("Player 2 wins!\n");
    } else {
        printf("It's a draw!\n");
    }

    return 0;
}
