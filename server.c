/* Defining all global variable and including all necessary libraries*/
#define _POSIX_C_SOURCE 200809L
#include "awale.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>

#define MAX_CLIENTS 64
#define MAX_GAMES 32
#define NAME_LEN 24
#define BIO_LEN 256
#define LINE_LEN 1024

/*making a structure for client including all the variables needed for a client to exist*/
typedef struct {
    int fd;
    char name[NAME_LEN];
    char bio[BIO_LEN];
    char friends[256]; /* comma-separated usernames with 256 max friends */
    int private_mode;
    int pending_from;
    char input[LINE_LEN];
    size_t used;
} Client;

/*making the structure for a match and what is needed*/
typedef struct {
    int active, id, a, b;
    int watchers[MAX_CLIENTS], watcher_count;
    int private_mode; /*checks if match is private*/
    AwaleGame game;
} Match;

static Client clients[MAX_CLIENTS]; /*set max clients*/
static Match games[MAX_GAMES];/*set max current games*/
static int next_game_id = 1;/*sets first match id to 1*/

/*makes sending formatted messages over the server much easier*/
static void sendf(int fd, const char *fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf);
        (void)send(fd, buf, len, MSG_NOSIGNAL);
    }
}

/*notify the client they've been connected*/
static void notify_client(int idx, const char *msg)
{
    if (idx >= 0 && idx < MAX_CLIENTS && clients[idx].fd >= 0)
        sendf(clients[idx].fd, "%s\n", msg);
}

/*find the user of the client*/
static int find_user(const char *name)
{
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (clients[i].fd >= 0 && strcmp(clients[i].name, name) == 0) return i;
    return -1;
}

/*find the game with the ID provided*/
static int find_game(int id)
{
    for (int i = 0; i < MAX_GAMES; ++i)
        if (games[i].active && games[i].id == id) return i;
    return -1;
}

/*Checks if the user is playing a game*/
static int user_game(int user)
{
    for (int i = 0; i < MAX_GAMES; ++i)
        if (games[i].active && (games[i].a == user || games[i].b == user)) return i;
    return -1;
}

/* sets up the board text and the pits with X number of seeds in it*/
static void board_text(const Match *m, char *out, size_t cap)
{
    size_t n = 0;
    n += (size_t)snprintf(out+n, cap-n, "GAME %d: %s vs %s | P1=%d P2=%d | turn=%s\n",
        m->id, clients[m->a].name, clients[m->b].name,
        m->game.score[0], m->game.score[1],
        clients[m->game.turn == 0 ? m->a : m->b].name);
    n += (size_t)snprintf(out+n, cap-n, "P2:");
    for (int i = 11; i >= 6; --i) n += (size_t)snprintf(out+n, cap-n, " %d", m->game.pits[i]);
    n += (size_t)snprintf(out+n, cap-n, "\nP1:");
    for (int i = 0; i < 6; ++i) n += (size_t)snprintf(out+n, cap-n, " %d", m->game.pits[i]);
    (void)snprintf(out+n, cap-n, "\n");
}

/*sends current state of game to both players and everyone watching*/
static void broadcast_match(Match *m)
{
    char text[1024];
    board_text(m, text, sizeof(text));
    notify_client(m->a, text);
    notify_client(m->b, text);
    for (int i = 0; i < m->watcher_count; ++i)
        notify_client(m->watchers[i], text);
}

/*checks if the game owner and the watcher is friends*/
static int is_friend(int owner, int watcher)
{
    char copy[256];
    snprintf(copy, sizeof(copy), "%s", clients[owner].friends);
    char *save = NULL;
    for (char *p = strtok_r(copy, ",", &save); p; p = strtok_r(NULL, ",", &save))
        if (strcmp(p, clients[watcher].name) == 0) return 1;
    return 0;
}
/*saves the game to games.log and closes it, otherwise it just prints the result*/
static void save_match(const Match *m)
{
    FILE *f = fopen("games.log", "a");
    if (!f) return;
    fprintf(f, "game=%d player1=%s player2=%s score1=%d score2=%d winner=%s\n",
        m->id, clients[m->a].name, clients[m->b].name,
        m->game.score[0], m->game.score[1],
        m->game.winner < 0 ? "draw" :
        clients[m->game.winner == 0 ? m->a : m->b].name);
    fclose(f);
}

/*ends game and notifies all players and spectators that it's finished*/
static void finish_match(Match *m)
{
    save_match(m); /*saves match*/
    char msg[256];
    snprintf(msg, sizeof(msg), "GAME %d OVER: %s (%d) - %s (%d).",
        m->id, clients[m->a].name, m->game.score[0],
        clients[m->b].name, m->game.score[1]);
    notify_client(m->a, msg);
    notify_client(m->b, msg);
    for (int i = 0; i < m->watcher_count; ++i) notify_client(m->watchers[i], msg);
    m->active = 0;
}

/* function to recognise which command is being used*/
static void command(int u, char *line)
{
    char *save = NULL;
    char *cmd = strtok_r(line, " \t\r\n", &save);
    if (!cmd) return;

    if (strcmp(cmd, "REGISTER") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save);
        if (!name || strlen(name) >= NAME_LEN ||
            strspn(name, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") != strlen(name)) {
            notify_client(u, "ERR usage: REGISTER username (letters, digits, underscore; max 23)");
        } else if (find_user(name) >= 0 && find_user(name) != u) {
            notify_client(u, "ERR username already in use");
        } else {
            snprintf(clients[u].name, NAME_LEN, "%s", name);
            notify_client(u, "OK registered");
        }
    } else if (clients[u].name[0] == '\0') {
        notify_client(u, "ERR register first: REGISTER username");
    } else if (strcmp(cmd, "HELP") == 0) {
        notify_client(u, "Commands: LIST - Lists users that are currently online|\nCHALLENGE user - Challenges a user to a game|\nACCEPT user - Accept a user's challenge|\nREFUSE user - Refuse a user's challenge |\nMOVE pit(0-5) - Make a move from pit 0-5 (left to right for P1 and right to left P2) |\nGAMES - Shows a list of live games with their match ID |\nWATCH id - Watch the match of the id provided |\nSAY user message - Send the user a private message if their profile is public |\nBIO text - Set your bio |\nGETBIO user - Get the bio of a diff user |\nFRIEND user - Friend the user|\nPRIVATE on/off - Set your privacy to public or private affecting if random people can join/spectate your games|\nQUIT - Quit the server");}
    else if (strcmp(cmd, "LIST") == 0) {
        char out[2048] = "Online:";
        for (int i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].fd >= 0 && clients[i].name[0])
                snprintf(out + strlen(out), sizeof(out)-strlen(out), "\n\t%s - Privacy: %s", clients[i].name, clients[i].private_mode ? "Private": "Public");
        notify_client(u, out);
    } else if (strcmp(cmd, "CHALLENGE") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save);
        int v = name ? find_user(name) : -1;
        if (v < 0 || v == u) notify_client(u, "ERR user not online");
        else if (user_game(u) >= 0 || user_game(v) >= 0) notify_client(u, "ERR one player is already in a match");
        else {
            char msg[128]; snprintf(msg, sizeof(msg), "CHALLENGE from %s. Type ACCEPT %s or REFUSE %s", clients[u].name, clients[u].name, clients[u].name);
            clients[v].pending_from = u;
            notify_client(v, msg);
            notify_client(u, "OK challenge sent");
        }
    } else if (strcmp(cmd, "ACCEPT") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save);
        int v = name ? find_user(name) : -1, slot = -1;
        if (v < 0 || clients[u].pending_from != v || user_game(u) >= 0 || user_game(v) >= 0) notify_client(u, "ERR no matching pending challenge or a player is busy");
        else {
            for (int i = 0; i < MAX_GAMES; ++i) if (!games[i].active) { slot = i; break; }
            if (slot < 0) notify_client(u, "ERR server game capacity reached");
            else {
                Match *m = &games[slot];
                memset(m, 0, sizeof(*m)); m->active = 1; m->id = next_game_id++;
                m->a = v; m->b = u; clients[u].pending_from = -1; m->private_mode = clients[u].private_mode || clients[v].private_mode;
                awale_init(&m->game); m->game.turn = rand() % 2;
                char msg[128]; snprintf(msg, sizeof(msg), "OK game %d started; %s starts. Use MOVE 0..5", m->id, clients[m->game.turn ? m->b : m->a].name);
                notify_client(u, msg); notify_client(v, msg); broadcast_match(m);
            }
        }
    } else if (strcmp(cmd, "REFUSE") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save); int v = name ? find_user(name) : -1;
        if (v >= 0 && clients[u].pending_from == v) { clients[u].pending_from = -1; notify_client(v, "Challenge refused"); notify_client(u, "OK refused"); }
        else notify_client(u, "ERR user not online");
    } else if (strcmp(cmd, "MOVE") == 0) {
        char *arg = strtok_r(NULL, " \t\r\n", &save); int pit = arg ? atoi(arg) : -1;
        int gi = user_game(u);
        if (gi < 0) notify_client(u, "ERR you are not in a game");
        else {
            Match *m = &games[gi]; int p = (m->a == u) ? 0 : 1;
            if (!awale_move(&m->game, p, pit)) notify_client(u, "ERR illegal move; check turn and pit (0-5)");
            else { broadcast_match(m); if (m->game.over) finish_match(m); }
        }
    } else if (strcmp(cmd, "GAMES") == 0) {
        int found = 0;
        for (int i = 0; i < MAX_GAMES; ++i) if (games[i].active) {
            char msg[128]; snprintf(msg, sizeof(msg), "Game %d: %s vs %s", games[i].id, clients[games[i].a].name, clients[games[i].b].name);
            notify_client(u, msg); found = 1;
        }
        if (!found) notify_client(u, "No active games");
    } else if (strcmp(cmd, "WATCH") == 0) {
        char *arg = strtok_r(NULL, " \t\r\n", &save); int id = arg ? atoi(arg) : -1;
        int gi = find_game(id);
        if (gi < 0) notify_client(u, "ERR game not found");
        else {
            Match *m = &games[gi];
            if (m->private_mode && !is_friend(m->a, u) && !is_friend(m->b, u))
                notify_client(u, "ERR private game: you are not on either player's friend list");
            else if (u == m->a || u == m->b) notify_client(u, "ERR players cannot watch their own game");
            else if (m->watcher_count >= MAX_CLIENTS) notify_client(u, "ERR watcher capacity reached");
            else {
                m->watchers[m->watcher_count++] = u;
                notify_client(u, "OK watching game");
                broadcast_match(m);
            }
        }
    } else if (strcmp(cmd, "SAY") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save);
        char *msg = save; int v = name ? find_user(name) : -1;
        while (msg && (*msg == ' ' || *msg == '\t')) ++msg;
        if (v < 0 || !msg || !*msg) notify_client(u, "ERR usage: SAY username message");
        else { sendf(clients[v].fd, "[private] %s: %s\n", clients[u].name, msg); notify_client(u, "OK sent"); }
    } else if (strcmp(cmd, "BIO") == 0) {
        char *bio = save; while (bio && (*bio == ' ' || *bio == '\t')) ++bio;
        if (!bio || strlen(bio) >= BIO_LEN) notify_client(u, "ERR BIO text (max 255 chars)");
        else { snprintf(clients[u].bio, BIO_LEN, "%s", bio); notify_client(u, "OK bio saved"); }
    } else if (strcmp(cmd, "GETBIO") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save); int v = name ? find_user(name) : -1;
        if (v < 0) notify_client(u, "ERR user not online");
        else sendf(clients[u].fd, "BIO %s: %s\n", clients[v].name, clients[v].bio[0] ? clients[v].bio : "(empty)");
    } else if (strcmp(cmd, "FRIEND") == 0) {
        char *name = strtok_r(NULL, " \t\r\n", &save); int v = name ? find_user(name) : -1;
        if (v < 0 || v == u) notify_client(u, "ERR usage: FRIEND online_username");
        else {
            {
                char friend_name[NAME_LEN];
                size_t used = strlen(clients[u].friends);
                snprintf(friend_name, sizeof(friend_name), "%s", clients[v].name);
                if (used + 1 + strlen(friend_name) < sizeof(clients[u].friends)) {
                    if (used) clients[u].friends[used++] = ',';
                    memcpy(clients[u].friends + used, friend_name, strlen(friend_name) + 1);
                    notify_client(u, "OK friend added (for this server session)");
                } else {
                    notify_client(u, "ERR friend list is full");
                }
            }
        }
    } else if (strcmp(cmd, "PRIVATE") == 0) {
        char *arg = strtok_r(NULL, " \t\r\n", &save);
        if (!arg || (strcmp(arg,"on") && strcmp(arg,"off"))) notify_client(u, "ERR PRIVATE on|off");
        else { clients[u].private_mode = strcmp(arg,"on") == 0; notify_client(u, "OK privacy updated; affects games you create or join"); }
    } else if (strcmp(cmd, "QUIT") == 0) {
        notify_client(u, "BYE"); close(clients[u].fd); clients[u].fd = -1;
        for (int i = 0; i < MAX_GAMES; ++i)
            if (games[i].active && (games[i].a == u || games[i].b == u)) {
                int other = games[i].a == u ? games[i].b : games[i].a;
                notify_client(other, "Opponent disconnected; match abandoned");
                games[i].active = 0;
            }
    } else notify_client(u, "ERR unknown command; type HELP");
}

int main(int argc, char **argv)
{
    /*if more or less than 2 arguments are added an error is provided*/
    if (argc != 2) { fprintf(stderr, "Usage: %s port\n", argv[0]); return 1; }
    /*create signal*/
    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(NULL));
    for (int i = 0; i < MAX_CLIENTS; ++i) clients[i].fd = -1;
    /*create a listener*/
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    int yes = 1; setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)atoi(argv[1]));
    /*bind the client to the listener(port)*/
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(listener, 16) < 0) {
        perror("bind/listen"); close(listener); return 1;
    }
    printf("Awale server listening on port %s\n", argv[1]);
    /*infinite loop so server keeps listening*/
    for (;;) {
        fd_set readfds; FD_ZERO(&readfds); FD_SET(listener, &readfds); int maxfd = listener;
        for (int i = 0; i < MAX_CLIENTS; ++i) if (clients[i].fd >= 0) {
            FD_SET(clients[i].fd, &readfds); if (clients[i].fd > maxfd) maxfd = clients[i].fd;
        }
        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) { if (errno == EINTR) continue; perror("select"); break; }
        if (FD_ISSET(listener, &readfds)) {
            /*accept the incoming connection*/
            int fd = accept(listener, NULL, NULL);
            if (fd >= 0) {
                /*find available slot */
                int slot = -1; for (int i = 0; i < MAX_CLIENTS; ++i) if (clients[i].fd < 0) { slot = i; break; }
                /*Check whether the server has reached its client limit*/
                if (slot < 0) { send(fd, "ERR server full\n", 16, 0); close(fd); }
                /*send a welcome message*/
                else { memset(&clients[slot], 0, sizeof(clients[slot])); clients[slot].fd = fd; clients[slot].pending_from = -1; sendf(fd, "Welcome. Register with: REGISTER username. After registering type HELP for commands!\n"); }
            }
        }
        /*Loop through all clients to check whether they have sent data */
        for (int i = 0; i < MAX_CLIENTS; ++i) if (clients[i].fd >= 0 && FD_ISSET(clients[i].fd, &readfds)) {
            char ch;
            ssize_t n = recv(clients[i].fd, &ch, 1, 0);
            /*check if client disconnected or error occured and close the socket and mark it as available*/
            if (n <= 0) {
                close(clients[i].fd); clients[i].fd = -1;
                for (int g = 0; g < MAX_GAMES; ++g) if (games[g].active && (games[g].a == i || games[g].b == i)) {
                    int other = games[g].a == i ? games[g].b : games[g].a;
                    notify_client(other, "Opponent disconnected; match abandoned");
                    games[g].active = 0;
                }
                continue;
            }
            /*if there's a newline client has finished sending a command*/
            if (ch == '\n') {
                clients[i].input[clients[i].used] = '\0';
                command(i, clients[i].input);
                clients[i].used = 0;
            } else if (clients[i].used < sizeof(clients[i].input)-1) {
                clients[i].input[clients[i].used++] = ch;
            } else clients[i].used = 0;
        }
    }
    close(listener); /*close the listening socket*/
    return 0;
}
