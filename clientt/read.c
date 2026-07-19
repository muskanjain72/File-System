#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../error_codes.h"
#include <arpa/inet.h>
#include "read.h"


void read_file_from_storage(const char *ss_ip, int ss_port, int inode_no)
{

    // 1) Socket creation failed
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "%s", ERR_SOCKET_FAIL);
        perror("socket");
        return;
    }

    struct sockaddr_in ss_addr;
    memset(&ss_addr, 0, sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);

    // 2) Invalid IP address format
    if (inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr) <= 0)
    {
        // fprintf(stderr, "ERR|INVALID_IP|Storage server IP '%s' is invalid.\n", ss_ip);
        fprintf(stderr, "%s\n", ERR_SS_INVALID_IP);
        perror("Invalid storage server IP");
        close(sock);
        return;
    }

    // 3) Storage server is down or unreachable
    if (connect(sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        // fprintf(stderr, "ERR|CONNECTION_FAIL|Unable to connect to Storage Server %s:%d.\n",
        //         ss_ip, ss_port);
        // fprintf(stderr, ERR_CONNECTION_FAIL);
        fprintf(stderr, "%s\n", ERR_SS_CONNECTION_FAIL);
        perror("connect to storage server failed");
        close(sock);
        return;
    }

    printf("Connected to Storage Server %s:%d\n", ss_ip, ss_port);

    // 4) Failed to send read request
    char req[128];
    snprintf(req, sizeof(req), "READ %d", inode_no);
    if (send(sock, req, strlen(req), 0) <= 0)
    {
        fprintf(stderr, "%s\n", ERR_READ_FAIL);
        perror("send");
        close(sock);
        return;
    }

    printf("Sent read request for inode %d\n", inode_no);

    // 5) Receiving file data
    char buffer[1024];
    ssize_t n;

    printf("File contents:\n");

    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[n] = '\0';
        printf("%s", buffer);
        fflush(stdout);  // to ensure immediate output
    }

    // 6) Storage server crashed mid-transfer
    if (n < 0)
    {
        fprintf(stderr, "%s\n", ERR_SS_RECV_FAIL);
        perror("recv failed");
    }

    // 7) Server closed connection — normal EOF after transfer. Do not treat as error.
    // If recv returned 0 it means the server closed the connection after sending all data.
    // No error message should be printed in this normal case.

    printf("\nFinished reading file.\n");
    close(sock);
}
