#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "write.h"
#include <ctype.h>

/*void write_file_to_storage(const char *ss_ip, int ss_port, int inode_no, int sentence_no, int is_terminated, int client_sock_to_ns)
{
    // Implementation for writing a file to the storage server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }
    struct sockaddr_in ss_addr;
    memset(&ss_addr, 0, sizeof(ss_addr));
    ss_addr.sin_family = AF_INET;
    ss_addr.sin_port = htons(ss_port);
    if (inet_pton(AF_INET, ss_ip, &ss_addr.sin_addr) <= 0)
    {
        perror("Invalid storage server IP");
        close(sock);
        return;
    }

    // Connect to storage server
    if (connect(sock, (struct sockaddr *)&ss_addr, sizeof(ss_addr)) < 0)
    {
        perror("connect failed");
        close(sock);
        return;
    }
    printf("Connected to Storage Server %s:%d\n", ss_ip, ss_port);
    printf("sending inode number and sentence number to the ss\n");
    // Step 1: Send inode number and sentence number
    char data[64];
    snprintf(data, sizeof(data), "WRITE %d %d", inode_no, sentence_no);
    if (send(sock, &data, sizeof(data), 0) <= 0)
    {
        perror("send inode/sentence failed");
        close(sock);
        return;
    }
    // Step 2: Receive current content of the sentence
    printf("Receiving current content of the sentence from the storage server...\n");
    char full_content[8192] = {0};
    if (recv(sock, &full_content, sizeof(full_content) - 1, 0) <= 0)
    {
        perror("recv ack failed");
        close(sock);
        return;
    }
    full_content[sizeof(full_content) - 1] = '\0';
    if (strcmp(full_content, "~") == 0)
    {
        printf("Empty sentence received.\n");
        full_content[0] = '\0'; // empty sentence
    }
    printf("Current content received: %s\n", full_content);
    // Send write request (inode_no) to storage server
    printf("📝 Start typing content (type ETIRW in a new line to finish):\n");
    char input[1024];
    int MAX = 1024;
    int words_added = 0, chars_added = 0, sentences_added = 0;
    while (1)
    {
        printf("\nCurrent sentence: %s\n", full_content);
        printf("Enter position and text (e.g. '1 hey ok.'): ");

        char line[MAX];
        if (!fgets(line, sizeof(line), stdin))
            break;
        if (strncmp(line, "ETIRW", 5) == 0)
            break;

        int pos;
        char rest[MAX];

        // Parse position and rest of line
        if (sscanf(line, "%d %[^\n]", &pos, rest) < 2)
        {
            printf("Invalid input format.\n");
            continue;
        }

        // Split full_content into words
        char words[MAX][MAX];
        int count = 0;
        char temp[MAX];
        strcpy(temp, full_content);
        
        char *tok = strtok(temp, " "); // only split by spaces
        while (tok)
        {
            strcpy(words[count++], tok);
            tok = strtok(NULL, " ");
        }

        if (pos < 1 || pos > count + 1)
        {
            printf("Out of bounds.\n");
            continue;
        }

        // Split new text into words
        char new_words[MAX][MAX];
        int new_count = 0;
        char rest_copy[MAX];
        strcpy(rest_copy, rest);

        tok = strtok(rest_copy, " "); // only split by spaces
        while (tok)
        {
            strcpy(new_words[new_count++], tok);
            if (strcmp(tok, ".") == 0 || strcmp(tok, "!") == 0 || strcmp(tok, "?") == 0)
            {
                // do not Treat punctuation as separate words
                new_count--;
            }
            tok = strtok(NULL, " ");
        }

        // Update counters
        words_added += new_count;
        chars_added += strlen(rest);
        for (int i = 0; i < strlen(rest); i++)
        {
            if (rest[i] == '.' || rest[i] == '!' || rest[i] == '?')
                sentences_added++;
        }

        // Build new sentence
        char updated[1024] = "";
        int i;

        for (i = 0; i < pos - 1; i++)
        {
            strcat(updated, words[i]);
            strcat(updated, " ");
        }

        for (i = 0; i < new_count; i++)
        {
            strcat(updated, new_words[i]);
            strcat(updated, " ");
        }

        for (i = pos - 1; i < count; i++)
        {
            strcat(updated, words[i]);
            if (i != count - 1)
                strcat(updated, " ");
        }

        // Trim trailing spaces
        int len = strlen(updated);
        while (len > 0 && updated[len - 1] == ' ')
            updated[--len] = '\0';

        strcpy(full_content, updated);

        printf("Updated: %s\n", full_content);
        printf("Words added: %d, Chars added: %d, Sentences added: %d\n",
               words_added, chars_added, sentences_added);
    }
    if (!is_terminated)
    {
        // Check if the last character indicates termination
        size_t len = strlen(full_content);
        if (len > 0)
        {
            char last_char = full_content[len - 1];
            if (last_char == '.' || last_char == '?' || last_char == '!')
            {
                is_terminated = 1;
            }
        }
    }
    // Step 3: Send full content to storage server
    if (send(sock, full_content, strlen(full_content), 0) <= 0)
    {
        perror("send content failed");
        close(sock);
        return;
    }
    // Step 4: Wait for WRITE_SUCCESS from storage server
    char ack_buf[32];
    ssize_t n = recv(sock, ack_buf, sizeof(ack_buf) - 1, 0);
    if (n <= 0)
    {
        perror("recv failed");
        close(sock);
        return;
    }
    ack_buf[n] = '\0';
    // int terminated = is_terminated_prev;
    if (strncmp(ack_buf, "WRITE_SUCCESS", 13) == 0)
    {
        printf("✅ Storage Server confirmed write of inode %d, sentence %d\n", inode_no, sentence_no);
        // Step 5: Send metadata back to NameServer
        char ns_packet[256];
        snprintf(ns_packet, sizeof(ns_packet),
                 "RESULT|%d|%d|%d|%d",
                 sentences_added, words_added, chars_added, is_terminated);

        if (send(client_sock_to_ns, ns_packet, strlen(ns_packet), 0) <= 0)
        {
            perror("send metadata to NS failed");
        }
        else
        {
            printf("📊 Sent metadata to NameServer: %s\n", ns_packet);
        }
    }
    else
    {
        printf("❌ Storage Server failed to write sentence %d of inode %d\n", sentence_no, inode_no);
    }

    close(sock);
}
*/
#define MAX 1024
int split_with_punct(const char *str, char words[MAX][MAX])
{
    int count = 0, j = 0;
    for (int i = 0; str[i]; i++)
    {
        if (str[i] == ' ' || str[i] == '\n')
        {
            if (j > 0)
            {
                words[count][j] = '\0';
                count++;
                j = 0;
            }
        }
        else if (str[i] == '.' || str[i] == '!' || str[i] == '?')
        {
            if (j > 0)
            {
                words[count][j] = '\0';
                count++;
                j = 0;
            }
            words[count][0] = str[i];
            words[count][1] = '\0';
            count++;
        }
        else
        {
            words[count][j++] = str[i];
        }
    }
    if (j > 0)
    {
        words[count][j] = '\0';
        count++;
    }
    return count;
}
void write_file_to_storage(const char *ss_ip, int ss_port, int inode_no, int sentence_no, int is_terminated, int client_sock_to_ns)
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
        perror("connect failed");
        close(sock);
        // Notify NameServer about the failure using a Packet (consistent format)
        Packet p_fail;
        p_fail.client_id = 0;
        snprintf(p_fail.msg, sizeof(p_fail.msg), "%s", ERR_SS_CONNECTION_FAIL);
        send(client_sock_to_ns, &p_fail, sizeof(p_fail), 0);
        return;
    }

    printf("Connected to Storage Server %s:%d\n", ss_ip, ss_port);
    printf("sending inode number and sentence number to the ss\n");

    // Step 1: Send inode number and sentence number
    char data[64];
    snprintf(data, sizeof(data), "WRITE %d %d", inode_no, sentence_no);
    if (send(sock, &data, sizeof(data), 0) <= 0)
    {
        fprintf(stderr, "%s\n", ERR_SS_CONNECTION_FAIL);
        perror("send inode/sentence failed");
        close(sock);
        return;
    }

    // Step 2: Receive current content of the sentence
    printf("Receiving current content of the sentence from the storage server...\n");
    char full_content[8192] = {0};
    if (recv(sock, &full_content, sizeof(full_content) - 1, 0) <= 0)
    {
        
        fprintf(stderr, "%s\n", ERR_SS_RECV_FAIL);
        perror("recv ack failed");
        close(sock);
        return;
    }

    full_content[sizeof(full_content) - 1] = '\0';
    if (strcmp(full_content, "~") == 0)
    {
        printf("Empty sentence received.\n");
        full_content[0] = '\0';
    }

    printf("Current content received: %s\n", full_content);
    printf("📝 Start typing content (type ETIRW in a new line to finish):\n");

    int words_added = 0, chars_added = 0, sentences_added = 0;

    while (1)
    {
        printf("\nCurrent sentence: %s\n", full_content);
        printf("Enter position and text (e.g. '1 hey ok.'): ");

        char line[MAX];
        if (!fgets(line, sizeof(line), stdin))
            break;
        if (strncmp(line, "ETIRW", 5) == 0)
            break;

        int pos;
        char rest[MAX];
        if (sscanf(line, "%d %[^\n]", &pos, rest) < 2)
        {
            printf("Invalid input format.\n");
            continue;
        }

        // ✅ Split existing content (preserve punctuation)
        char words[MAX][MAX];
        int count = split_with_punct(full_content, words);

        if (pos < 1 || pos > count + 1)
        {
            printf("Out of bounds.\n");
            continue;
        }

        // ✅ Split new input (preserve punctuation)
        char new_words[MAX][MAX];
        int new_count = split_with_punct(rest, new_words);

        // ✅ Update counters
        words_added += new_count;
        chars_added += strlen(rest);
        for (int i = 0; i < strlen(rest); i++)
        {
            if (rest[i] == '.' || rest[i] == '!' || rest[i] == '?')
                sentences_added++;
        }

        // ✅ Rebuild sentence, preserving punctuation (no spaces before .!?)
        char updated[8192] = "";
        for (int i = 0; i < pos - 1; i++)
        {
            strcat(updated, words[i]);
            if (!(strcmp(words[i], ".") == 0 || strcmp(words[i], "!") == 0 || strcmp(words[i], "?") == 0))
                strcat(updated, " ");
        }

        for (int i = 0; i < new_count; i++)
        {
            strcat(updated, new_words[i]);
            if (!(strcmp(new_words[i], ".") == 0 || strcmp(new_words[i], "!") == 0 || strcmp(new_words[i], "?") == 0))
                strcat(updated, " ");
        }

        for (int i = pos - 1; i < count; i++)
        {
            strcat(updated, words[i]);
            if (i != count - 1 &&
                !(strcmp(words[i], ".") == 0 || strcmp(words[i], "!") == 0 || strcmp(words[i], "?") == 0))
                strcat(updated, " ");
        }

        // Trim trailing spaces
        int len = strlen(updated);
        while (len > 0 && updated[len - 1] == ' ')
            updated[--len] = '\0';

        strcpy(full_content, updated);

        printf("Updated: %s\n", full_content);
        printf("Words added: %d, Chars added: %d, Sentences added: %d\n",
               words_added, chars_added, sentences_added);
    }

    if (!is_terminated)
    {
        size_t len = strlen(full_content);
        if (len > 0)
        {
            char last_char = full_content[len - 1];
            if (last_char == '.' || last_char == '?' || last_char == '!')
            {
                is_terminated = 1;
            }
        }
    }

    // Step 3: Send full content to storage server
    if (send(sock, full_content, strlen(full_content), 0) <= 0)
    {
        
        fprintf(stderr, "%s\n", ERR_SS_CONNECTION_FAIL);
        perror("send content failed");
        close(sock);
        return;
    }

    // Step 4: Wait for WRITE_SUCCESS
    char ack_buf[32];
    ssize_t n = recv(sock, ack_buf, sizeof(ack_buf) - 1, 0);
    if (n <= 0)
    {
        
        fprintf(stderr, "%s\n", ERR_SS_RECV_FAIL);
        perror("recv failed");
        close(sock);
        return;
    }
    ack_buf[n] = '\0';

    if (strncmp(ack_buf, "WRITE_SUCCESS", 13) == 0)
    {
        printf("✅ Storage Server confirmed write of inode %d, sentence %d\n", inode_no, sentence_no);
        char ns_packet[256];
        snprintf(ns_packet, sizeof(ns_packet),
                 "RESULT|%d|%d|%d|%d",
                 sentences_added, words_added, chars_added, is_terminated);
        
        p1 p;
        p.id=0;
        strncpy(p.msg, ns_packet, sizeof(p.msg) - 1);
        p.msg[sizeof(p.msg) - 1] = '\0';

        if (send(client_sock_to_ns, &p, sizeof(p), 0) <= 0)
        {
            fprintf(stderr, "%s\n", ERR_NM_CONNECTION_FAIL);
            perror("send metadata to NS failed");
        }
        else
        {
            printf("📊 Sent metadata to NameServer: %s\n", ns_packet);
            printf("%d %d %d %d\n",sentences_added,words_added,chars_added,is_terminated);
        }   
    }
    else
    {
        printf("❌ Storage Server failed to write sentence %d of inode %d\n", sentence_no, inode_no);
    }

    close(sock);
}
// RESULT , WORD ADDED, SENTENCE ADDED, CHARACTER ADDED, IS TEMRINATED((0 OR 1))
