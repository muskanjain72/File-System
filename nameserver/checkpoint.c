#include "checkpoint.h"
#include "log.h"
#include"nm.h"

static void append_checkpoint(FileMeta *file, const char *tag) {
    if (!file || !tag || !*tag) return;
    CheckpointTag *node = (CheckpointTag*)calloc(1, sizeof(CheckpointTag));
    if (!node) return;
    strncpy(node->tag, tag, sizeof(node->tag) - 1);
    node->when = time(NULL);
    node->next = NULL;

    // Insert at head for simplicity
    node->next = file->checkpoints;
    file->checkpoints = node;
}

void checkpoint_file(Client *c, const char *filename, const char *tag, Packet *outp, int client_fd)
{
    (void)client_fd; // using main SS channel; response to client via Packet
    outp->client_id = c->client_id;

    if (!filename || !tag || !*tag) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_args|%s", filename ? filename : "");
        return;
    }

    nameserver_log("CHECKPOINT_REQUEST user=%s id=%d file=%s tag=%s", c->client_name, c->client_id, filename, tag);

    // 1) Resolve file
    FileMeta *file = find_file_by_path(filename);
    if (!file) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", filename);
        nameserver_log("CHECKPOINT_FAIL not_found user=%s file=%s", c->client_name, filename);
        return;
    }

    // 2) Validate file pointer
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i] == file) { valid = 1; break; }
    }
    if (!valid) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|stale|%s", filename);
        nameserver_log("CHECKPOINT_FAIL stale_pointer user=%s file=%s", c->client_name, filename);
        return;
    }

    // 3) Duplicate tag check (per-file)
    pthread_mutex_lock(&files_mutex);
    for (CheckpointTag *it = file->checkpoints; it; it = it->next) {
        if (strcmp(it->tag, tag) == 0) {
            pthread_mutex_unlock(&files_mutex);
            snprintf(outp->msg, sizeof(outp->msg),
                     "CHECKPOINT|CHECKPOINT_FAIL|%s|%s|this tag name for this filename already exists",
                     filename, tag);
            nameserver_log("CHECKPOINT_FAIL tag_exists user=%s file=%s tag=%s", c->client_name, filename, tag);
            return;
        }
    }
    pthread_mutex_unlock(&files_mutex);

    // 4) Check SS availability
    int ss_id = file->ss_id;
    if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id]) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_unavailable|%d", ss_id);
        nameserver_log("CHECKPOINT_FAIL ss_unavailable user=%s file=%s ss=%d", c->client_name, filename, ss_id);
        return;
    }
    StorageServer *ss = storages[ss_id];

    // 5) Permission: require write (like UNDO)
    int has_permission = 0;
    for (FileOwnerPerm *owner = file->owners; owner; owner = owner->next) {
        if (owner->client_id == c->client_id || strcmp(owner->client_name, c->client_name) == 0) {
            if (owner->perm[0] == 'w' || owner->perm[1] == 'w') has_permission = 1;
            break;
        }
    }
    if (!has_permission) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_permission|%s", filename);
        nameserver_log("CHECKPOINT_DENIED user=%s id=%d file=%s", c->client_name, c->client_id, filename);
        return;
    }
    int sockfd=ss->fd;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "CHECKPOINT %d %s", file->inode_no, tag);
    pthread_mutex_lock(&ss->lock);
    if (send(sockfd, cmd, strlen(cmd), 0) <= 0) {
        perror("send CHECKPOINT failed");
        // close(sockfd);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_send_fail|%s", filename);
        nameserver_log("CHECKPOINT_SEND_FAIL ss=%d file=%s tag=%s", ss->ss_id, filename, tag);
        return;
    }
    nameserver_log("CHECKPOINT_SENT_TO_SS ss=%d inode=%d tag=%s via_client_port", ss->ss_id, file->inode_no, tag);

    // 7) Await SS response on this dedicated socket
    p1 resp;
    ssize_t n = recv(sockfd, &resp, sizeof(resp), 0);
    pthread_mutex_unlock(&ss->lock);
    if (n <= 0) {
        perror("recv CHECKPOINT response failed");
        // close(sockfd);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_recv_fail|%s", filename);
        nameserver_log("CHECKPOINT_RECV_FAIL ss=%d file=%s", ss->ss_id, filename);
        return;
    }
    // close(sockfd);

    printf("Checkpoint operation completed for file '%s' with tag '%s' and response '%s' %d.\n", filename, tag, resp.msg, strncmp(resp.msg, "CHECKPOINT_SUCCESS", 18) == 0);

    if (strncmp(resp.msg, "CHECKPOINT_SUCCESS", 18) == 0) {
        pthread_mutex_lock(&files_mutex);
        append_checkpoint(file, tag);
        pthread_mutex_unlock(&files_mutex);
        file->wtime = time(NULL);
        snprintf(outp->msg, sizeof(outp->msg), "CHECKPOINT|CHECKPOINT_SUCCESS|%s|%s", filename, tag);
        nameserver_log("CHECKPOINT_OK user=%s file=%s tag=%s", c->client_name, filename, tag);
    } else {
        const char *reason = NULL;
        if (strncmp(resp.msg, "CHECKPOINT_FAIL|", 16) == 0) reason = resp.msg + 16;
        if (reason && *reason)
            snprintf(outp->msg, sizeof(outp->msg), "CHECKPOINT|CHECKPOINT_FAIL|%s|%s|%s", filename, tag, reason);
        else
            snprintf(outp->msg, sizeof(outp->msg), "CHECKPOINT|CHECKPOINT_FAIL|%s|%s", filename, tag);
        nameserver_log("CHECKPOINT_FAIL user=%s file=%s tag=%s resp=%s", c->client_name, filename, tag, resp.msg);
    }
}
