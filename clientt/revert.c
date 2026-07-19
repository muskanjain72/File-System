#include"revert.h"

void revertc(const char* ip_address, int port, const char* destpath) {
    // 1) Socket creation failed
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in ss_addr;
    memset(&ss_addr, 0, sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address, &ss_addr.sin_addr) <= 0) {
        perror("Invalid storage server IP");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0) {
        perror("connect to storage server failed");
        close(sock);
        return;
    }

    printf("Connected to Storage Server %s:%d\n", ip_address, port);

    // Send revert request
    char req[512];
    snprintf(req, sizeof(req), "REVERT %s", destpath);
    if (send(sock, req, strlen(req), 0) <= 0) {
        perror("send revert request");
        close(sock);
        return;
    }

    printf("Sent revert request for %s\n", destpath);

    // Receive file data
    char buffer[1024];
    ssize_t n;
    printf("Revert contents:\n");
    while ((n = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[n] = '\0';  // null terminate
        printf("%s", buffer);
    }

    if (n < 0) perror("recv failed");

    printf("\nFinished reading checkpoint.\n");
    close(sock);
}