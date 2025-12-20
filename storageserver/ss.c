#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../main.h"
#include "log.h"
#include "../error_codes.h"
#include"write.h"

int ss_id = -1;
StorageServer s;

int rename_storage_directory(const char *old_dir, const char *new_dir)
{
    if (!old_dir || !new_dir) {
        fprintf(stderr, "rename_storage_directory: NULL path received\n");
        return -1;
    }

    // Attempt to rename
    if (rename(old_dir, new_dir) == 0) {
        printf("📁 Successfully renamed storage directory: %s → %s\n",
               old_dir, new_dir);
        storageserver_log("RENAME_OK old=%s new=%s", old_dir, new_dir);
        return 0;
    }

    // Error handling
    fprintf(stderr, "❌ Failed to rename directory '%s' → '%s': %s\n",
            old_dir, new_dir, strerror(errno));
    storageserver_log("RENAME_FAIL old=%s new=%s err=%s", old_dir, new_dir, strerror(errno));

    return -1;
}

int remove_directory_recursive(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        perror("opendir");
        storageserver_log("RMDIR_OPENDIR_FAIL path=%s err=%s", path, strerror(errno));
        return -1;
    }

    struct dirent *entry;
    char filepath[1024];

    while ((entry = readdir(d)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively delete subdirectory
                remove_directory_recursive(filepath);
            } else {
                // Remove file
                remove(filepath);
            }
        }
    }

    closedir(d);

    // Finally remove the directory itself
    if (rmdir(path) == 0) {
        printf("Deleted directory: %s\n", path);
        storageserver_log("DIR_DELETED path=%s", path);
        return 0;
    }
    perror("rmdir");
    storageserver_log("RMDIR_FAIL path=%s err=%s", path, strerror(errno));
    return -1;
}


unsigned char* read_file_bytes(const char *path, long *size_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;

    // Find file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    // Allocate buffer
    unsigned char *buffer = malloc(size);
    if (!buffer) {
        fclose(fp);
        storageserver_log("READ_OOM path=%s size=%ld", path, size);
        return NULL;
    }

    // Read bytes
    fread(buffer, 1, size, fp);
    fclose(fp);

    *size_out = size;
    return buffer;
}

void add_inode_to_storage(StorageServer *ss, int inode_no) {
    // pthread_mutex_lock(&storage_mutex);
    for (int i = 0; i < MAX_FILES; i++) {
        if (ss->inodes[i] == -1) {
            ss->inodes[i] = inode_no;
            storageserver_log("INODE_ADD ss_id=%d inode=%d slot=%d", ss_id, inode_no, i);
            break;
        }
    }
    // pthread_mutex_unlock(&storage_mutex);
}

void free_inode_from_storage(StorageServer *ss, int inode_no) {
    // pthread_mutex_lock(&storage_mutex);
        for (int i = 0; i < MAX_FILES; i++) {
            if (ss->inodes[i] == inode_no) {
                ss->inodes[i] = -1; // Mark as free
                storageserver_log("INODE_FREE ss_id=%d inode=%d slot=%d", ss_id, inode_no, i);
                break;
            }
        }
    // pthread_mutex_unlock(&storage_mutex);
}

// Function to create directories and touch files
int mkdir_and_touch(const char *path)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);

    char *last_slash = strrchr(tmp, '/');
    if (last_slash)
    {
        *last_slash = '\0';
        char dir_tmp[1024];
        snprintf(dir_tmp, sizeof(dir_tmp), "%s", tmp);
        char *p;
        for (p = dir_tmp + 1; *p; p++)
        {
            if (*p == '/')
            {
                *p = '\0';
                if (mkdir(dir_tmp, 0755) != 0 && errno != EEXIST)
                {
                            perror("mkdir failed");
                            storageserver_log("MKDIR_FAIL path=%s err=%s", dir_tmp, strerror(errno));
                            return -1;
                }
                *p = '/';
            }
        }
        if (mkdir(dir_tmp, 0755) != 0 && errno != EEXIST)
        {
            perror("mkdir failed");
            storageserver_log("MKDIR_FAIL path=%s err=%s", dir_tmp, strerror(errno));
            return -1;
        }
    }

    FILE *fp = fopen(path, "a");
    if (!fp)
    {
        perror("fopen failed");
        storageserver_log("TOUCH_FAIL path=%s err=%s", path, strerror(errno));
        return -1;
    }
    fclose(fp);
    storageserver_log("MKDIR_TOUCH_OK path=%s", path);
    return 0;
}

// Thread function to handle a single client connection
void *handle_client(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);

    printf("✅ Client connected!\n");
    storageserver_log("CLIENT_CONNECT fd=%d", client_sock);

    char buf[1024];
    while (1)
    {
        ssize_t n = recv(client_sock, buf, sizeof(buf) - 1, 0);
            if (n > 0)
        {
            buf[n] = '\0';
            printf("📥 Message from client: %s\n", buf);
            storageserver_log("CLIENT_CMD fd=%d cmd=\"%s\"", client_sock, buf);
            if (strncmp(buf, "READ ", 5) == 0)
            {
                // Parse inode number
                char *token = strtok(buf, " ");
                token = strtok(NULL, " ");
                if (!token)
                {
                    char err[] = "Invalid READ command\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("READ_INVALID_CMD fd=%d", client_sock);
                    continue;
                }
                int inode_no = atoi(token);
                // Construct the path to file based on how CREATE made it: "<ss_id>/<inode>.txt"
                char filepath[256];
                if (ss_id >= 0)
                    snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
                else
                    snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);
                printf("📂 Reading file for inode %d -> %s\n", inode_no, filepath);
                storageserver_log("READ_START inode=%d filepath=%s by_fd=%d", inode_no, filepath, client_sock);
                // Send file content to client
                send_file_to_client(client_sock, filepath);
                // Optionally send EOF marker
                const char *endmsg = "\n<EOF>\n";
                send(client_sock, endmsg, strlen(endmsg), 0);
                printf("✅ File sent to client (inode %d)\n", inode_no);
                storageserver_log("READ_SENT inode=%d filepath=%s to_fd=%d", inode_no, filepath, client_sock);
                break; // close after one read (client will reconnect for
            }
            else if (strncmp(buf, "WRITE", 5) == 0)
            {
                // Handle WRITE command
                char *token = strtok(buf, " ");
                token = strtok(NULL, " ");
                if (!token)
                {
                    char err[] = "Invalid WRITE command\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("WRITE_INVALID_CMD fd=%d", client_sock);
                    continue;
                }
                printf("%s\n", token);
                int inode_no = atoi(token);

                token = strtok(NULL, " "); // sentence_no
                int sentence_no = token ? atoi(token) : 0;
                // Construct the path to file based on how CREATE made it: "<ss_id>/<inode>.txt"
                char filepath[256];
                if (ss_id >= 0)
                    snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
                else
                    snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);
                printf("📂 Writing file for inode %d -> %s\n", inode_no, filepath);
                storageserver_log("WRITE_REQUEST inode=%d filepath=%s from_fd=%d sentence=%d", inode_no, filepath, client_sock, sentence_no);
                // Receive file content from client
                write_file_to_client(filepath, sentence_no, client_sock);
                save_storage_server(&s, "backup.txt", MAX_FILES);
                storageserver_log("WRITE_COMPLETED inode=%d filepath=%s", inode_no, filepath);
                break;
            }
            else if (strncmp(buf, "STREAM", 6) == 0)
            {
                // Parse inode number
                char *token = strtok(buf, " ");
                token = strtok(NULL, " ");
                if (!token)
                {
                    char err[] = "Invalid STREAM command\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("STREAM_INVALID_CMD fd=%d", client_sock);
                    continue;
                }
                int inode_no = atoi(token);
                // Construct the path to file based on how CREATE made it: "<ss_id>/<inode>.txt"
                char filepath[256];
                if (ss_id >= 0)
                    snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
                else
                    snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);
                printf("📂 Streaming file for inode %d -> %s\n", inode_no, filepath);
                storageserver_log("STREAM_REQUEST inode=%d filepath=%s from_fd=%d", inode_no, filepath, client_sock);
                // Send file content to client
                send_file_to_client_in_stream_mode(client_sock, filepath);
                // Optionally send EOF marker
                const char *endmsg = "\n<EOF>\n";
                send(client_sock, endmsg, strlen(endmsg), 0);
                printf("✅ File sent to client (inode %d)\n", inode_no);
                storageserver_log("STREAM_SENT inode=%d filepath=%s to_fd=%d", inode_no, filepath, client_sock);
                break; // close after one read (client will reconnect for
            }
            else if(strncmp(buf,"READ_CHECKPOINT",15)==0){
                char* token = strtok(buf, " ");
                token = strtok(NULL, " ");
                if (!token)
                {
                    char err[] = "Invalid READ_CHECKPOINT command\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("READ_CHECKPOINT_INVALID_CMD fd=%d", client_sock);
                    continue;
                }
                send_file_to_client(client_sock, token);
                storageserver_log("READ_CHECKPOINT_SENT filepath=%s to_fd=%d", token, client_sock);
                break;
            }
            else if(strncmp(buf,"REVERT",6)==0){
                char *cmd = strtok(buf, " ");       // "REVERT"
                char *chkpath = strtok(NULL, " ");    // "a/b_c.txt.chk"
                if (!chkpath)
                {
                    char err[] = "Invalid REVERT command\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("REVERT_INVALID_CMD fd=%d", client_sock);
                    break;
                }
                if(access(chkpath, F_OK ) == -1 ) {
                    char err[] = "Checkpoint file does not exist\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("REVERT_CHKPT_NOT_EXIST filepath=%s to_fd=%d", chkpath, client_sock);
                    break;
                }
                char original[512];
                // c/ 1. Copy full path
                    strncpy(original, chkpath, sizeof(original));
                    original[sizeof(original)-1] = '\0';

                    // 2. Find last '/' to separate directory
                    char *slash = strrchr(original, '/');
                    char *filename = slash ? slash + 1 : original;

                    // 3. Find '_' inside filename
                    char *underscore = strchr(filename, '_');
                    if (!underscore) {
                        printf("Invalid checkpoint format\n");
                        return;
                    }

                    // 4. Extract right side (tag)
                    char *tag = underscore + 1;

                    // 5. Remove .chk (and .txt.chk)
                    char *dot = strstr(tag, ".txt.chk");
                    if (dot)
                        *dot = '\0';

                    // 6. Build final original file path
                    if (slash) {
                        // keep directory
                        *(slash + 1) = '\0';       // keep "a/"
                        snprintf(original + strlen(original),
                                 sizeof(original) - strlen(original),
                                 "%s.txt", tag);   // append c.txt
                    } else {
                        // no directory
                        snprintf(original, sizeof(original), "%s.txt", tag);
                    }
                printf("Reverting file %s to %s\n", original, chkpath);
                storageserver_log("REVERT_START filepath=%s to_fd=%d", original, client_sock);
                if(!copy_file(chkpath,original)){
                    char err[] = "Revert failed\n";
                    send(client_sock, err, strlen(err), 0);
                    storageserver_log("REVERT_FAIL filepath=%s to_fd=%d", original, client_sock);
                    break;
                }
                storageserver_log("REVERT_SENT filepath=%s to_fd=%d", original, client_sock);
                break;
            }
            else
            {
                char err[] = "Unknown command\n";
                send(client_sock, err, strlen(err), 0);
                storageserver_log("UNKNOWN_CMD fd=%d cmd=\"%s\"", client_sock, buf);
                break;
            }
        }
    }

    close(client_sock);
    printf("Client disconnected.\n");
    return NULL;
}

// Thread function to listen for clients on storage server port
void *client_listener(void *arg)
{
    int port = *(int *)arg;

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 10) < 0)
    {
        perror("listen failed");
        close(listen_sock);
        return NULL;
    }

    printf("Listening for clients on port %d...\n", port);
    storageserver_log("CLIENT_LISTENER start port=%d", port);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addrlen);
        if (*client_sock < 0)
        {
            perror("accept failed");
            storageserver_log("CLIENT_ACCEPT_FAIL err=%s", strerror(errno));
            free(client_sock);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_sock);
        pthread_detach(tid);
        storageserver_log("CLIENT_ACCEPT fd=%d", *client_sock);
    }

    close(listen_sock);
    return NULL;
}

int main()
{
    char server_ip[32];
    int server_port;

    printf("Enter main server IP: ");
    scanf("%31s", server_ip);
    int ci;
    while ((ci = getchar()) != '\n' && ci != EOF)
        ;

    printf("Enter main server port: ");
    scanf("%d", &server_port);
    while ((ci = getchar()) != '\n' && ci != EOF)
        ;

    // Connect to main server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid IP");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect failed");
        close(sock);
        exit(1);
    }

    printf("Connected to main server!\n");

    int type = 1; // storage server
    if (send(sock, &type, sizeof(int), 0) <= 0)
    {
        perror("send type failed");
        close(sock);
        exit(1);
    }
    // printf("it waits\n");

    char* path="backup.txt";
    StorageServer* s1=load_storage_server(path);
    memset(&s, 0, sizeof(s));
    printf("Enter storage server IP: ");
    scanf("%31s", s.ip_address);
    printf("Enter storage server port: ");
    scanf("%d",&s.port);

    int prev_ss_id=-1;

    if(s1){
        prev_ss_id=s1->ss_id;
        s.used_bytes=s1->used_bytes;
        s.status=1; // online
        s.fd = -1;  // fd cannot be sent
        memcpy(s.inodes, s1->inodes, sizeof(s.inodes));
        if(s.inodes){
            printf("Loaded inodes from backup:\n");
            for(int i=0;i<MAX_FILES;i++){
                if(s.inodes[i]!=-1){
                    printf("%d\n",s.inodes[i]);
                }
            }
        }
    }  
    else{
        s.used_bytes=0;
        s.status=1; // online
        s.fd = -1;  // fd cannot be sent
        // s.inodes = (int *)malloc(MAX_FILES * sizeof(int));
        for (int i = 0; i < MAX_FILES; i++) {
            s.inodes[i] = -1; // Initialize to -1 indicating empty slots
        }
    } 

    if (send(sock, &s, sizeof(s), 0) <= 0)
    {
        perror("send struct failed");
        close(sock);
        exit(1);
    }

    printf("Storage server info sent!\n");

    int m_net;
    ssize_t rn = recv(sock, &m_net, sizeof(m_net), 0);
    if (rn != sizeof(m_net))
    {
        printf("Failed to receive assigned storage server ID. Server disconnected?\n");
        close(sock);
        exit(1);
    }
    s.ss_id = ntohl(m_net);
    printf("Assigned Storage Server ID from server: %d\n", s.ss_id);
    ss_id = s.ss_id;

    if(prev_ss_id!=-1 && prev_ss_id!=s.ss_id){
        // Rename directory from prev_ss_id to new ss_id
        char old_dir[256];
        char new_dir[256];
        snprintf(old_dir, sizeof(old_dir), "%d", prev_ss_id);
        snprintf(new_dir, sizeof(new_dir), "%d", s.ss_id);
        if (rename(old_dir, new_dir) == 0) {
            printf("Renamed storage directory from %s to %s\n", old_dir, new_dir);
        } else {
            perror("Failed to rename storage directory");
        }
    }

    // Now that ss_id is known, start the client listener
    pthread_t listener_tid;
    pthread_create(&listener_tid, NULL, client_listener, &s.port);
    pthread_detach(listener_tid);

    // Main connection loop to central server
    char buf[1024];
    while (1)
    {
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
        {
            if (n == 0)
                printf("Server closed connection.\n");
            else
                perror("recv");

            // DELETE YOUR FILE HERE
            printf("Name server disconnected. Cleaning up...\n");
            remove("backup.txt");   // or whatever file(s) you want removed

            remove_directory_recursive(ss_id>=0 ? (char[]){ss_id+'0', '\0'} : ""); // remove storage directory
            break;
        }

        char *start = buf;
        char *end;

        // Trim leading spaces/tabs
        while (*start == ' ' || *start == '\t')
            start++;

        // Trim trailing CR/LF/spaces
        end = start + strlen(start) - 1;
        while (end >= start && (*end == '\n' || *end == '\r' || *end == ' '))
        *end-- = '\0';
    
        printf("📥 Message from main server: %s\n", buf);

    if (strncmp(start, "CREATE ", 7) == 0)
        {
            char *dir_name = start + 7;
            // trim trailing whitespace/newline (already stripped above, but be safe)
            size_t len = strlen(dir_name);
            while (len > 0 && (dir_name[len-1] == '\n' || dir_name[len-1] == '\r' || dir_name[len-1] == ' '))
            {
                dir_name[--len] = '\0';
            }

            if (mkdir_and_touch(dir_name) == 0)
                printf("✅ Directory '%s' created successfully.\n", dir_name);
            else
                printf("❌ Failed to create directory '%s'\n", dir_name);

            char newpath[256];
            strcpy(newpath, dir_name);
            strcat(newpath,".bak");

            printf("📂 Creating inode file -> %s\n", newpath);
            mkdir_and_touch(newpath);
            int id;
            sscanf(dir_name, "%*[^/]/%d.txt", &id);
            printf("%d\n", id);

            add_inode_to_storage(&s, id);                                                                   
            int count=0;
            for(int i=0;i<MAX_FILES;i++){
                if(s.inodes[i]!=-1){
                    count++;
                }
            }
            printf("count- %d, id- %d\n",count,id);
            save_storage_server(&s, "backup.txt", count);
        }
    else if (strncmp(start, "UNDO", 4) == 0)
        {
            // Parse command safely: "UNDO <inode>"
            // Use strtok on a copy because strtok modifies string
            char tmpbuf[1024];
            strncpy(tmpbuf, start, sizeof(tmpbuf)-1);
            tmpbuf[sizeof(tmpbuf)-1] = '\0';

            strtok(tmpbuf, " \t\r\n");
            char *arg = strtok(NULL, " \t\r\n");

            if (!arg) {
                const char *err = "UNDO_FAIL|invalid_args\n";
                p1 p;
                p.id=0;
                strcpy(p.msg,err);
                send(sock, &p, sizeof(p), 0);
                printf("❌ [NS] Invalid UNDO command received: '%s'\n", buf);
                continue;
            }

            int inode_no = atoi(arg);
            char filepath[256];
            if (ss_id >= 0)
                snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
            else
                snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);

            printf("↩️ [NS] Undoing last change for inode %d -> %s\n", inode_no, filepath);

            char backup[256];
            get_backup_path(filepath, backup); // assume this fills backup path string

            if (access(backup, F_OK) != 0)
            {
                const char *msg = "UNDO_FAIL|no_backup\n";
                p1 p;
                p.id=0;
                strcpy(p.msg,msg);
                send(sock, &p, sizeof(p), 0);
                printf("❌ [NS] Undo failed for inode %d (no backup: %s)\n", inode_no, backup);
                continue;
            }

            if (undo_last_change(filepath) == 1)
            {
                const char *msg = "UNDO_SUCCESS\n";
                p1 p;
                p.id=0;
                strcpy(p.msg,msg);
                send(sock, &p, sizeof(p), 0);
                printf("✅ [NS] Undo successful for inode %d\n", inode_no);
                long size;
                unsigned char* data=read_file_bytes(filepath, &size);
                s.used_bytes -= size;
                save_storage_server(&s, "backup.txt", MAX_FILES);
            }
            else
            {
                const char *msg = "UNDO_FAIL|copy_error\n";
                p1 p;
                p.id=0;
                strcpy(p.msg,msg);
                send(sock, &p, sizeof(p), 0);
                printf("❌ [NS] Undo failed for inode %d (copy error)\n", inode_no);
            }
        }
         else if (strncmp(start, "EXEC", 4) == 0)
        {
            // Parse inode number
            char *token = strtok(start, " ");
            token = strtok(NULL, " ");
            if (!token)
            {
                p1 p;
                char err[] = "Invalid EXEC command\n";
                p.id=0;
                strcpy(p.msg,err);
                send(sock, &p, sizeof(p), 0);
                continue;
            }
            int inode_no = atoi(token);
            // Construct the path to file based on how CREATE made it: "<ss_id>/<inode>.txt"
            char filepath[256];
            if (ss_id >= 0)
                snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
            else
                snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);
            printf("📂 Executing file for inode %d -> %s\n", inode_no, filepath);
            // Send file content to nameserver
            send_file_to_nm(sock, filepath);
        }
        else if(strncmp(start, "DELETE", 6) == 0)
        {
            char *token = strtok(start, " ");
            token = strtok(NULL, " ");
            if (!token)
            {
                char err[] = "Invalid DELETE command\n";
                p1 p;
                p.id = 0;
                memset(p.msg, 0, sizeof(p.msg));
                size_t _n = strlen(err);
                if (_n > sizeof(p.msg) - 1) _n = sizeof(p.msg) - 1;
                memcpy(p.msg, err, _n);
                p.msg[_n] = '\0';
                send(sock, &p, sizeof(p), 0);
                continue;
            }
            int inode_no = atoi(token);
            char filepath[256];
            char backup[256];
            
            if (ss_id >= 0)
                snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
            else
                snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);
            get_backup_path(filepath, backup);
            if (remove(filepath) == 0 && remove(backup) == 0)
            {
                printf("✅ File deleted successfully: %s\n", filepath);
                char success[] = "DELETE_SUCCESS\n";
                p1 p;
                p.id = 0;
                memset(p.msg, 0, sizeof(p.msg));
                size_t _n = strlen(success);
                if (_n > sizeof(p.msg) - 1) _n = sizeof(p.msg) - 1;
                memcpy(p.msg, success, _n);
                p.msg[_n] = '\0';
                send(sock, &p, sizeof(p), 0);

                free_inode_from_storage(&s, inode_no);
                save_storage_server(&s, "backup.txt", MAX_FILES);
            }
            else
            {
                perror("File deletion failed");
                char fail[] = "DELETE_FAIL\n";
                p1 p;
                p.id = 0;
                memset(p.msg, 0, sizeof(p.msg));
                size_t _n2 = strlen(fail);
                if (_n2 > sizeof(p.msg) - 1) _n2 = sizeof(p.msg) - 1;
                memcpy(p.msg, fail, _n2);
                p.msg[_n2] = '\0';
                send(sock, &p, sizeof(p), 0);
            }
        }
        else if (strncmp(start, "CHECKPOINT", 10) == 0)
        {
            // Format: CHECKPOINT <inode> <tag>
            char tmp[1024];
            strncpy(tmp, start, sizeof(tmp)-1);
            tmp[sizeof(tmp)-1] = '\0';

            strtok(tmp, " \t\r\n");
            char *arg_inode = strtok(NULL, " \t\r\n");
            char *arg_tag = strtok(NULL, " \t\r\n");

            if (!arg_inode || !arg_tag) {
                const char *err = "CHECKPOINT_FAIL|invalid_args\n";
                p1 p;
                p.id = 0;
                memset(p.msg, 0, sizeof(p.msg));
                size_t _m = strlen(err);
                if (_m > sizeof(p.msg) - 1) _m = sizeof(p.msg) - 1;
                memcpy(p.msg, err, _m);
                p.msg[_m] = '\0';
                send(sock, &p, sizeof(p), 0);
                printf("❌ [NS] Invalid CHECKPOINT command received: '%s'\n", start);
                continue;
            }

            int inode_no = atoi(arg_inode);
            char filepath[256];
            if (ss_id >= 0)
                snprintf(filepath, sizeof(filepath), "%d/%d.txt", ss_id, inode_no);
            else
                snprintf(filepath, sizeof(filepath), "%d.txt", inode_no);

            // Destination checkpoint file path: <ss_id>/<inode>_<tag>.txt.chk
            char destpath[256];
            if (ss_id >= 0)
                snprintf(destpath, sizeof(destpath), "%d/%d_%s.txt.chk", ss_id, inode_no, arg_tag);
            else
                snprintf(destpath, sizeof(destpath), "%d_%s.txt.chk", inode_no, arg_tag);

            printf("🧷 [NS] Creating checkpoint for inode %d -> %s (tag=%s)\n", inode_no, destpath, arg_tag);

            // Touch dest directory/file (ensure directory exists)
            if (mkdir_and_touch(destpath) != 0) {
                const char *msg = "CHECKPOINT_FAIL|touch_fail\n";
                p1 p;
                p.id = 0;
                memset(p.msg, 0, sizeof(p.msg));
                size_t _m2 = strlen(msg);
                if (_m2 > sizeof(p.msg) - 1) _m2 = sizeof(p.msg) - 1;
                memcpy(p.msg, msg, _m2);
                p.msg[_m2] = '\0';
                send(sock, &p, sizeof(p), 0);
                printf("❌ [NS] CHECKPOINT touch failed: %s\n", destpath);
                continue;
            }

            // Copy contents from source file to checkpoint file
            if (!copy_file(filepath, destpath)) {
                const char *msg = "CHECKPOINT_FAIL|copy_error\n";
                p1 p;
                p.id = 0;
                memset(p.msg, 0, sizeof(p.msg));
                size_t _m3 = strlen(msg);
                if (_m3 > sizeof(p.msg) - 1) _m3 = sizeof(p.msg) - 1;
                memcpy(p.msg, msg, _m3);
                p.msg[_m3] = '\0';
                send(sock, &p, sizeof(p), 0);
                printf("❌ [NS] CHECKPOINT copy failed: %s -> %s\n", filepath, destpath);
                continue;
            }

            const char *ok = "CHECKPOINT_SUCCESS\n";
            p1 p;
            p.id = 0;
            memset(p.msg, 0, sizeof(p.msg));
            size_t _ok = strlen(ok);
            if (_ok > sizeof(p.msg) - 1) _ok = sizeof(p.msg) - 1;
            memcpy(p.msg, ok, _ok);
            p.msg[_ok] = '\0';
            if (send(sock, &p, sizeof(p), 0) < 0) {
                perror("send checkpoint success failed");
            }
            printf("✅ [NS] CHECKPOINT created: %s\n", destpath);
        }
        else
        {
            char err[] = "Unknown command\n";
            p1 p;
            p.id = 0;
            memset(p.msg, 0, sizeof(p.msg));
            size_t _unk = strlen(err);
            if (_unk > sizeof(p.msg) - 1) _unk = sizeof(p.msg) - 1;
            memcpy(p.msg, err, _unk);
            p.msg[_unk] = '\0';
            send(sock, &p, sizeof(p), 0);
        }
    }

    close(sock);
    printf("Disconnected.\n");
    return 0;
}
