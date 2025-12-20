#ifndef write_client
#define write_client

#include"../main.h"
int is_sentence_terminated(const char *s);
void write_file_to_storage(const char *ss_ip, int ss_port, int inode_no, int sentence_no, int is_terminated, int client_sock_to_ns);

#endif