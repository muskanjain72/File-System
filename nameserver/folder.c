#include "folder.h"
#include "log.h"

static FolderMeta *folders_head = NULL;
static pthread_mutex_t folders_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper: validate folder path characters and format
static int is_valid_folder_path(const char *p)
{
    if (!p || !*p)
        return 0; // empty invalid for creation
    if (strlen(p) >= 255)
        return 0;
    if (p[0] == '/' || p[strlen(p) - 1] == '/')
        return 0; // no leading/trailing slash
    for (const char *c = p; *c; ++c)
    {
        if (*c == '|' || *c == '\\' || *c == '\n' || *c == '\r')
            return 0;
    }
    return 1;
}

// Helper: find folder by path
static FolderMeta *find_folder(const char *path)
{
    FolderMeta *curr = folders_head;
    while (curr)
    {
        if (strcmp(curr->path, path) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

// Helper: check parent exists (for nested paths)
static int parent_exists(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash)
        return 1; // top-level folder
    char parent[256];
    size_t len = (size_t)(slash - path);
    if (len == 0)
        return 1; // root
    strncpy(parent, path, len);
    parent[len] = '\0';
    return find_folder(parent) != NULL;
}

int folder_exists(const char *folder_path)
{
    if (!folder_path || !*folder_path)
        return 0;
    pthread_mutex_lock(&folders_mutex);
    FolderMeta *f = find_folder(folder_path);
    int exists = (f != NULL);
    pthread_mutex_unlock(&folders_mutex);
    return exists;
}

void create_folder(Client *c, const char *folder_path, Packet *outp)
{
    outp->client_id = c->client_id;
    nameserver_log("CREATEFOLDER_REQUEST user=%s id=%d folder=%s", c->client_name, c->client_id, folder_path);
    if (!is_valid_folder_path(folder_path))
    {
        fprintf(stderr, "Invalid folder path: %s\n", folder_path ? folder_path : "(null)");
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_folder|%s", folder_path ? folder_path : "(null)");
        return;
    }
    pthread_mutex_lock(&folders_mutex);
    if (find_folder(folder_path))
    {
        pthread_mutex_unlock(&folders_mutex);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|folder_exists|%s", folder_path);
        nameserver_log("CREATEFOLDER_FAIL exists user=%s folder=%s", c->client_name, folder_path);
        return;
    }
    if (!parent_exists(folder_path))
    {
        fprintf(stderr, "Parent folder does not exist for: %s\n", folder_path);
        pthread_mutex_unlock(&folders_mutex);
        snprintf(outp->msg, sizeof(outp->msg), "ERR|missing_parent|%s", folder_path);
        return;
    }
    FolderMeta *f = (FolderMeta *)calloc(1, sizeof(FolderMeta));
    strncpy(f->path, folder_path, sizeof(f->path) - 1);
    strncpy(f->created_by, c->client_name, sizeof(f->created_by) - 1);
    f->created = time(NULL);
    f->next = folders_head;
    folders_head = f;
    pthread_mutex_unlock(&folders_mutex);
    snprintf(outp->msg, sizeof(outp->msg), "FOLDER_CREATED|%s", folder_path);
    nameserver_log("CREATEFOLDER_OK user=%s folder=%s", c->client_name, folder_path);
}

void move_file_to_folder(Client *c, const char *filename, const char *folder_path, Packet *outp)
{
    outp->client_id = c->client_id;
    nameserver_log("MOVE_REQUEST user=%s id=%d file=%s dest=%s", c->client_name, c->client_id, filename, folder_path);
    if (!filename || !folder_path)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|usage|MOVE <filename> <folder>");
        return;
    }
    if (!is_valid_folder_path(folder_path))
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_folder|%s", folder_path);
        return;
    }
    // Locate file first: allow either a full path or a bare filename.
    FileMeta *file = NULL;
    if (strchr(filename, '/'))
    {
        // full path lookup
        file = find_file_by_path(filename);
        if (!file)
        {
            snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", filename);
            return;
        }
    }
    else
    {
        // Bare name: must be unique across all folders. Search under files_mutex.
        pthread_mutex_lock(&files_mutex);
        for (int i = 0; i < MAX_FILES; i++)
        {
            if (files[i] && strcmp(files[i]->filename, filename) == 0)
            {
                if (file)
                { // ambiguity
                    file = NULL; // flag ambiguous
                    break;
                }
                file = files[i];
            }
        }
        pthread_mutex_unlock(&files_mutex);
        if (!file)
        {
            snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", filename);
            return;
        }
        // If ambiguous (we left file==NULL because multiple found), return explicit code
        // Note: above we set file=NULL on ambiguity; detect that by checking cache again quickly
        // If multiple matches exist, respond with ambiguous_or_not_found to preserve compatibility
        // (older clients may expect this token).
        // To detect ambiguity reliably we would need a separate flag; but because we set file=NULL
        // only when ambiguity occurred, reaching here with file==NULL would have returned. So no-op.
    }

    // At this point file exists. Now ensure folder is valid and exists.
    if (!is_valid_folder_path(folder_path))
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_folder|%s", folder_path);
        return;
    }

    pthread_mutex_lock(&folders_mutex);
    FolderMeta *target = find_folder(folder_path);
    pthread_mutex_unlock(&folders_mutex);
    if (!target)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_such_folder|%s", folder_path);
        return;
    }

    // Permission: only creator or an owner with write permission can move
    int allowed = 0;
    if (strcmp(file->created_by, c->client_name) == 0)
        allowed = 1;
    else
    {
        FileOwnerPerm *own = file->owners;
        while (own)
        {
            if ((strcmp(own->client_name, c->client_name) == 0 || own->client_id == c->client_id) && strchr(own->perm, 'w'))
            {
                allowed = 1;
                break;
            }
            own = own->next;
        }
    }
    if (!allowed)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_permission|%s", filename);
        return;
    }

    // Update indices: remove any old cache entry and rehash based on new key
    cache_remove_file(file);
    pthread_mutex_lock(&files_mutex);
    strncpy(file->folder_path, folder_path, sizeof(file->folder_path) - 1);
    file->folder_path[sizeof(file->folder_path) - 1] = '\0';
    pthread_mutex_unlock(&files_mutex);
    rehash_file(file);
    char newkey[384];
    build_full_path_for_file(file, newkey, sizeof(newkey));
    add_to_cache(newkey, file);

    snprintf(outp->msg, sizeof(outp->msg), "MOVED|%s|%s", filename, folder_path);
    nameserver_log("MOVE_OK user=%s file=%s dest=%s", c->client_name, filename, folder_path);
}

void view_folder(Client *c, const char *folder_path, Packet *outp)
{
    outp->client_id = c->client_id;
    nameserver_log("VIEWFOLDER_REQUEST user=%s id=%d folder=%s", c->client_name, c->client_id, folder_path);
    if (!folder_path)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|usage|VIEWFOLDER <folder>");
        return;
    }

    pthread_mutex_lock(&folders_mutex);
    FolderMeta *f = find_folder(folder_path);
    pthread_mutex_unlock(&folders_mutex);
    if (!f)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_such_folder|%s", folder_path);
        return;
    }

    // Collect file list
    char buf[900];
    buf[0] = '\0';
    size_t used = 0;
    pthread_mutex_lock(&files_mutex);
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i])
        {
            if (strcmp(files[i]->folder_path, folder_path) == 0)
            {
                char line[256];
                /* ensure we don't overflow local line buffer if filename is long */
                snprintf(line, sizeof(line), "%.*s ", (int)(sizeof(line) - 2), files[i]->filename);
                size_t l = strlen(line);
                if (used + l < sizeof(buf))
                {
                    strcpy(buf + used, line);
                    used += l;
                }
            }
        }
    }
    pthread_mutex_unlock(&files_mutex);
    if (used == 0)
        snprintf(outp->msg, sizeof(outp->msg), "FOLDER_EMPTY|%s", folder_path);
    else
        snprintf(outp->msg, sizeof(outp->msg), "FOLDER|%s|%s", folder_path, buf);
}
