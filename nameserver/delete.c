#include "delete.h"
#include "log.h"

// forward declarations to avoid implicit-declaration warnings
void send_delete_request_to_storage(int ss_id, int file_id);
void free_inode_from_storage(int ss_id, int inode_no);

void free_inode_from_storage(int ss_id, int inode_no) {
    // Find storage server by id and mark the inode slot as free (-1)
    pthread_mutex_lock(&storage_mutex);
    StorageServer *ss = NULL;
    for (int i = 0; i < MAX_STORAGE; i++) {
        if (storages[i] && storages[i]->ss_id == ss_id) {
            ss = storages[i];
            break;
        }
    }
    if (ss) {
        for (int i = 0; i < MAX_FILES; i++) {
            if (ss->inodes[i] == inode_no) {
                ss->inodes[i] = -1; // Mark as free
                break;
            }
        }
    }
    pthread_mutex_unlock(&storage_mutex);
}

void remove_from_hash(const char *filename) {
    // filename is the full path key
    unsigned int h = hash_filename(filename);

    pthread_mutex_lock(&hash_mutex);
    HashNode *curr = file_hash_table[h];
    HashNode *prev = NULL;

    while (curr) {
        char key[384];
        build_full_path_for_file(curr->file, key, sizeof(key));
        if (strcmp(key, filename) == 0) {
            // Found the node to remove
            if (prev)
                prev->next = curr->next;
            else
                file_hash_table[h] = curr->next;

            printf("[DEBUG] Removed '%s' from hash[%u]\n", filename, h);
            free(curr); // free the hash node (NOT the file itself!)
            pthread_mutex_unlock(&hash_mutex);
            return;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&hash_mutex);
    printf("[DEBUG] '%s' not found in hash table for removal.\n", filename);
}
void remove_from_cache(const char *filename) {
    pthread_mutex_lock(&cache_mutex);
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(cache[i].filename, filename) == 0) {
            printf("[DEBUG] Removing '%s' from cache at index %d\n", filename, i);
            // Move the last entry into this position
            cache_count--;
            if (i < cache_count) {
                cache[i] = cache[cache_count];
            }
            pthread_mutex_unlock(&cache_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&cache_mutex);
    printf("[DEBUG] '%s' not found in cache for removal.\n", filename);
}

int delete_file(Client *c, const char *filename, int fd)
{
    if (!filename) return -1;
    nameserver_log("DELETE_REQUEST user=%s id=%d file=%s", c->client_name, c->client_id, filename);

    FileMeta *file = find_file_by_path(filename);
    if (!file) {
        fprintf(stderr, "%s Filename:%s\n", ERR_NOT_FOUND, filename);
        nameserver_log("DELETE_FAIL not_found user=%s file=%s", c->client_name, filename);
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return -1;
    }

    int valid = 0;
    int index = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i] == file) {
            valid = 1;
            index = i;
            break;
        }
    }
    if (!valid) {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|stale|%s", filename);
        fprintf(stderr, "%s Filename:%s\n", ERR_STALE_POINTER, filename);
        nameserver_log("DELETE_FAIL stale_pointer user=%s file=%s", c->client_name, filename);
        send(fd, &outp, sizeof(outp), 0);
        return -1;
    }

    // Save BEFORE freeing
    int ss_id_val = file->ss_id;
    int inode_val = file->inode_no;

    // Remove from global array
    pthread_mutex_lock(&files_mutex);
    files[index] = NULL;
    pthread_mutex_unlock(&files_mutex);

    // Remove from cache + hash using full path key
    char key[384];
    build_full_path_for_file(file, key, sizeof(key));
    remove_from_cache(key);
    remove_from_hash(key);

    // Free the FileMeta structure
    free(file);

    // Notify storage
    send_delete_request_to_storage(ss_id_val, inode_val);

    // Mark inode free in storage server metadata
    free_inode_from_storage(ss_id_val, inode_val);

    /* NOTE: do NOT send a Packet here. The caller (nm.c) is responsible for
     * sending the standardized response to the client to avoid duplicate
     * replies. We still log the successful delete for operator visibility. */
    nameserver_log("DELETE_OK user=%s file=%s inode=%d ss=%d", c->client_name, filename, inode_val, ss_id_val);

    return ss_id_val;
}


// === Send Delete Request to Storage ===
void send_delete_request_to_storage(int ss_id, int file_id) {
    pthread_mutex_lock(&storage_mutex);
    StorageServer *ss = NULL;
    for (int i = 0; i < MAX_STORAGE; i++) {
        if (storages[i] && storages[i]->ss_id == ss_id) {
            ss = storages[i];
            break;
        }
    }
    pthread_mutex_unlock(&storage_mutex);

    if (!ss) {
        fprintf(stderr, "%s SS_ID:%d\n", ERR_SS_UNAVAILABLE, ss_id);
        return;
    }

    char req[128];
    snprintf(req, sizeof(req), "DELETE %d/%d.txt", ss_id, file_id);

    pthread_mutex_lock(&ss->lock);
    if (send(ss->fd, req, strlen(req), 0) <= 0) {
        perror("send to storage");
        fprintf(stderr, "%s SS_ID:%d\n", ERR_SS_UNAVAILABLE, ss_id);
        nameserver_log("DELETE_SEND_FAIL ss=%d req=\"%s\"", ss_id, req);
        pthread_mutex_unlock(&ss->lock);
        return;
    }
    pthread_mutex_unlock(&ss->lock);

    // save_state("nm_state.dat");

    printf("🗑️ Sent delete request to storage %d: %s\n", ss_id, req);
    nameserver_log("DELETE_SENT_TO_SS ss=%d req=\"%s\"", ss_id, req);
}
