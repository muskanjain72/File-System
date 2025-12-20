#include "addaccess.h"
#include "log.h"

void add_access(Client* c, const char* perm, const char* fname, const char* cname, Packet* outp) {
    nameserver_log("ADDACCESS_REQUEST user=%s id=%d ip=%s port=%d file=%s target=%s perm=%s",
                   c->client_name, c->client_id, c->ip_address, c->port, fname, cname, perm);
    FileMeta *file = NULL;

    /* find_file_by_path may acquire files_mutex internally; do not hold
     * files_mutex while calling it to avoid deadlocks. */
    file = find_file_by_path(fname);

    if (!file) {
        fprintf(stderr, "%s FileName:%s\n", ERR_NOT_FOUND, fname);
        nameserver_log("ADDACCESS_FAIL not_found user=%s file=%s target=%s", c->client_name, fname, cname);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", fname);
        return;
    }

    /* Validate that the returned pointer is one of the current files[] entries. */
    int valid = 0;
    pthread_mutex_lock(&files_mutex);
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i] == file) {
            valid = 1;
            break;
        }
    }
    if (!valid) {
        fprintf(stderr, "%s FileName:%s\n", ERR_STALE_POINTER, fname);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|stale|%s", fname);
        pthread_mutex_unlock(&files_mutex);
        return;
    }
    pthread_mutex_unlock(&files_mutex);

    // --- Step 2: Ensure only creator can modify access ---
    if (strcmp(file->created_by, c->client_name) != 0) {
        fprintf(stderr, "%s\n", ERR_ACCESS_DENIED);
        snprintf(outp->msg, sizeof(outp->msg),
                 "Permission denied: only '%s' can modify access.", file->created_by);
        return;
    }

    // --- Step 3: Check if the target client exists and capture its current id (if connected) ---
    pthread_mutex_lock(&clients_mutex);
    int client_exists = 0;
    int target_client_id = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->client_name, cname) == 0) {
            client_exists = 1;
            target_client_id = clients[i]->client_id;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!client_exists) {
        fprintf(stderr, "%s ClientName:%s\n", ERR_NO_CLIENT, cname);
        nameserver_log("ADDACCESS_FAIL no_client user=%s target=%s", c->client_name, cname);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_such_client|%s", cname);
        return;
    }

    // --- Step 4: Normalize permission ---
    char add_perm[4] = "";
    if (strcmp(perm, "R") == 0)
        strcpy(add_perm, "r");
    else if (strcmp(perm, "W") == 0)
        /* Grant write permission and implicitly grant read as well. */
        strcpy(add_perm, "rw");
    else {
        fprintf(stderr, "%s Invalid permission: %s\n", ERR_NO_PERMISSION, perm);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_perm|%s", perm);
        return;
    }

    // --- Step 5: Update or add permission ---
    pthread_mutex_lock(&files_mutex);
    FileOwnerPerm *curr = file->owners;
    while (curr) {
        if (strcmp(curr->client_name, cname) == 0) {
            // Merge permissions: ensure both are represented
            char newperm[4] = "";
            int has_r = strchr(curr->perm, 'r') != NULL || strchr(add_perm, 'r') != NULL;
            int has_w = strchr(curr->perm, 'w') != NULL || strchr(add_perm, 'w') != NULL;
            if (has_r) strcat(newperm, "r");
            if (has_w) strcat(newperm, "w");

            strncpy(curr->perm, newperm, sizeof(curr->perm) - 1);
            curr->perm[sizeof(curr->perm) - 1] = '\0';

            // Also refresh client_id if we know it (helps when matching by id as fallback)
            curr->client_id = target_client_id;

            snprintf(outp->msg, sizeof(outp->msg),
                     "Updated access for '%s' to '%s' on file '%s'.",
                     cname, curr->perm, fname);
            pthread_mutex_unlock(&files_mutex);
            return;
        }
        curr = curr->next;
    }

    // --- Step 6: Add new entry (if user not found) ---
    FileOwnerPerm *new_perm = malloc(sizeof(FileOwnerPerm));
    new_perm->client_id = target_client_id; // note: client_id can be ephemeral; name is the stable key
    strncpy(new_perm->client_name, cname, sizeof(new_perm->client_name) - 1);
    new_perm->client_name[sizeof(new_perm->client_name) - 1] = '\0';
    strncpy(new_perm->perm, add_perm, sizeof(new_perm->perm) - 1);
    new_perm->perm[sizeof(new_perm->perm) - 1] = '\0';
    new_perm->next = file->owners;
    file->owners = new_perm;

    snprintf(outp->msg, sizeof(outp->msg),
             "Access '%s' added for '%s' on file '%s'.",
             add_perm, cname, fname);
    nameserver_log("ADDACCESS_OK user=%s file=%s target=%s perm=%s",
                   c->client_name, fname, cname, add_perm);
    // save_state("nm_state.dat");
    pthread_mutex_unlock(&files_mutex);
}
