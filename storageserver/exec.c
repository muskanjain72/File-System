#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include"exec.h"
#include "log.h"
#include <arpa/inet.h>

void send_file_to_nm(int client_fd, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");  // open in binary mode
    if (!fp) {
        p1 p;
        p.id=0;
        char err[] = "Failed to open file\n";
        strcpy(p.msg,err);
        send(client_fd, err, strlen(err), 0);
        storageserver_log("EXEC_FAIL open path=%s to_nm_fd=%d", filepath, client_fd);
        return;
    }

    /* Read up to one packet's worth of data and send a single framed packet.
     * The nameserver expects a single p1 response containing the shell command
     * (or the entire file if it's small). Streaming multiple p1 packets here
     * leaves leftover packets on the control socket and can desynchronize
     * subsequent control commands; therefore send exactly one packet. */
    char buf[sizeof(((p1*)0)->msg)];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    p1 p;
    p.id = 0;
    memset(p.msg, 0, sizeof(p.msg));
    if (n > 0) {
        if (n > sizeof(p.msg) - 1) n = sizeof(p.msg) - 1;
        memcpy(p.msg, buf, n);
        p.msg[n] = '\0';
    } else {
        /* Empty file: send an explicit empty message so NM doesn't block */
        p.msg[0] = '\0';
    }
    if (send(client_fd, &p, sizeof(p), 0) <= 0) {
        perror("send failed");
    }

    fclose(fp);
    printf("✅ Sent file '%s' to nm\n", filepath);
    storageserver_log("EXEC_SENT filepath=%s to_nm_fd=%d", filepath, client_fd);
}
