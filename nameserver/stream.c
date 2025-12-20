#include "stream.h"
#include "log.h"

void stream_file(Client *c, const char *filename, int fd)
{
    // Basic sanity checks
    if (!filename) {
        printf("stream_file: filename is NULL\n");
        return;
    }

    // Step 1: Resolve the file (supports folder-qualified names)
    printf("Looking up file: %s\n", filename);
    nameserver_log("STREAM_REQUEST user=%s id=%d ip=%s port=%d file=%s",
                   c->client_name, c->client_id, c->ip_address, c->port, filename);
    FileMeta *file = find_file_by_path(filename);
    printf("Resolve result: %p\n", (void *)file);
    if (!file)
    {
        printf("%s Filename:%s\n", ERR_NOT_FOUND, filename ? filename : "(null)");
        nameserver_log("STREAM_FAIL not_found user=%s file=%s", c->client_name, filename ? filename : "(null)");
        Packet outp; outp.client_id = c->client_id; snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", filename ? filename : "");
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // Validate that the returned pointer points to an entry in the global files[]
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i] == file) { valid = 1; break; }
    }
    if (!valid) {
        printf("Warning: lookup returned a pointer (%p) not found in files[] — possible stale/dangling pointer.\n",
               (void *)file);
        Packet outp; outp.client_id = c->client_id; snprintf(outp.msg, sizeof(outp.msg), "ERR|stale|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // printf("File found: inode %d on SS %d\n", file->inode_no, file->ss_id);
    int ss_id = file->ss_id;
    if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id])
    {
        printf("%s SS_ID:%d\n", ERR_SS_UNAVAILABLE, ss_id);
        Packet outp; outp.client_id = c->client_id; snprintf(outp.msg, sizeof(outp.msg), "ERR|ss_unavailable|%d", ss_id);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }
    StorageServer *ss = storages[ss_id];
    printf("Storage Server Info - ID: %d, IP: %s, Port: %d\n",
           ss->ss_id, ss->ip_address, ss->port);
    printf("File '%s' is on Storage Server ID %d\n", filename, ss_id);
    printf("sending response with the storage server\n");
    // Step 2: Prepare response with storage server info
    Packet outp;
    outp.client_id = c->client_id;
    FileOwnerPerm *owner = file->owners;
    int has_permission = 0;
    while (owner) {
        // Match by stable client_name first; fall back to client_id for legacy entries
        if (strcmp(owner->client_name, c->client_name) == 0 || owner->client_id == c->client_id) {
            if (strchr(owner->perm, 'r') != NULL) {
                has_permission = 1;
            }
            break;
        }
        owner = owner->next;
    }
    if (!has_permission) {
        printf("Client %d does not have permission to read file '%s'\n", c->client_id, filename);
        nameserver_log("STREAM_DENIED user=%s id=%d file=%s", c->client_name, c->client_id, filename);
        snprintf(outp.msg, sizeof(outp.msg), "ERR|no_permission|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }
    snprintf(outp.msg, sizeof(outp.msg), "STREAM|%s|%s|%d|%d",
             file->filename,
             ss->ip_address,
             ss->port,
             file->inode_no);
    printf("Prepared stream response: %s\n", outp.msg);
    nameserver_log("STREAM_RESPONSE user=%s id=%d file=%s ss=%s port=%d inode=%d",
                   c->client_name, c->client_id, file->filename, ss->ip_address, ss->port, file->inode_no);
    // Step 3: Send to client
    // You would normally send this over the client socket
    send(fd, &outp, sizeof(outp), 0);  
    nameserver_log("STREAM_SENT_TO_CLIENT id=%d user=%s msg=\"%s\"", c->client_id, c->client_name, outp.msg);
    file->rtime = time(NULL);
    // save_state("nm_state.dat");
    printf("Sending client %d info: %s\n", c->client_id, outp.msg);
}