#include "../main.h"
#include "log.h"
#include "access_control.h"

// Define globals declared in nm.h
Client *clients[MAX_CLIENTS] = {0};
StorageServer *storages[MAX_STORAGE] = {0};
FileMeta *files[MAX_FILES] = {NULL};

// ===== Hash & Cache Tables =====
HashNode *file_hash_table[HASH_SIZE] = {NULL};
CacheEntry cache[CACHE_SIZE];
int cache_count = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t hash_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t files_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connection_client_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connection_storage_lock = PTHREAD_MUTEX_INITIALIZER;

// ----------------- Client Functions -----------------

void remove_file_access_by_client_id(Client* c, int client_id, int fd)
{
    pthread_mutex_lock(&files_mutex);
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i])
        {
            if (strcmp(files[i]->created_by, clients[client_id]->client_name) == 0)
            {
                // file created by this client, delete the file
                //  Implement file deletion logic here
                remove_from_hash(files[i]->filename);
                remove_from_cache(files[i]->filename);
                free_inode_from_storage(files[i]->ss_id, files[i]->inode_no);
                files[i] = NULL;
                free(files[i]);
                continue;
            }
        }
    }
    pthread_mutex_unlock(&files_mutex);
}
int add_client(Client *c)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] && strcmp(clients[i]->client_name, c->client_name) == 0)
        {
            pthread_mutex_unlock(&clients_mutex);
            return -1; // client already exists
        }
    }
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (!clients[i])
        {
            c->client_id = i;
            clients[i] = c;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

void remove_client(Client *c, int client_id, int fd)
{
    pthread_mutex_lock(&clients_mutex);
    remove_file_access_by_client_id(c,client_id,fd);
    clients[client_id] = NULL;
    free(clients[client_id]);
    pthread_mutex_unlock(&clients_mutex);
}

void print_clients()
{
    pthread_mutex_lock(&clients_mutex);
    printf("=== Connected Clients ===\n");
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {
            printf("ID: %d, Name: %s, IP: %s, Port: %d\n",
                   clients[i]->client_id,
                   clients[i]->client_name,
                   clients[i]->ip_address,
                   clients[i]->port);
        }
    }
    printf("=========================\n");
    pthread_mutex_unlock(&clients_mutex);
}

// ----------------- Storage Server Functions -----------------
void add_storage(StorageServer *s)
{
    pthread_mutex_lock(&storage_mutex);

    for (int i = 0; i < MAX_STORAGE; i++)
    {
        if (!storages[i])
        {
            s->ss_id = i;
            storages[i] = s;
            for (int j = 0; j < MAX_FILES; j++)
            {
                if (s->inodes[j] != -1)
                {
                    printf("%d\n", s->inodes[j]);
                    int sp = s->inodes[j];
                    files[sp]->ss_id = i;
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&storage_mutex);
}

void remove_storage(int ss_id)
{
    pthread_mutex_lock(&storage_mutex);
    for (int i = 0; i < MAX_STORAGE; i++)
    {
        if (storages[i] && storages[i]->ss_id == ss_id)
        {
            for (int j = 0; j < MAX_FILES; j++)
            {
                if (files[j] && files[j]->ss_id == ss_id)
                {
                    files[j]->ss_id = -1; // Mark as invalid
                }
            }
            storages[i] = NULL;
            printf("Storage server ID %d removed.\n", ss_id);
            break;
        }
    }
    pthread_mutex_unlock(&storage_mutex);
}

void print_storages()
{
    pthread_mutex_lock(&storage_mutex);
    printf("=== Connected Storage Servers ===\n");
    for (int i = 0; i < MAX_STORAGE; i++)
    {
        if (storages[i] && storages[i]->status == 1)
        {
            printf("ID: %d, IP: %s, Port: %d, UsedBytes: %ld\n",
                   storages[i]->ss_id,
                   storages[i]->ip_address,
                   storages[i]->port,
                   storages[i]->used_bytes);
        }
    }
    printf("===============================\n");
    pthread_mutex_unlock(&storage_mutex);
}

// ----------------- Handler -----------------
void *connection_handler(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    // First, identify type: 0=client, 1=storage
    int type;
    ssize_t n = recv(fd, &type, sizeof(int), 0);
    if (n <= 0)
    {
        close(fd);
        return NULL;
    }

    if (type == 0)
    { // Client
        Client *c = (Client *)malloc(sizeof(Client));
        n = recv(fd, c, sizeof(Client), 0);
        if (n <= 0)
        {
            close(fd);
            free(c);
            return NULL;
        }

        int a = add_client(c);
        if (a == -1)
        {
            Packet errp;
            errp.client_id = -1;
            snprintf(errp.msg, sizeof(errp.msg), "%s", ERR_DUPLICATE_CLIENT);
            send(fd, &errp, sizeof(errp), 0);
            nameserver_log("CLIENT_CONNECT_REJECT name=%s ip=%s port=%d", c->client_name, c->ip_address, c->port);
            close(fd);
            free(c);
            return NULL;
        }
        printf("Client connected: ID=%d, Name=%s, IP=%s, Port=%d\n",
               c->client_id, c->client_name, c->ip_address, c->port);
        nameserver_log("CLIENT_CONNECT id=%d name=%s ip=%s port=%d",
                       c->client_id, c->client_name, c->ip_address, c->port);
        print_clients();
        printf("Assigned Client ID: %d\n", c->client_id);

        if (send(fd, &c->client_id, sizeof(c->client_id), 0) <= 0)
        {
            nameserver_log("ASSIGN_ID_FAIL id=%d name=%s", c->client_id, c->client_name);
            close(fd);
            return NULL;
        }
        nameserver_log("ASSIGNED_CLIENT_ID id=%d name=%s", c->client_id, c->client_name);

        Packet p;
        while (1)
        {
            n = recv(fd, &p, sizeof(p), 0);
            if (n <= 0)
            {
                printf("Client disconnected: ID=%d, Name=%s\n", c->client_id, c->client_name);
                nameserver_log("CLIENT_DISCONNECT id=%d name=%s ip=%s port=%d",
                               c->client_id, c->client_name, c->ip_address, c->port);
                close(fd);
                remove_client(c, c->client_id, fd);
                print_clients();
                break;
            }
            else if (p.client_id != c->client_id)
            {
                continue;
            }
            // sanitize message buffer
            p.msg[sizeof(p.msg) - 1] = '\0';
            p.msg[strcspn(p.msg, "\r\n")] = '\0';
            // printf("%s\n",p.msg);
            fflush(stdout);
            nameserver_log("REQUEST from id=%d name=%s ip=%s port=%d cmd=%s",
                           c->client_id, c->client_name, c->ip_address, c->port, p.msg);
            char *strt;
            strt = strtok(p.msg, " ");
            if (strt && strcmp(strt, "CREATE") == 0)
            {
                char *fname = strtok(NULL, " ");
                if (fname && *fname)
                {
                    int rc = create(c, fname);
                    Packet outp;
                    outp.client_id = c->client_id;
                    if (rc >= 0)
                    {
                        snprintf(outp.msg, sizeof(outp.msg), "%s|%s\n", OK_FILE_CREATED, fname);
                        nameserver_log("CREATE_OK user=%s file=%s", c->client_name, fname);
                    }
                    else if (rc == -2)
                    {
                        /* File already exists */
                        snprintf(outp.msg, sizeof(outp.msg), "ERR|FILE_EXISTS|%s\n", fname);
                        nameserver_log("CREATE_FAIL exists user=%s file=%s", c->client_name, fname);
                    }
                    else if (rc == -3)
                    {
                        /* Parent folder missing */
                        snprintf(outp.msg, sizeof(outp.msg), "ERR|MISSING_PARENT|%s\n", fname);
                        nameserver_log("CREATE_FAIL missing_parent user=%s file=%s", c->client_name, fname);
                    }
                    else
                    {
                        snprintf(outp.msg, sizeof(outp.msg), "%s|%s\n", ERR_CREATE_FAIL, fname);
                        nameserver_log("CREATE_FAIL user=%s file=%s", c->client_name, fname);
                    }
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    // printf("Usage: CREATE <filename>\n");
                    Packet outp;
                    outp.client_id = c->client_id;
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: CREATE <filename>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "VIEW") == 0)
            {
                strt = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (strt == NULL)
                {
                    view(c, &outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else if (strcmp(strt, "-l") == 0)
                {
                    viewl(c, &outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else if (strcmp(strt, "-a") == 0)
                {
                    viewa(&outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else if (strcmp(strt, "-al") == 0)
                {
                    viewal(&outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    p.msg[0] = '\0';
                    snprintf(p.msg, sizeof(p.msg), "Usage: VIEW [-l|-a|-al]\n");
                    send(fd, &p, sizeof(p), 0);
                }
            }
            else if (strt && strcmp(strt, "READ") == 0)
            {
                printf("ok\n");
                char *fname = strtok(NULL, " ");
                printf("file name: %s\n", fname);
                read_file(c, fname, fd);
            }
            else if (strt && strcmp(strt, "WRITE") == 0)
            {
                printf("write command received in the nm side\n");
                char *fname = strtok(NULL, " ");
                char *snum_str = strtok(NULL, " ");
                int sentenceno = snum_str ? atoi(snum_str) : 0;
                printf("Callling write function\n");
                write_file(c, fname, sentenceno, fd);
            }
            else if (strt && strcmp(strt, "UNDO") == 0)
            {
                char *fname = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && *fname)
                {
                    /* delete_file may send an error response itself (not_found/stale).
                     * Only send the OK response from nm.c when delete_file reports success
                     * (non-negative return value). This avoids sending two replies for
                     * the same client request. */
                    // int rc = delete_file(c, fname, fd);
                    // if (rc >= 0)
                    // {
                    //     snprintf(outp.msg, sizeof(outp.msg), "%s|%s\n", OK_DELETE_SUCCESS, fname);
                    //     /* sending response back to client */
                    //     send(fd, &outp, sizeof(outp), 0);
                    // }
                    undo_file(c, fname, &outp, fd);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    // printf("Usage: UNDO <filename>\n");
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: UNDO <filename>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "CHECKPOINT") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *tag = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && tag)
                {
                    checkpoint_file(c, fname, tag, &outp, fd);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: CHECKPOINT <filename> <tag>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && (strcmp(strt, "LISTCHECKPOINT") == 0 || strcmp(strt, "LISTCHECKPOINTS") == 0))
            {
                char *fname = strtok(NULL, " ");
                /* Optional tag parameter (currently ignored for listing all) */
                strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && *fname)
                {
                    list_checkpoints(c, fname, &outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: LISTCHECKPOINT <filename> [tag]\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "VIEWCHECKPOINT") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *tag = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && tag)
                {
                    view_checkpoint(c, fname, tag, &outp, fd);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: VIEWCHECKPOINT <filename> <tag>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "REVERT") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *tag = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && tag)
                {
                    revert(c, fname, tag, &outp, fd);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: REVERT <filename> <tag>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp("INFO", strt) == 0)
            {
                char *fname = strtok(NULL, " ");
                if (!fname)
                {
                    // Packet outp;
                    // outp.client_id = c->client_id;
                    printf("Usage: INFO <filename>\n");
                    snprintf(p.msg, sizeof(p.msg), "Usage: INFO <filename>\n");
                    send(fd, &p, sizeof(p), 0);
                    continue;
                }
                printf("file name: %s\n", fname);
                file_info(c, fname, fd);
            }
            else if (strt && strcmp(strt, "DELETE") == 0)
            {
                char *fname = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && *fname)
                {
                    delete_file(c, fname, fd);
                    snprintf(outp.msg, sizeof(outp.msg), "%s|%s\n", OK_DELETE_SUCCESS, fname);
                    /* sending response back to client */
                    send(fd, &outp, sizeof(outp), 0);
                    /* Free the inode from the storage server */
                }
                else
                {
                    // printf("Usage: DELETE <filename>\n");
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: DELETE <filename>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp("LIST", strt) == 0)
            {
                Packet outp;
                outp.client_id = c->client_id;
                list_files(c, &outp);
                send(fd, &outp, sizeof(outp), 0);
            }
            else if (strt && strcmp("ADDACCESS", strt) == 0)
            {
                char *perm = strtok(NULL, " -");
                char *fname = strtok(NULL, " ");
                char *cname = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (perm && fname && cname)
                {
                    add_access(c, perm, fname, cname, &outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: ADDACCESS <permission> <filename> <clientname>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "CREATEFOLDER") == 0)
            {
                char *folder = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (folder)
                {
                    create_folder(c, folder, &outp);
                }
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: CREATEFOLDER <path>\n");
                }
                send(fd, &outp, sizeof(outp), 0);
            }
            else if (strt && strcmp(strt, "MOVE") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *folder = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && folder)
                {
                    move_file_to_folder(c, fname, folder, &outp);
                }
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: MOVE <filename> <folder>\n");
                }
                send(fd, &outp, sizeof(outp), 0);
            }
            else if (strt && strcmp(strt, "VIEWFOLDER") == 0)
            {
                char *folder = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (folder)
                {
                    view_folder(c, folder, &outp);
                }
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: VIEWFOLDER <folder>\n");
                }
                send(fd, &outp, sizeof(outp), 0);
            }
            else if (strt && strcmp(strt, "REMACCESS") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *cname = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && cname)
                {
                    remove_access(c, fname, cname, &outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: REMACCESS <filename> <clientname>\n");
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "STREAM") == 0)
            {
                printf("in the streaming section\n");
                char *fname = strtok(NULL, " ");
                printf("file name: %s\n", fname);
                stream_file(c, fname, fd);
            }
            else if (strt && strcmp(strt, "EXEC") == 0)
            {
                char *fname = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname && *fname)
                {
                    exec(c, fname, fd);
                }
                else
                {
                    outp.msg[0] = '\0';
                    snprintf(outp.msg, sizeof(outp.msg), "Usage: EXEC <filename>\n");
                    // send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "REQUESTACCESS") == 0)
            {
                printf("In REQUEST ACCESS section\n");
                char *perm = strtok(NULL, " ");
                char *fname = strtok(NULL, " ");
                printf("Permission: %s, File name: %s\n", perm, fname);
                Packet outp;
                outp.client_id = c->client_id;

                if (!perm || !fname)
                {
                    snprintf(outp.msg, sizeof(outp.msg),
                             "Usage: REQUESTACCESS -R|-W|-RW <filename>");
                    send(fd, &outp, sizeof(outp), 0);
                    continue;
                }

                // Normalize perm format for internal use:
                // "-R"  -> "R"
                // "-W"  -> "W"
                // "-RW" -> "RW"

                char norm_perm[4] = "";
                if (strcmp(perm, "-R") == 0)
                    strcpy(norm_perm, "R");
                else if (strcmp(perm, "-W") == 0)
                    strcpy(norm_perm, "W");
                else if (strcmp(perm, "-RW") == 0)
                    strcpy(norm_perm, "RW");
                else
                {
                    snprintf(outp.msg, sizeof(outp.msg),
                             "Invalid permission flag. Use -R, -W, or -RW.");
                    send(fd, &outp, sizeof(outp), 0);
                    continue;
                }
                pthread_mutex_lock(&files_mutex);
                FileMeta *maybe_file = find_file_by_path(fname);
                if (!maybe_file)
                {
                    pthread_mutex_unlock(&files_mutex);
                    /* Notify server operator and reply to client */
                    printf("REQUESTACCESS: client '%s' requested file '%s' which does not exist\n", c->client_name, fname);
                    snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", fname);
                    nameserver_log("REQUESTACCESS_FAIL user=%s file=%s reason=not_found", c->client_name, fname);
                    send(fd, &outp, sizeof(outp), 0);
                    continue;
                }
                pthread_mutex_unlock(&files_mutex);

                printf("DEBUG: Requesting %s access for file: %s by client:%s\n", norm_perm, fname, c->client_name);
                request_access(c, norm_perm, fname, &outp);
                send(fd, &outp, sizeof(outp), 0);
            }
            else if (strt && strcmp(strt, "VIEWREQUESTS") == 0)
            {
                char *fname = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                if (fname == NULL)
                {
                    /* If no filename supplied, aggregate pending requests for all files
                     * owned by this client and return them. This avoids calling
                     * view_requests() with a NULL filename which caused '(null)'. */
                    pthread_mutex_lock(&files_mutex);
                    char buf[4096] = "";
                    int found = 0;
                    for (int i = 0; i < MAX_FILES; i++)
                    {
                        if (!files[i])
                            continue;
                        if (strcmp(files[i]->created_by, c->client_name) != 0)
                            continue;
                        AccessRequest *r = files[i]->pending_requests;
                        while (r)
                        {
                            char line[512];
                            snprintf(line, sizeof(line), "Filename:%s Requester:%s Permission-requested:%s\n", files[i]->filename, r->requester, r->perm);
                            strncat(buf, line, sizeof(buf) - strlen(buf) - 1);
                            found = 1;
                            r = r->next;
                        }
                    }
                    pthread_mutex_unlock(&files_mutex);
                    if (!found)
                    {
                        snprintf(outp.msg, sizeof(outp.msg), "No pending requests found\n");
                    }
                    else
                    {
                        strncpy(outp.msg, buf, sizeof(outp.msg) - 1);
                        outp.msg[sizeof(outp.msg) - 1] = '\0';
                    }
                    send(fd, &outp, sizeof(outp), 0);
                }
                else
                {
                    pthread_mutex_lock(&files_mutex);
                    FileMeta *maybe_file = find_file_by_path(fname);
                    if (!maybe_file)
                    {
                        pthread_mutex_unlock(&files_mutex);
                        snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", fname);
                        nameserver_log("REQUESTACCESS_FAIL user=%s file=%s reason=not_found",
                                       c->client_name, fname);
                        send(fd, &outp, sizeof(outp), 0);
                        continue;
                    }
                    pthread_mutex_unlock(&files_mutex);

                    printf("DEBUG: Viewing requests for file: %s to the client: %s\n", fname, c->client_name);
                    view_requests(c, fname, &outp);
                    send(fd, &outp, sizeof(outp), 0);
                }
            }
            else if (strt && strcmp(strt, "APPROVE") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *uname = strtok(NULL, " ");
                char *perm = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                printf("DEBUG: Approving access for file: %s to user: %s by client:%s perm=%s\n", fname, uname, c->client_name, perm ? perm : "(any)");
                approve_request(c, fname, uname, perm, &outp);
                send(fd, &outp, sizeof(outp), 0);
            }
            else if (strt && strcmp(strt, "DENY") == 0)
            {
                char *fname = strtok(NULL, " ");
                char *uname = strtok(NULL, " ");
                char *perm = strtok(NULL, " ");
                Packet outp;
                outp.client_id = c->client_id;
                printf("DEBUG: Denying access for file: %s to user: %s by client:%s perm=%s\n", fname, uname, c->client_name, perm ? perm : "(any)");
                deny_request(c, fname, uname, perm, &outp);
                send(fd, &outp, sizeof(outp), 0);
            }

            else
                printf("else\n");
            printf("[%s]: %s\n", c->client_name, p.msg);
        }
    }
    else if (type == 1)
    { // Storage Server
        StorageServer *s = (StorageServer *)malloc(sizeof(StorageServer));
        n = recv(fd, s, sizeof(StorageServer), 0);
        if (n <= 0)
        {
            close(fd);
            free(s);
            return NULL;
        }

        s->fd = fd;
        // printf("1\n");

        // Override reported IP with the actual peer IP to ensure clients can connect
        do
        {
            struct sockaddr_in peer;
            socklen_t plen = sizeof(peer);
            if (getpeername(fd, (struct sockaddr *)&peer, &plen) == 0)
            {
                char ipbuf[32];
                if (inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf)))
                {
                    strncpy(s->ip_address, ipbuf, sizeof(s->ip_address) - 1);
                    s->ip_address[sizeof(s->ip_address) - 1] = '\0';
                }
            }
        } while (0);

        // printf("2\n");
        pthread_mutex_init(&s->lock, NULL);
        add_storage(s);
        printf("Storage Server connected: ID=%d, IP=%s, Port=%d, Used=%ld\n",
               s->ss_id, s->ip_address, s->port, s->used_bytes);
        nameserver_log("STORAGE_CONNECT id=%d ip=%s port=%d used=%ld",
                       s->ss_id, s->ip_address, s->port, s->used_bytes);
        print_storages();

        printf("Assigned Storage Server ID: %d\n", s->ss_id);
        int id_net = htonl(s->ss_id);
        ssize_t sent = send(fd, &id_net, sizeof(id_net), 0);
        if (sent != sizeof(id_net))
        {
            printf("Failed to send storage server ID\n");
            close(fd);
            return NULL;
        }

        char buf[1024];
        while (1)
        {
            n = recv(fd, buf, 1, MSG_PEEK);

            if (n == 0)
            {
                // Clean disconnect
                printf("Storage Server disconnected: ID=%d\n", s->ss_id);
                nameserver_log("STORAGE_DISCONNECT id=%d ip=%s port=%d",
                               s->ss_id, s->ip_address, s->port);
                close(fd);
                remove_storage(s->ss_id);
                print_storages();
                break;
            }
            else if (n < 0)
            {
                // Error
                perror("Heartbeat recv");
                close(fd);
                remove_storage(s->ss_id);
                break;
            }

            // n > 0: Connection alive, continue monitoring
            sleep(1);
        }
    }
    else
    {
        printf("Unknown connection type\n");
        close(fd);
    }

    return NULL;
}

// ----------------- Main -----------------
int main()
{
    char server_ip[32];
    int server_port;

    printf("Enter server IP to bind: ");
    scanf("%31s", server_ip);
    printf("Enter server port to bind: ");
    scanf("%d", &server_port);
    nameserver_log("Nameserver started on %s:%d", server_ip, server_port);
    // if(load_state("nm_state.dat") == 0) {
    //     printf("State loaded successfully from nm_state.dat\n");
    // } else {
    //     printf("No previous state found or failed to load state.\n");
    // }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0)
    {
        perror("Invalid IP");
        exit(1);
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }
    if (listen(listen_fd, 128) < 0)
    {
        perror("listen");
        exit(1);
    }

    printf("Server listening on %s:%d\n", server_ip, server_port);

    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;
        pthread_t th;
        pthread_create(&th, NULL, connection_handler, pclient);
        pthread_detach(th);

        // save_state("nm_state.dat");
    }

    close(listen_fd);
    return 0;
}
