#ifndef persist_h
#define persist_h

#include "../main.h"
int save_storage_server(const StorageServer *s, const char *path, int inode_count);
StorageServer* load_storage_server(const char *path);
#endif