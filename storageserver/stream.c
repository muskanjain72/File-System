#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "log.h"

void send_file_to_client_in_stream_mode(int client_fd, const char *filepath) {
    FILE *fp = fopen(filepath, "r");  // open as text
    if (!fp) {
        char err[] = "❌ Failed to open file\n";
        send(client_fd, err, strlen(err), 0);
        storageserver_log("STREAM_FAIL open path=%s to_fd=%d", filepath, client_fd);
        return;
    }

    char word[256];

    // Read and stream the content word-by-word
    while (fscanf(fp, "%255s", word) == 1) {
        strcat(word, " "); // add a space after each word
        if (send(client_fd, word, strlen(word), 0) <= 0) {
            perror("send failed");
            break;
        }
        memset(word, 0, sizeof(word));  // optional clean-up
        usleep(100000); // delay of 0.1 seconds between words
    }

    fclose(fp);
    printf("✅ Streamed '%s' to client word-by-word.\n", filepath);
    storageserver_log("STREAM_SEND filepath=%s to_fd=%d", filepath, client_fd);
}
