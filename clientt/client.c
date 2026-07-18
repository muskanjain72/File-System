#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "client.h"

char *pretty_time(const char *ts)
{
    static char out[64];

    // If timestamp missing or not numeric
    if (!ts || !*ts || atoi(ts) == 0) {
        snprintf(out, sizeof(out), "unknown");
        return out;
    }

    time_t t = (time_t)atol(ts);  //convert to time_t
    struct tm tm;
    localtime_r(&t, &tm);

    // Format: YYYY-MM-DD HH:MM:SS
    strftime(out, sizeof(out), "%Y-%m-%d %H:%M:%S", &tm); //used to format the time in a human-readable way

    return out;
}


int main()
{
    char client_name[64];
    char server_ip[32];
    int server_port;

    // Get client info
    printf("Enter your name: ");
    scanf("%63s", client_name);
    int ci;
    while ((ci = getchar()) != '\n' && ci != EOF)
        ; // discard leftover newline

    printf("Enter server IP: ");
    scanf("%31s", server_ip);
    while ((ci = getchar()) != '\n' && ci != EOF)
        ; // discard leftover newline
    printf("Enter server port: ");
    scanf("%d", &server_port);
    while ((ci = getchar()) != '\n' && ci != EOF)
        ; // discard leftover newline

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fprintf(stderr, "%s\n", ERR_SOCKET_FAIL);
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "%s\n", ERR_NM_INVALID_IP);
        perror("Invalid server IP");
        exit(1);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "%s\n", ERR_NM_CONNECTION_FAIL);
        perror("connect failed");
        close(sock);
        exit(1);
    }

    // Get OS-assigned client IP and port
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    if (getsockname(sock, (struct sockaddr *)&client_addr, &len) < 0)
    {
        perror("getsockname failed");
        close(sock);
        exit(1);
    }
    char client_ip[32];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    printf("Connected! OS assigned IP: %s, Port: %d\n", client_ip, client_port);

    // Identify as client (type=0), then send client info struct
    int type = 0;
    if (send(sock, &type, sizeof(type), 0) <= 0)
    {
        // printf("Failed to send client type. Server disconnected?\n");
        fprintf(stderr, "%s\n", ERR_NM_CONNECTION_FAIL);
        perror("send failed");
        close(sock);
        exit(1);
    }

    // Send client info to server
    Client c;
    memset(&c, 0, sizeof(c)); // Initialize all fields to 0
    c.client_id = 0;          // Server can assign unique ID
    strncpy(c.client_name, client_name, sizeof(c.client_name) - 1);
    strncpy(c.ip_address, client_ip, sizeof(c.ip_address) - 1);
    c.port = client_port;

    if (send(sock, &c, sizeof(c), 0) <= 0)
    {
        // printf("Failed to send client info. Server disconnected?\n");
        fprintf(stderr, "%s\n", ERR_NM_CONNECTION_FAIL);
        perror("send failed");
        close(sock);
        exit(1);
    }

    printf("Client info sent to server.\n");

    int m;
    if (recv(sock, &m, sizeof(m), 0) <= 0)
    {
        // printf("Failed to receive assigned client ID. Server disconnected?\n");
        fprintf(stderr, "%s\n", ERR_NM_RECV_FAIL);
        perror("recv failed");
        close(sock);
        exit(1);
    }
    c.client_id = m;
    printf("Assigned Client ID from server: %d\n", c.client_id);

    // Main loop: send messages
    char buf[1024];
    while (1)
    {
        printf("> ");
        if (!fgets(buf, sizeof(buf), stdin))
            break;

        buf[strcspn(buf, "\n")] = 0; // remove newline

        if (strcmp(buf, "exit") == 0)
            break;

        // --- Prepare packet to send ---
        Packet p;
        p.client_id = c.client_id;
        strncpy(p.msg, buf, sizeof(p.msg) - 1);
        p.msg[sizeof(p.msg) - 1] = '\0';

    if (strncmp(p.msg, "VIEW", 4) != 0 && strncmp(p.msg, "READ", 4) != 0 && strncmp(p.msg, "WRITE", 5) != 0 && strncmp(p.msg, "CREATE", 6) != 0 && strncmp(p.msg, "INFO", 4) != 0 && strncmp(p.msg, "UNDO", 4) != 0 && strncmp(p.msg, "EXEC", 4) != 0 && strncmp(p.msg, "DELETE", 6) != 0 && strncmp(p.msg, "STREAM", 6) != 0 && strncmp(p.msg, "LIST", 4) != 0 && strncmp(p.msg, "ADDACCESS", 9) != 0 && strncmp(p.msg, "REMACCESS", 9) != 0 && strncmp(p.msg, "CREATEFOLDER", 13) != 0 && strncmp(p.msg, "MOVE", 4) != 0 && strncmp(p.msg, "CHECKPOINT",10)!=0 && strncmp(p.msg, "REVERT",6)!=0 && strncmp(p.msg,"VIEWCHECKPOINT",13)!=0 && strncmp(p.msg,"LISTCHECKPOINT",14)!=0 && strncmp(p.msg, "REQUESTACCESS", 13) != 0 && strncmp(p.msg, "VIEWREQUESTS", 12) != 0 && strncmp(p.msg, "APPROVE", 7) != 0 && strncmp(p.msg, "DENY", 4) != 0)
        {
            // printf("ONot a valid cmd.\n");
            printf(ERR_INVALID_CMD "\n");
            continue;
        }

        // --- Send to server ---
        ssize_t n = send(sock, &p, sizeof(p), 0);

        if (n <= 0)
        {
            if (n == 0)
                // printf("Server disconnected!\n");
                fprintf(stderr, "%s\n", ERR_NM_DISCONNECTED);
            else
                perror("send failed");
            break;
        }

        // --- Wait for response from server ---
        Packet response;
        size_t got = 0;
        while (got < sizeof(response))
        {
            ssize_t rn = recv(sock, (char *)&response + got, sizeof(response) - got, 0);
            if (rn <= 0)
            {
                // printf("Server closed connection.\n");
                fprintf(stderr, "%s\n", ERR_NM_RECV_FAIL);
                perror("recv failed");
                got = 0;
                break;
            }
            got += (size_t)rn;
        }
        if (got != sizeof(response))
            break;

        if (strncmp(response.msg, "READ|", 5) == 0)
        {
            char *token;
            char buf[1024];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            token = strtok(buf, "|");  // "READ"
            token = strtok(NULL, "|"); // filename
            char filename[64];
            strncpy(filename, token, sizeof(filename));

            token = strtok(NULL, "|"); // ss_ip
            char ss_ip[32];
            strncpy(ss_ip, token, sizeof(ss_ip));

            token = strtok(NULL, "|"); // ss_port
            int ss_port = atoi(token);

            token = strtok(NULL, "|"); // inode_no
            int inode_no = atoi(token);

            printf("File '%s' is on Storage Server %s:%d (inode %d)\n",
                   filename, ss_ip, ss_port, inode_no);

            read_file_from_storage(ss_ip, ss_port, inode_no);

            // Now you can connect to ss_ip:ss_port to read the file
        }
        else if (strncmp(response.msg, "WRITE|", 6) == 0)
        {
            char *token;
            char buf[1024];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            token = strtok(buf, "|");  // "WRITE"
            token = strtok(NULL, "|"); // filename
            char filename[64];
            strncpy(filename, token, sizeof(filename));

            token = strtok(NULL, "|"); // ss_ip
            char ss_ip[32];
            strncpy(ss_ip, token, sizeof(ss_ip));

            token = strtok(NULL, "|"); // ss_port
            int ss_port = atoi(token);

            token = strtok(NULL, "|"); // inode_no
            int inode_no = atoi(token);

            token = strtok(NULL, "|"); // sentence number
            int sentence_no = token ? atoi(token) : 0;

            int is_terminated = 0;
            token = strtok(NULL, "|"); // is_terminated
            is_terminated = token ? atoi(token) : 0;

            // printf("File '%s' should be written to Storage Server %s:%d (inode %d)\n",
            //        filename, ss_ip, ss_port, inode_no);
            printf("WRITE request approved: File '%s', sentence %d, SS=%s:%d (inode=%d)\n",
                   filename, sentence_no, ss_ip, ss_port, inode_no);

            write_file_to_storage(ss_ip, ss_port, inode_no, sentence_no, is_terminated, sock);

            // Now you can connect to ss_ip:ss_port to write the file
        }
        else if (strncmp(response.msg, "ERR|", 4) == 0)
        {
            char tmp[1024];
            strncpy(tmp, response.msg, sizeof(tmp));
            tmp[sizeof(tmp) - 1] = '\0';
            /* skip the leading "ERR" token */
            strtok(tmp, "|");
            char *code = strtok(NULL, "|");
            char *detail = strtok(NULL, "|");
            const char *c = code ? code : "unknown";
            const char *d = detail ? detail : "";

            /* Map common server error codes to friendlier messages for folder/move/view flows */
            if (strcmp(c, "no_such_folder") == 0)
            {
                printf("Error: folder '%s' does not exist.\n", d);
            }
            else if (strcmp(c, "missing_parent") == 0)
            {
                printf("Error: parent folder for '%s' does not exist.\n", d);
            }
            else if (strcmp(c, "folder_exists") == 0)
            {
                printf("Error: folder '%s' already exists.\n", d);
            }
            else if (strcmp(c, "invalid_folder") == 0)
            {
                printf("Error: invalid folder name '%s'.\n", d);
            }
            else if (strcmp(c, "ambiguous_or_not_found") == 0)
            {
                printf("Error: file '%s' is ambiguous or not found; try specifying a full path.\n", d);
            }
            else if (strcmp(c, "not_found") == 0)
            {
                printf("Error: '%s' not found.\n", d);
            }
            else if (strcmp(c, "no_permission") == 0)
            {
                printf("Permission denied for '%s'.\n", d);
            }
            else if (strcmp(c, "usage") == 0)
            {
                printf("Usage error: %s\n", d);
            }
            else
            {
                /* Fallback: show raw code/detail */
                printf("Error from server: %s (%s)\n", c, d);
            }
        }
        else if (strncmp(response.msg, "UNDO", 4) == 0)
        {
            // Accept both formats:
            // 1) UNDO|UNDO_SUCCESS|filename
            // 2) UNDO_SUCCESS|filename
            char tmp[1024];
            strncpy(tmp, response.msg, sizeof(tmp));
            tmp[sizeof(tmp) - 1] = '\0';
            char *first = strtok(tmp, "|");
            char *second = strtok(NULL, "|");
            char *third = strtok(NULL, "|");

            const char *status = NULL;
            const char *filename = NULL;
            const char *reason = NULL;

            if (first && second && third && strcmp(first, "UNDO") == 0)
            {
                // Format 1
                status = second;
                filename = third;
                reason = strtok(NULL, "|");
            }
            else if (first && second && !third && strncmp(first, "UNDO_", 5) == 0)
            {
                // Format 2: first=UNDO_SUCCESS or UNDO_FAIL
                status = first;
                filename = second;
            }
            else if (first && second && strcmp(first, "UNDO_FAIL") == 0)
            {
                status = first;
                filename = second;
            }

            if (status && strstr(status, "SUCCESS"))
                printf("✅ UNDO successful for file '%s'\n", filename ? filename : "unknown");
            else
                fprintf(stderr, "%s File: %s Reason: %s\n", ERR_UNDO_FAIL, filename ? filename : "unknown", reason ? reason : "unknown");

        }
        else if (strncmp(response.msg, "STREAM|", 7) == 0)
        {
            char *token;
            char buf[1024];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            token = strtok(buf, "|");  // "STREAM"
            token = strtok(NULL, "|"); // filename
            char filename[64];
            strncpy(filename, token, sizeof(filename));

            token = strtok(NULL, "|"); // ss_ip
            char ss_ip[32];
            strncpy(ss_ip, token, sizeof(ss_ip));

            token = strtok(NULL, "|"); // ss_port
            int ss_port = atoi(token);

            token = strtok(NULL, "|"); // inode_no
            int inode_no = atoi(token);

            printf("File '%s' is on Storage Server %s:%d (inode %d)\n",
                   filename, ss_ip, ss_port, inode_no);

            stream_file_from_storage(ss_ip, ss_port, inode_no);

            // Now you can connect to ss_ip:ss_port to read the file
        }
        else if (strncmp(response.msg, "EXEC|", 5) == 0)
        {
            char *token;
            char buf[1024];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            token = strtok(buf, "|");  // "EXEC"
            token = strtok(NULL, "|"); // data
            if(strcmp(token, "BEGIN") == 0)
            {
                printf("Execution started.\n");
                handle_exec_response(sock);
            }
            else if(strcmp(token, "END") == 0)
            {
                printf("Execution ended.\n");
            }
            else
            {
                printf("%s\n", token);
            }
        }
        else if(strncmp(response.msg,"LISTCHECKPOINT|",15)==0)
        {
            char *token;
            char buf[2048];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            token = strtok(buf, "|");  // "LISTCHECKPOINT"
            token = strtok(NULL, "|"); // filename
            token = strtok(NULL, "|"); // count

            int count = atoi(token);
            for(int i=0;i<count;i++)
            {
                token = strtok(NULL, "|"); // tag
                char *tag = token ? token : "unknown";

                token = strtok(NULL, "|");  // timestamp
                char *timestamp = token ? token : "0";

                printf("  Tag: %s, Timestamp: %s\n", tag, pretty_time(timestamp));
                token = NULL;
            }
        }
        else if(strncmp(response.msg, "VIEWCHECKPOINT|", 15) == 0)
        {
            // char *token;
            char buf[1024];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            /* skip the leading token (VIEWCHECKPOINT) */
            strtok(buf, "|");
            char* ip_address = strtok(NULL, "|"); // ss->ip
            char* ss_port_str = strtok(NULL, "|"); // ss_port
            char* filepath = strtok(NULL, "|"); // filepath

            int ss_port = atoi(ss_port_str);
            view_check(ip_address, ss_port, filepath);

        }
        else if(strncmp(response.msg,"REVERT",6)==0)
        {
            // char *token;
            char buf[1024];
            strncpy(buf, response.msg, sizeof(buf));

            // Parse
            /* skip the leading token (REVERT) */
            strtok(buf, "|");
            char* ip_address = strtok(NULL, "|"); // ss->ip
            char* ss_port_str = strtok(NULL, "|"); // ss_port
            char* filepath = strtok(NULL, "|"); // filepath

            int ss_port = atoi(ss_port_str);
            revertc(ip_address, ss_port, filepath);
        }
        else
        {
            // --- Print server’s reply ---
            printf("Server's reply:\n%s\n", response.msg);
        }
    }

    close(sock);
    printf("Client Disconnected.\n");
    return 0;
}
