#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#define PORT 8080
#define MAX_PLAYERS 5

typedef int playerID;

int main(void) {
    int server_fd;
    
    struct sockaddr_in addr;
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("SERVER: Server socket failed");
        exit(EXIT_FAILURE);
    }

    {
        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    }
    {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
    
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        perror("SERVER: Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("SERVER: Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("SERVER: Listening on port %d\n", PORT);

    int client_fds[MAX_PLAYERS];
    playerID id = 0;

    struct pollfd fds[MAX_PLAYERS+1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    char buffer[128];
    
    struct pollfd stdinfd[1];
    stdinfd[0].fd = STDIN_FILENO;
    stdinfd[0].events = POLLIN;
    // lobby phase
    while (1) {
        if (id >= MAX_PLAYERS) {
            printf("SERVER: Server full\n");
        } else {
            int ret = poll(fds, 1, 100);
            if (ret > 0 && (fds[0].revents & POLLIN)) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
                if (client_fd < 0) {
                    continue;
                } else {
                    client_fds[id++] = client_fd;
                }
            }
        }

        int ret = poll(stdinfd, 1, 100);
        if (ret > 0 && (stdinfd[0].revents & POLLIN)) {
            ssize_t bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                if (strcmp("start\n", buffer) == 0) {
                    printf("SERVER: Starting game\n");
                    break;
                }
            }
        }
    }
    // game phase
    while (1) {
        for (playerID i = 0; i < MAX_PLAYERS; ++i) {
            int fd = client_fds[i];
        }
    }

    close(server_fd);
}
