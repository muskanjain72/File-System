#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "stream.h"

void stream_file_from_storage(const char *ss_ip, int ss_port, int inode_no)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "%s\n", ERR_SOCKET_FAIL);
        perror("socket");
        return;
    }

    struct sockaddr_in ss_addr;
    memset(&ss_addr, 0, sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);

    if (inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "%s\n", ERR_SS_INVALID_IP);
        perror("Invalid storage server IP");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        fprintf(stderr, "%s\n", ERR_SS_CONNECTION_FAIL);
        perror("connect to storage server failed");
        close(sock);
        return;
    }

    printf("Connected to Storage Server %s:%d\n", ss_ip, ss_port);

    // Send stream request
    char req[128];
    snprintf(req, sizeof(req), "STREAM %d", inode_no);
    if (send(sock, req, strlen(req), 0) <= 0)
    {
        fprintf(stderr, "%s\n", ERR_SS_CONNECTION_FAIL);
        perror("send");
        close(sock);
        return;
    }

    printf("Sent stream request for inode %d\n", inode_no);

    // Receive file data and print word-by-word
    char buffer[1024];
    char leftover[256] = ""; // store incomplete word between recv calls
    ssize_t n;

    printf("File contents in streaming mode:\n");

    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[n] = '\0';

        char *ptr = buffer;
        char word[256];

        while (*ptr)
        {
            // Extract each word
            int i = 0;
            // prepend leftover from previous chunk
            if (leftover[0]) //ignore if leftover is empty
            {
                strcpy(word, leftover);
                i = strlen(word);
                leftover[0] = '\0';
            }
            else
            {
                word[0] = '\0';
            }

            while (*ptr && *ptr != ' ' && i < 255)
            {
                word[i++] = *ptr++;
            }
            word[i] = '\0';

            // If we reached a space, print word
            if (*ptr == ' ')
            {
                printf("%s ", word);
                fflush(stdout);
                usleep(100000); // 0.1 second delay
                ptr++;          // skip space
            }
            else if (*ptr == '\0')
            {
                // reached end of buffer, save leftover
                strcpy(leftover, word);
            }
        }
    }

    // Print any leftover word
    if (leftover[0])
    {
        printf("%s", leftover);
    }

    // Decide outcome
    if (n == 0)
    {
        // orderly close by storage server
        fprintf(stderr, "%s\n", ERR_SS_DISCONNECTED);
    }
    else if (n < 0)
    {
        // network error
        fprintf(stderr, "%s\n", ERR_SS_RECV_FAIL);
    }
    else
    {
        printf("\nStream ended successfully.\n");
    }
    close(sock);
}
