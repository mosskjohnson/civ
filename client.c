#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "raylib.h"

#include "data.h"
#include "resources.h"

#define PORT 8080
#define SERVER_IP "169.231.116.248"

int main(void) {
    int sock_fd;
    struct sockaddr_in server_addr;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("CLIENT: Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, (socklen_t)sizeof(server_addr)) < 0) {
        perror("CLIENT: Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("CLIENT: Connected to server %s:%d\n", SERVER_IP, PORT);

    InitWindow(640, 480, "Civ");

    TextureManager tm = {0};

    load_textures(&tm);
    
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        TexturePortion tp = tm.units[U_KNIGHTS];
    
        BeginDrawing();
            ClearBackground((Color){200.0, 200.0, 200.0, 1.0});
            DrawTexturePro(tp.texture, tp.portion, (Rectangle){200, 200, 64, 64}, (Vector2){0.0, 0.0}, 0.0f, WHITE);
        EndDrawing();
    }

    unload_textures(&tm);

    close(sock_fd);
}
