#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include"read.h"
#include "log.h"
#include <arpa/inet.h>

void send_file_to_client(int client_fd, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");  // open in binary mode
    if (!fp) {
        char err[] = "Failed to open file\n";
        send(client_fd, err, strlen(err), 0);
        storageserver_log("READ_FAIL open path=%s to_fd=%d", filepath, client_fd);
        return;
    }

    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send(client_fd, buf, n, 0) <= 0) {
            perror("send failed");
            break;
        }
    }

    fclose(fp);
    printf("✅ Sent file '%s' to client\n", filepath);
    storageserver_log("READ_SEND filepath=%s to_fd=%d", filepath, client_fd);
}
