#include "removeaccess.h"
#include "log.h"

void remove_access(Client* c, char* fname, char* cname, Packet* outp) {
    nameserver_log("REMACCESS_REQUEST user=%s id=%d file=%s target=%s",
                   c->client_name, c->client_id, fname, cname);
    FileMeta *file = NULL;

    pthread_mutex_lock(&files_mutex);
    file = find_file_by_path(fname);

    if (!file) {
        nameserver_log("REMACCESS_FAIL not_found user=%s file=%s", c->client_name, fname);
        snprintf(outp->msg, sizeof(outp->msg), "%s", ERR_NOT_FOUND);
        pthread_mutex_unlock(&files_mutex);
        return;
    }

    // --- Validate that file pointer is still valid ---
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i] == file) {
            valid = 1;
            break;
        }
    }
    if (!valid) {
        snprintf(outp->msg, sizeof(outp->msg), "%s", ERR_STALE_POINTER);
        pthread_mutex_unlock(&files_mutex);
        return;
    }
    pthread_mutex_unlock(&files_mutex);

    // --- Step 2: Ensure only creator can modify access ---
    if (strcmp(file->created_by, c->client_name) != 0) {
        nameserver_log("REMACCESS_FAIL owner_mismatch user=%s file=%s owner=%s", c->client_name, fname, file->created_by);
        snprintf(outp->msg, sizeof(outp->msg), "%s", ERR_OWNER_MISMATCH);
        return;
    }

    // --- Step 3: Check if the target client exists and capture its current id (if connected) ---
    pthread_mutex_lock(&clients_mutex);
    int client_exists = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->client_name, cname) == 0) {
            client_exists = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!client_exists) {
        nameserver_log("REMACCESS_FAIL no_client user=%s target=%s", c->client_name, cname);
        snprintf(outp->msg, sizeof(outp->msg), "%s", ERR_NO_CLIENT);
        return;
    }

    // --- Step 5: remove permission ---
    pthread_mutex_lock(&files_mutex);
    FileOwnerPerm *curr = file->owners;
    FileOwnerPerm *prev = NULL;
    while (curr) {
        if (strcmp(curr->client_name, cname) == 0) {
            if(prev==NULL){
                file->owners = curr->next;
            }
            else{
                prev->next = curr->next;
            }
            snprintf(outp->msg, sizeof(outp->msg), "%s", OK_ACCESS_REVOKED);
            nameserver_log("REMACCESS_OK user=%s file=%s removed=%s", c->client_name, fname, cname);
            free(curr);
            pthread_mutex_unlock(&files_mutex);
            return;
        }
        prev=curr;
        curr = curr->next;
    }

    // save_state("nm_state.dat");
    pthread_mutex_unlock(&files_mutex);
}
