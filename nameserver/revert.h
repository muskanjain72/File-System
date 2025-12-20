#ifndef REVERT_H
#define REVERT_H

#include "../main.h"

void revert(Client *c, const char *filename, const char* tag, Packet *outp, int fd);

#endif // REVERT_H