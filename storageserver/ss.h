#ifndef ss_h
#define ss_h

#include"../main.h"
// Declare the storage server state here. The definition lives in storageserver/ss.c
extern StorageServer s;
int mkdir_and_touch(const char *path);
void *handle_client(void *arg);
void *client_listener(void *arg);

#endif
