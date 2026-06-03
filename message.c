#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "message.h"

#define MSG_SIZE_LIMIT (1 << 24)

static int send_all(int fd, const void* buf, size_t len) {
    const void* p = buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, p + total, len - total);
        if (n <= 0) return -1;

        total += n;
    }
    return 0;
}

static int recv_all(int fd, void* buf, size_t len) {
    char* p = buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, p + total, len - total);
        if (n == 0) {
            return -1;
        } else if (n < 0) {
            if (errno == EINTR) continue;
            else return -1;
        } else {
            total += n;
        }
    }
    return 0;
}

void send_msg(int fd, MsgType type, const void* body, int count, size_t element_size) {
    MsgHeader header = {type, count, element_size};
    send_all(fd, &header, sizeof(header));
    if (count > 0) send_all(fd, body, count*element_size);
}

int recv_alloc_msg(int fd, MsgHeader* header, void** body_out) {
    int r = recv_all(fd, header, sizeof(MsgHeader));
    if (r < 0) {
        *body_out = NULL; 
        return r;
    }
    size_t body_len = header->count * header->element_size;
    
    if (body_len > 0 && body_len < MSG_SIZE_LIMIT) {
        *body_out = malloc(body_len);
        int r = recv_all(fd, *body_out, body_len);
        if (r < 0) {
            free(*body_out);
            *body_out = NULL;
            return r;
        }
    } else {
        *body_out = NULL;
    }
    return 0;
}
