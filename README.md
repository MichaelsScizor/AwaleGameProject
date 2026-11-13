# Awalé client/server (C, TCP)

## Contents

- `awale.c`, `awale.h`: isolated game engine (board, legal moves, sowing, capture, scoring, end conditions).
- `server.c`: TCP server, username registration, online list, challenges, simultaneous matches, spectators, chat, biographies, friends/privacy and completed-game log.
- `client.c`: terminal TCP client.
- `Makefile`: build instructions.

## Build

Requires a POSIX system (Linux/macOS), GCC or Clang, and `make`.

```sh
make
```

## Run

Terminal 1:
```sh
./awale_server 4242
```

Terminal 2 and 3 (one per player):
```sh
./awale_client 127.0.0.1 4242
```

Each player first registers:
```text
REGISTER alice
REGISTER bob
```

## Commands

```text
HELP
REGISTER username       username: letters, digits, underscore; max 23 chars
LIST                    list online users
CHALLENGE username      challenge an online user
ACCEPT username         accept that user's challenge
REFUSE username         refuse a challenge
MOVE 0                  play own pit 0 (leftmost own pit; valid range 0..5)
GAMES                   list active matches
WATCH game_id           observe a public match
SAY username message    private message
BIO text                set a one-line biography (max 255 chars)
GETBIO username         display an online user's biography
FRIEND username         add an online user to your friend list
PRIVATE on|off          make future matches friends-only for spectators
QUIT                    disconnect
```

After a challenge is accepted, the server chooses the starting player randomly.
The server is authoritative: clients submit only a pit number, and the server
checks the turn and legality before updating the match. A player in a match
cannot join another match. Several different matches can run at once.

Completed match summaries are appended to `games.log` in the server's working
directory. The file records players, scores and winner.

## Awalé rule conventions used

- 12 pits, initially 4 seeds per pit.
- Player 1 owns pits 0..5; Player 2 owns pits 6..11.
- A move sows anticlockwise, skipping the starting pit.
- Capture proceeds backwards from the final pit through consecutive opponent
  pits containing exactly 2 or 3 seeds.
- A capture that would take every remaining opponent seed is cancelled.
- A player must feed an empty opponent side if a legal feeding move exists.
- A game ends when a side has no seeds or a score reaches 25; remaining seeds
  are collected and the higher score wins.

## Known limitations / next improvements

This is a teaching-project baseline, not a production service:
- Pending challenges are tracked in memory and can be accepted/refused while
  both players are online.
- Usernames, bios and friend lists live in memory and reset when the server exits.
- `games.log` stores completed-game summaries, not full move-by-move replay.
- The select-based server is single-threaded and uses a simple newline protocol.
  It supports multiple simultaneous clients/matches, but is not intended for
  high-load deployment.
- Chat is private-message based (`SAY`); room/game-wide chat can be added.
- Private spectator access is checked against either player's friend list.
- Disconnection abandons an active match; reconnect/resume is not implemented.
- For a formal Awalé tournament, confirm the exact variant and end-game rules
  expected by your instructor; regional rule variants exist.
