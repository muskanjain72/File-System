#include"persist.h"
#include "log.h"

int save_storage_server(const StorageServer *s, const char *path, int inode_count)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("fopen");
        storageserver_log("PERSIST_SAVE_FAIL path=%s err=%s", path, strerror(errno));
        return -1;
    }

    // Write fields in text form
    fprintf(fp, "%d\n", s->ss_id);
    fprintf(fp, "%s\n", s->ip_address);
    fprintf(fp, "%d\n", s->port);
    fprintf(fp, "%ld\n", s->used_bytes);
    fprintf(fp, "%d\n", s->status);

    // Write inode count
    fprintf(fp, "%d\n", inode_count);

    // Write inode list
    for (int i = 0; i < inode_count; i++) {
        fprintf(fp, "%d ", s->inodes[i]);
    }
    fprintf(fp, "\n");

    fclose(fp);
    storageserver_log("PERSIST_SAVED path=%s ss_id=%d inode_count=%d", path, s->ss_id, inode_count);
    return 0;
}

StorageServer* load_storage_server(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen");
        storageserver_log("PERSIST_LOAD_FAIL path=%s err=%s", path, strerror(errno));
        return NULL;
    }

    StorageServer *s = malloc(sizeof(StorageServer));
    if (!s) {
        storageserver_log("PERSIST_LOAD_FAIL alloc_error path=%s", path);
        return NULL;
    }

    fscanf(fp, "%d", &s->ss_id);
    fscanf(fp, "%s", s->ip_address);
    fscanf(fp, "%d", &s->port);
    fscanf(fp, "%ld", &s->used_bytes);
    fscanf(fp, "%d", &s->status);

    int count;
    fscanf(fp, "%d", &count);

    // Initialize fixed-size array to -1
    for (int i = 0; i < 1024; i++)
        s->inodes[i] = -1;

    // Read stored values up to array capacity
    for (int i = 0; i < count && i < 1024; i++) {
        fscanf(fp, "%d", &s->inodes[i]);
    }

    s->fd = -1;

    fclose(fp);
    storageserver_log("PERSIST_LOADED path=%s ss_id=%d", path, s->ss_id);
    return s;
}
