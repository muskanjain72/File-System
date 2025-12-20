#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include "../main.h"

// Create a checkpoint of file contents with a tag; fills Packet with response
void checkpoint_file(Client *c, const char *filename, const char *tag, Packet *outp, int client_fd);

#endif
