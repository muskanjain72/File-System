#ifndef LISTCHECKPOINT_H
#define LISTCHECKPOINT_H

#include "../main.h"

// Lists all checkpoint tags (and timestamps) for a file.
// Packet format: LISTCHECKPOINT|<filename>|<count>|<tag>|<timestamp>|...
void list_checkpoints(Client *c, const char *filename, Packet *outp);

#endif
