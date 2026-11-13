#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "Usage: %s server_ip port\n", argv[0]); return 1; }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) { fprintf(stderr, "Invalid IP address\n"); close(fd); return 1; }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("connect"); close(fd); return 1; }
    printf("Connected to server.\n");
    for (;;) {
        fd_set r; FD_ZERO(&r); FD_SET(fd, &r); FD_SET(STDIN_FILENO, &r);
        int max = fd > STDIN_FILENO ? fd : STDIN_FILENO;
        if (select(max + 1, &r, NULL, NULL, NULL) < 0) { if (errno == EINTR) continue; perror("select"); break; }
        if (FD_ISSET(fd, &r)) {
            char buf[2048]; ssize_t n = recv(fd, buf, sizeof(buf)-1, 0);
            if (n <= 0) { printf("Server disconnected.\n"); break; }
            buf[n] = '\0'; fputs(buf, stdout); fflush(stdout);
        }
        if (FD_ISSET(STDIN_FILENO, &r)) {
            char line[1024];
            if (!fgets(line, sizeof(line), stdin)) break;
            if (send(fd, line, strlen(line), 0) < 0) { perror("send"); break; }
            if (strncmp(line, "QUIT", 4) == 0) {printf("Goodbye! Thanks for playing!\n"); break;}
        }
    }
    close(fd);
    return 0;
}
