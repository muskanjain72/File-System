#include "path.h"

int split_path(const char *input, char *out_folder, size_t folder_sz, char *out_name, size_t name_sz)
{
    if (!input)
    {
        if (out_folder && folder_sz)
            out_folder[0] = '\0';
        if (out_name && name_sz)
            out_name[0] = '\0';
        return 0;
    }
    const char *slash = strrchr(input, '/');
    if (!slash)
    {
        if (out_folder && folder_sz)
            out_folder[0] = '\0';
        if (out_name && name_sz)
        {
            strncpy(out_name, input, name_sz - 1);
            out_name[name_sz - 1] = '\0';
        }
        return 0;
    }
    // folder part is up to slash-1, name is after slash
    size_t f_len = (size_t)(slash - input);
    if (out_folder && folder_sz)
    {
        size_t cpy = f_len < (folder_sz - 1) ? f_len : (folder_sz - 1);
        memcpy(out_folder, input, cpy);
        out_folder[cpy] = '\0';
    }
    if (out_name && name_sz)
    {
        strncpy(out_name, slash + 1, name_sz - 1);
        out_name[name_sz - 1] = '\0';
    }
    return 1;
}

FileMeta *find_file_by_path(const char *path)
{
    if (!path)
        return NULL;
    char folder[256];
    char name[128];
    int has_folder = split_path(path, folder, sizeof(folder), name, sizeof(name));
    // Try fast path: look up by full path key regardless of folder presence
    FileMeta *by_key = check_cache(path);
    if (!by_key)
        by_key = search_file_in_hash(path);
    if (by_key)
        return by_key;

    // Fallback: Full scan for a match on folder_path + filename
    pthread_mutex_lock(&files_mutex);
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i])
        {
            if (strcmp(files[i]->filename, name) == 0 && strcmp(files[i]->folder_path, folder) == 0)
            {
                FileMeta *f = files[i];
                pthread_mutex_unlock(&files_mutex);
                return f;
            }
        }
    }
    pthread_mutex_unlock(&files_mutex);
    return NULL;
}

void build_full_path_for_file(const FileMeta *f, char *out, size_t out_sz)
{
    if (!f || !out || out_sz == 0)
        return;
    if (f->folder_path[0] == '\0')
    {
        strncpy(out, f->filename, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
    else
    {
        snprintf(out, out_sz, "%s/%s", f->folder_path, f->filename);
    }
}

void build_full_path_from_parts(const char *folder, const char *name, char *out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return;
    if (folder && *folder)
    {
        snprintf(out, out_sz, "%s/%s", folder, name ? name : "");
    }
    else
    {
        strncpy(out, name ? name : "", out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}
