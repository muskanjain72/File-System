#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include "create.h"
#include "log.h"
#include "nm.h"

void add_inode_to_storage(int ss_id, int inode_no)
{
    pthread_mutex_lock(&storage_mutex);
    if (ss_id >= 0 && ss_id < MAX_STORAGE && storages[ss_id])
    {
        for (int i = 0; i < MAX_FILES; i++)
        {
            if (storages[ss_id]->inodes[i] == -1)
            {
                storages[ss_id]->inodes[i] = inode_no;
                break;
            }
        }
    }
    pthread_mutex_unlock(&storage_mutex);
}

// === Hash Helpers ===
unsigned int hash_filename(const char *filename)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *filename++))
        hash = ((hash << 5) + hash) + c; // djb2
    return hash % HASH_SIZE;
}

void insert_file_to_hash(FileMeta *file)
{
    char key[384];
    build_full_path_for_file(file, key, sizeof(key));
    unsigned int h = hash_filename(key);
    printf("[DEBUG] inserting '%s' ptr=%p at hash[%u]\n", key, (void *)file, h);
    HashNode *node = malloc(sizeof(HashNode));
    node->file = file;

    pthread_mutex_lock(&hash_mutex);
    node->next = file_hash_table[h];
    file_hash_table[h] = node;
    pthread_mutex_unlock(&hash_mutex);
}

FileMeta *search_file_in_hash(const char *filename)
{
    // filename here is the full path key (e.g., "folder/name" or just "name")
    unsigned int h = hash_filename(filename);
    pthread_mutex_lock(&hash_mutex);
    HashNode *node = file_hash_table[h];
    while (node)
    {
        char key[384];
        build_full_path_for_file(node->file, key, sizeof(key));
        if (strcmp(key, filename) == 0)
        {
            pthread_mutex_unlock(&hash_mutex);
            return node->file;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&hash_mutex);
    return NULL;
}

// Remove any cache entry that points to the given file pointer
void cache_remove_file(FileMeta *file)
{
    pthread_mutex_lock(&cache_mutex);
    for (int i = 0; i < cache_count;)
    {
        if (cache[i].file_ref == file)
        {
            cache_count--;
            if (i < cache_count)
                cache[i] = cache[cache_count];
            // do not increment i; re-check moved entry
        }
        else
        {
            i++;
        }
    }
    pthread_mutex_unlock(&cache_mutex);
}

// Rehash a file after its folder/name changed: remove old hash node then insert with new key
void rehash_file(FileMeta *file)
{
    pthread_mutex_lock(&hash_mutex);
    for (int h = 0; h < HASH_SIZE; h++)
    {
        HashNode *prev = NULL, *node = file_hash_table[h];
        while (node)
        {
            if (node->file == file)
            {
                if (prev)
                    prev->next = node->next;
                else
                    file_hash_table[h] = node->next;
                free(node);
                break;
            }
            prev = node;
            node = node->next;
        }
    }
    pthread_mutex_unlock(&hash_mutex);
    insert_file_to_hash(file);
}

// === Cache Helpers ===
FileMeta *check_cache(const char *filename)
{
    pthread_mutex_lock(&cache_mutex);
    printf("check_cache: looking for '%s' in cache (count=%d)\n", filename, cache_count);

    for (int i = 0; i < cache_count; i++)
    {
        if (strcmp(cache[i].filename, filename) == 0)
        {
            printf("check_cache: found '%s' in cache at index %d\n", filename, i);

            FileMeta *ref = cache[i].file_ref;

            if (ref)
            {
                // Validate that the pointer is actually present in files[]
                int valid = 0;
                for (int j = 0; j < MAX_FILES; j++)
                {
                    if (files[j] == ref)
                    {
                        valid = 1;
                        break;
                    }
                }

                if (valid)
                {
                    cache[i].last_used = time(NULL);
                    printf("check_cache: pointer %p is valid in files[], returning it.\n", (void *)ref);
                    pthread_mutex_unlock(&cache_mutex);
                    return ref;
                }
                else
                {
                    // Stale or dangling pointer — remove from cache
                    printf("%s Filename:%s\n", ERR_STALE_POINTER, cache[i].filename);
                    printf("check_cache: removing stale cache entry for '%s' (ptr=%p)\n",
                           cache[i].filename, (void *)ref);
                    // Compact cache by moving last entry here
                    cache_count--;
                    if (i < cache_count)
                    {
                        cache[i] = cache[cache_count];
                        i--; // Re-check the moved entry
                    }
                    continue;
                }
            }
            else
            {
                // Null pointer — remove from cache
                printf("check_cache: removing null cache entry for '%s'\n", cache[i].filename);
                cache_count--;
                if (i < cache_count)
                {
                    cache[i] = cache[cache_count];
                    i--;
                }
                continue;
            }
        }
    }

    pthread_mutex_unlock(&cache_mutex);
    printf("check_cache: '%s' not found in cache\n", filename);
    return NULL;
}
// Adds or updates cache entry
void add_to_cache(const char *filename, FileMeta *file)
{
    pthread_mutex_lock(&cache_mutex);

    if (!file)
    {
        printf("%s Filename not found\n",ERR_MISSING_ARGUMENTS);
        printf("add_to_cache: cannot cache NULL pointer for '%s'\n", filename);
        pthread_mutex_unlock(&cache_mutex);
        return;
    }

    // Verify that the pointer is in files[]
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
        printf("add_to_cache: refusing to cache pointer %p for '%s' — not found in files[]\n",
               (void *)file, filename);
               printf("%s Filename:%s\n", ERR_STALE_POINTER, filename);
        pthread_mutex_unlock(&cache_mutex);
        return;
    }

    if (cache_count < CACHE_SIZE)
    {
        strncpy(cache[cache_count].filename, filename, sizeof(cache[cache_count].filename) - 1);
        cache[cache_count].filename[sizeof(cache[cache_count].filename) - 1] = '\0';
        cache[cache_count].file_ref = file;
        cache[cache_count].last_used = time(NULL);
        printf("add_to_cache: cached ptr %p for '%s' at cache[%d]\n",
               (void *)file, filename, cache_count);
        cache_count++;
    }
    else
    {
        // Replace least recently used
        int lru = 0;
        for (int i = 1; i < CACHE_SIZE; i++)
            if (cache[i].last_used < cache[lru].last_used)
                lru = i;
        strncpy(cache[lru].filename, filename, sizeof(cache[lru].filename) - 1);
        cache[lru].filename[sizeof(cache[lru].filename) - 1] = '\0';
        cache[lru].file_ref = file;
        cache[lru].last_used = time(NULL);
        printf("add_to_cache: replaced LRU index %d with ptr %p for '%s'\n",
               lru, (void *)file, filename);
    }

    pthread_mutex_unlock(&cache_mutex);
}

// === Create Function ===
int create(Client *c, char *buf)
{
    printf("Create function called!\n");
    nameserver_log("CREATE_REQUEST user=%s id=%d ip=%s port=%d file=%s",
                   c->client_name, c->client_id, c->ip_address, c->port, buf);

    // Support folder-qualified path like "docs/file"
    char folder[256];
    char name[128];
    split_path(buf, folder, sizeof(folder), name, sizeof(name));

    // If target folder specified, ensure it exists
    if (folder[0] != '\0')
    {
        if (!folder_exists(folder))
        {
            printf("❌ Cannot create '%s': folder '%s' does not exist.\n", buf, folder);
            return -3; /* missing parent/folder */
        }
    }

    // Step 1: Lookup existing
    FileMeta *existing = NULL;
    if (folder[0] == '\0')
    {
        existing = check_cache(name);
        if (!existing)
            existing = search_file_in_hash(name);
    }
    else
    {
        existing = find_file_by_path(buf);
    }

    pthread_mutex_lock(&files_mutex);
    if (existing)
    {
        /* File already exists */
        fprintf(stderr, "%s Filename:%s\n", ERR_FILE_EXISTS, buf);
        nameserver_log("CREATE_FAIL exists user=%s file=%s", c->client_name, buf);
        pthread_mutex_unlock(&files_mutex);
        return -2; /* indicate file exists */
    }

    // Step 2: Create new FileMeta in table
    int free_index = -1;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (!files[i])
        {
            free_index = i;
            break;
        }
    }

    if (free_index == -1)
    {
        /* file table full */
        fprintf(stderr, "%s Filename:%s\n", ERR_FT_FULL, buf);
        pthread_mutex_unlock(&files_mutex);
        return -1;
    }

    FileMeta *f = (FileMeta *)calloc(1, sizeof(FileMeta));
    if (!f)
    {
        pthread_mutex_unlock(&files_mutex);
        fprintf(stderr, "%s Filename:%s\n", ERR_FT_FULL, buf);
        fprintf(stderr, "OOM FileMeta\n");
        return -1;
    }
    files[free_index] = f;

    printf("create: allocated FileMeta %p assigned to files[%d]\n", (void *)f, free_index);

    f->inode_no = free_index;
    strcpy(f->filename, name);
    f->owners = (FileOwnerPerm *)calloc(1, sizeof(FileOwnerPerm));
    if (!f->owners)
    {
        free(f);
        files[free_index] = NULL;
        pthread_mutex_unlock(&files_mutex);

        fprintf(stderr, "OOM owners\n");
        return -1;
    }
    f->owners->client_id = c->client_id;
    strcpy(f->owners->perm, "rw");
    strcpy(f->owners->client_name, c->client_name);
    f->owners->next = NULL;

    f->ss_id = -1;
    f->block_map = NULL;
    f->file_size = 0;
    f->sentences = NULL;
    pthread_mutex_init(&f->sentence_mutex, NULL);
    f->sentence_count = 0;
    f->word_count = 0;
    f->char_count = 0;
    strcpy(f->created_by, c->client_name);
    f->ctime = time(NULL);
    f->prev_sentence_count=0;
    f->prev_word_count=0;
    f->prev_char_count=0;
    f->rtime = 0;
    f->wtime = 0;
    // initialize checkpoint list
    f->checkpoints = NULL;
    f->pending_requests = NULL;
    // set folder association
    if (folder[0] != '\0')
    {
        strncpy(f->folder_path, folder, sizeof(f->folder_path) - 1);
        f->folder_path[sizeof(f->folder_path) - 1] = '\0';
    }
    else
    {
        f->folder_path[0] = '\0';
    }

    // Step 3: Pick least used storage server
    pthread_mutex_lock(&storage_mutex);
    int chosen_ss = -1;
    long min_used = LONG_MAX;

    for (int s = 0; s < MAX_STORAGE; s++)
    {
        if (storages[s] && storages[s]->used_bytes < min_used)
        {
            min_used = storages[s]->used_bytes;
            chosen_ss = storages[s]->ss_id;
        }
    }
    pthread_mutex_unlock(&storage_mutex);

    f->ss_id = chosen_ss;
    pthread_mutex_unlock(&files_mutex);

    // Track inode on storage metadata (nameserver view)
    if (chosen_ss != -1)
    {
        add_inode_to_storage(chosen_ss, f->inode_no);
    }

    if (chosen_ss != -1)
    {
        printf("✅ File '%s' assigned to Storage Server %d (used_bytes=%ld)\n",
               name, chosen_ss, min_used);
        nameserver_log("CREATE_OK user=%s file=%s inode=%d ss=%d",
                       c->client_name, buf, f->inode_no, chosen_ss);
    }
    else
    {
        fprintf(stderr, "%s Filename:%s\n", ERR_SS_FULL, buf);
        nameserver_log("CREATE_FAIL no_storage user=%s file=%s", c->client_name, buf);
    }

    if (folder[0] != '\0')
        printf("📁 File created: %s/%s (inode %d)\n", folder, name, free_index);
    else
        printf("📁 File created: %s (inode %d)\n", name, free_index);

    // Step 4: Update hash + cache
    insert_file_to_hash(f);
    char fullkey[384];
    build_full_path_for_file(f, fullkey, sizeof(fullkey));
    printf("Inserted file '%s' into hash table (inode=%d ptr=%p).\n", fullkey, f->inode_no, (void *)f);
    add_to_cache(fullkey, f);
    printf("Added file '%s' to cache (ptr=%p).\n", fullkey, (void *)f);

    // Step 5: Notify storage
    send_create_request_to_storage(f->ss_id, f->inode_no);
    return f->ss_id;
}

// === Send to Storage ===
void send_create_request_to_storage(int ss_id, int file_id)
{
    pthread_mutex_lock(&storage_mutex);
    StorageServer *ss = NULL;
    for (int i = 0; i < MAX_STORAGE; i++)
    {
        if (storages[i] && storages[i]->ss_id == ss_id)
        {
            ss = storages[i];
            break;
        }
    }
    pthread_mutex_unlock(&storage_mutex);

    if (!ss)
    {
        fprintf(stderr, "%s SS_ID:%d\n", ERR_SS_UNAVAILABLE, ss_id);
        nameserver_log("CREATE_SEND_FAIL ss_unavailable ss_id=%d file=%d", ss_id, file_id);
        return;
    }

    char req[128];
    snprintf(req, sizeof(req), "CREATE %d/%d.txt", ss_id, file_id);

    pthread_mutex_lock(&ss->lock);
    if (send(ss->fd, req, strlen(req), 0) <= 0)
    {
        perror("send to storage");
        fprintf(stderr, "%s SS_ID:%d\n", ERR_SS_UNAVAILABLE, ss_id);
        nameserver_log("CREATE_SEND_FAIL send_err ss_id=%d file=%d", ss_id, file_id);
        pthread_mutex_unlock(&ss->lock);
        return;
    }
    pthread_mutex_unlock(&ss->lock);
    // save_state("nm_state.dat");

    printf("📤 Sent create request to storage %d: %s\n", ss_id, req);
    nameserver_log("CREATE_SENT_TO_SS ss=%d req=\"%s\"", ss_id, req);
}
