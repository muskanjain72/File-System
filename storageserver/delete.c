#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "delete.h"
#include "log.h"

void delete_file_from_storage(int client_sock, const char *filepath)
{
    printf("Attempting to delete file: %s\n", filepath);
    storageserver_log("DELETE_REQUEST filepath=%s by_fd=%d", filepath, client_sock);
    if (remove(filepath) == 0)
    {
        printf("✅ File deleted successfully: %s\n", filepath);
        storageserver_log("DELETE_OK filepath=%s by_fd=%d", filepath, client_sock);
        const char *msg = "OK|deleted\n";
        send(client_sock, msg, strlen(msg), 0);
    }
    else
    {
        perror("❌ Failed to delete file");
        storageserver_log("DELETE_FAIL filepath=%s by_fd=%d err=%s", filepath, client_sock, strerror(errno));
        char msg[128];
        snprintf(msg, sizeof(msg), "ERR|delete_failed|%s\n", filepath);
        send(client_sock, msg, strlen(msg), 0);
    }
    return;
}