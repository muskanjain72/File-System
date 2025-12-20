#ifndef nm_
#define nm_

#include"../main.h"

#define MAX_CLIENTS 100
#define MAX_STORAGE 50
#define MAX_FILES 1024

// === Hash + Cache Structures ===
#define HASH_SIZE 2003
#define CACHE_SIZE 10

// Tables (defined in nm.c)
extern Client *clients[MAX_CLIENTS];
extern StorageServer *storages[MAX_STORAGE];
extern FileMeta *files[MAX_FILES];

extern pthread_mutex_t clients_mutex;
extern pthread_mutex_t storage_mutex;
extern pthread_mutex_t files_mutex;
extern pthread_mutex_t hash_mutex;
extern pthread_mutex_t cache_mutex;
extern pthread_mutex_t files_mutex;

extern pthread_mutex_t connection_client_lock;
extern pthread_mutex_t connection_storage_lock;

// ----------------- Client Functions -----------------
void remove_file_access_by_client_id(Client*c, int client_id, int fd);
int add_client(Client *c);
void remove_client(Client* c, int client_id, int fd);
void print_clients();   
// ----------------- Storage Server Functions -----------------
void add_storage(StorageServer *s);
void remove_storage(int ss_id);
void print_storages();

// === Globals (DECLARED, not defined) ===
extern HashNode *file_hash_table[HASH_SIZE];
extern CacheEntry cache[CACHE_SIZE];
extern int cache_count;

#endif