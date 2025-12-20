#ifndef PATH_RESOLVE_H
#define PATH_RESOLVE_H

#include "../main.h"

// Split input like "folder/sub/file" into folder="folder/sub" and name="file".
// Returns 1 if a folder component exists, 0 if no slash (name only). Out strings always set.
int split_path(const char *input, char *out_folder, size_t folder_sz, char *out_name, size_t name_sz);

// Find file by full path string; if no folder component, resolves by name only (existing fast path).
FileMeta* find_file_by_path(const char *path);

// Build the canonical full path string for a FileMeta (folder_path + "/" + filename or just filename).
void build_full_path_for_file(const FileMeta *f, char *out, size_t out_sz);

// Build a full path string from parts. If folder is empty, copies name only.
void build_full_path_from_parts(const char *folder, const char *name, char *out, size_t out_sz);

#endif
