#ifndef struct_
#define struct_

#include <time.h>

//this is required for end to end transfer and uniquely identify clients
typedef struct Client {
    int client_id;  //assigned by the name server
    char client_name[64];
    char ip_address[32];
    int port;
} Client;

//1 per file
typedef struct AccessRequest { 
    char requester[64];   // user asking permission -> stores client name
    char perm[4];         // "r" or "rw"
    struct AccessRequest *next;
} AccessRequest;

typedef struct StorageServer {
    int ss_id;  //assigned by the name server
    char ip_address[32];
    int port;
    long used_bytes; 
    int fd; 
    int status; //online=1, offline=0
    int inodes[1024]; // array of inode numbers stored on this server
    pthread_mutex_t lock; //lock for thready in case of concurrent access
} StorageServer;

//metadata for each file, stored in the name server
typedef struct FileOwnerPerm {
    int client_id;
    char client_name[64];
    char perm[3]; // e.g. "rw"
    struct FileOwnerPerm *next;
} FileOwnerPerm;

typedef struct SentenceNode {
    int sentence_no;
    int locked;  
    int is_terminated;
    struct SentenceNode *next;
} SentenceNode;

typedef struct FileMeta {
    int inode_no;
    char filename[128];
    FileOwnerPerm *owners; // linked list of owners/permissions
    int ss_id; // storage server id
    // char **block_map; // array of strings (paths on SS), set to data filename like "inode_<inode>.data"
    long file_size;
    SentenceNode *sentences; // linked list of sentence nodes
    pthread_mutex_t sentence_mutex; // used only briefly for list modifications -> at a time, only 1 can modify the metadata of a file
    int sentence_count;
    int word_count;
    int char_count;
    int prev_sentence_count;
    int prev_word_count;
    int prev_char_count;
    char created_by[64];
    time_t ctime; //create time
    time_t rtime; //last read time
    time_t wtime; //last write time
    char folder_path[256]; // hierarchical folder path ("" for root)
    // Linked list of checkpoints for this file (tag + time)
    struct CheckpointTag *checkpoints;
    AccessRequest *pending_requests;   // linked list of pending access requests for this file
} FileMeta;

typedef struct FolderMeta {
    char path[256]; // full path, e.g. "docs" or "docs/tutorials"
    char created_by[64]; // user who created the folder
    time_t created;
    struct FolderMeta *next;
} FolderMeta;

typedef struct HashNode {  //used for name server's hash table
    FileMeta *file;  // pointer to the file metadata
    struct HashNode *next;
} HashNode;

typedef struct CacheEntry {
    // Key used for cache lookups; now stores full path (e.g., "folder/name.txt").
    // Widened to accommodate nested folders safely.
    char filename[384];
    FileMeta *file_ref;
    time_t last_used;
} CacheEntry;

typedef struct {
    int client_id;
    char msg[1024];
} Packet;

typedef struct {
    int id;
    char msg[1024];
} p1;

typedef struct CheckpointTag {
    char tag[128];  // tag for the checkpoint
    time_t when;
    struct CheckpointTag *next;
} CheckpointTag;


#endif