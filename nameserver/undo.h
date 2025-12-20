#ifndef UNDO_H
#define UNDO_H

#include"../main.h"
// Updated signature: provide Packet* to fill, client_fd retained (not used for direct send here)
void undo_file(Client *c, const char *filename, Packet *outp, int client_fd);

#endif