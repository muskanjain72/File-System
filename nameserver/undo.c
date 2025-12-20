//#include <stdio.h>  // included transitively
#include "undo.h"
#include "log.h"
#include"nm.h"

void undo_file(Client *c, const char *filename, Packet *outp, int client_fd)
{
    (void)client_fd; // not used for direct sending; nm.c will send Packet after call
    if (!filename)
    {
        fprintf(stderr, "undo_file: filename is NULL\n");
        return;
    }
    printf("Processing UNDO request for file '%s' from client ID %d [nameserver]\n",
           filename, c->client_id);
    nameserver_log("UNDO_REQUEST user=%s id=%d file=%s", c->client_name, c->client_id, filename);

    // --- Step 1: Lookup file (supports folder-qualified path) ---
    FileMeta *file = find_file_by_path(filename);
    outp->client_id = c->client_id;
    if (!file)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", filename);
        nameserver_log("UNDO_FAIL not_found user=%s file=%s", c->client_name, filename);
        return;
    }

    // --- Step 2: Validate file reference ---
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i] == file)
        {
            valid = 1;
            break;
        }
    }
    if (!valid)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|stale|%s", filename);
        nameserver_log("UNDO_FAIL stale_pointer user=%s file=%s", c->client_name, filename);
        return;
    }
    printf("File reference validated for file '%s'\n", filename);

    // --- Step 3: Check Storage Server availability ---
    int ss_id = file->ss_id;
    if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id])
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_unavailable|%d", ss_id);
        nameserver_log("UNDO_FAIL ss_unavailable user=%s file=%s ss=%d", c->client_name, filename, ss_id);
        return;
    }
    StorageServer *ss = storages[ss_id];
    printf("Storage Server %d found for file '%s'\n", ss_id, filename);

    // --- Step 4: Check write permission ---
    FileOwnerPerm *owner = file->owners;
    int has_permission = 0;
    while (owner)
    {
        if (owner->client_id == c->client_id)
        {
            if (owner->perm[0] == 'w' || owner->perm[1] == 'w')
                has_permission = 1;
            break;
        }
        owner = owner->next;
    }
    if (!has_permission)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_permission|%s", filename);
        nameserver_log("UNDO_DENIED user=%s id=%d file=%s", c->client_name, c->client_id, filename);
        return;
    }
    printf("Write permission granted for client ID %d on file '%s'\n", c->client_id, filename);

    // --- Step 5: Send UNDO command to Storage Server (client listener) ---
    // Avoid racing with nm's background reader on ss->fd by using the SS client-facing port
    int tmp_sock = ss->fd;

    char buf[64];
    snprintf(buf, sizeof(buf), "UNDO %d", file->inode_no);
    pthread_mutex_lock(&ss->lock);
    if (send(tmp_sock, buf, strlen(buf), 0) <= 0)
    {
        perror("send UNDO command failed");
        snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_send_fail|%s", filename);
        nameserver_log("UNDO_SEND_FAIL ss=%d file=%s cmd=\"%s\"", ss->ss_id, filename, buf);
        return;
    }
    printf("UNDO command sent to Storage Server %d (%s:%d) for inode %d\n", ss->ss_id, ss->ip_address, ss->port, file->inode_no);
    nameserver_log("UNDO_SENT_TO_SS ss=%d file=%s inode=%d", ss->ss_id, filename, file->inode_no);

    // --- Step 6: Wait for response from Storage Server ---
    p1 resp;
    ssize_t n = recv(tmp_sock, &resp, sizeof(resp), 0);
    pthread_mutex_unlock(&ss->lock);
    if (n <= 0)
    {
        perror("recv UNDO response failed");
        snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_recv_fail|%s", filename);
        nameserver_log("UNDO_RECV_FAIL ss=%d file=%s", ss->ss_id, filename);
        return;
    }
    // resp[n] = '\0';
    // close(tmp_sock);
    if (strncmp(resp.msg, "UNDO_SUCCESS", 12) == 0)
    {
        snprintf(outp->msg, sizeof(outp->msg), "UNDO|UNDO_SUCCESS|%s", filename);
        printf("✅ UNDO successful for file '%s'\n", filename);
        nameserver_log("UNDO_OK user=%s file=%s", c->client_name, filename);
        // Update file metadata
        file->sentence_count = file->prev_sentence_count;
        file->word_count = file->prev_word_count;
        file->char_count = file->prev_char_count;
        file->wtime = time(NULL);
    }
    else
    {
        /* propagate reason if provided: UNDO_FAIL|<reason> */
        const char *reason = NULL;
        if (strncmp(resp.msg, "UNDO_FAIL|", 10) == 0)
            reason = resp.msg + 10;
        if (reason && *reason)
            snprintf(outp->msg, sizeof(outp->msg), "UNDO|UNDO_FAIL|%s|%s", filename, reason);
        else
            snprintf(outp->msg, sizeof(outp->msg), "UNDO|UNDO_FAIL|%s", filename);
        fprintf(stderr, "❌ UNDO failed for file '%s' (resp='%s')\n", filename, resp.msg);
        nameserver_log("UNDO_FAIL user=%s file=%s resp=\"%s\"", c->client_name, filename, resp.msg);
    }

    // save_state("nm_state.dat");
}