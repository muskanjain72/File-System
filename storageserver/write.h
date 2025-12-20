#ifndef WRITE_SS
#define WRITE_SS


#include"../main.h"
extern StorageServer s;
int copy_file(const char *src, const char *dest);
void get_backup_path(const char *filepath, char *backup);
int push_version(const char *filepath);
int is_sentence_end(char c);
int get_sentence_from_file(const char *filepath, int sentence_no, char *out);
int replace_sentence_in_file(const char *filepath, int sentence_no, const char *new_sentence);
void write_file_to_client(const char *filepath, int sentence_no, int fd);
#endif