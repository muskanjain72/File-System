#ifndef create_
#define create_

#include"../main.h"
int create(Client* c, char* buf);
void send_create_request_to_storage(int ss_id, int file_id);

// Expose cache/hash helpers so callers get correct prototypes (avoid implicit-int truncation)
FileMeta *check_cache(const char *filename);
FileMeta *search_file_in_hash(const char *filename);
void add_to_cache(const char *filename, FileMeta *file);
void insert_file_to_hash(FileMeta *file);
unsigned int hash_filename(const char *filename);
void cache_remove_file(FileMeta *file);
void rehash_file(FileMeta *file);

#endif