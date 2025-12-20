#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H
#include "../main.h"

void request_access(Client* c, const char* perm, const char* fname, Packet* out);
void view_requests(Client* c, const char* fname, Packet* out);
void approve_request(Client* c, const char* fname, const char* uname, const char* perm, Packet* out);
void deny_request(Client* c, const char* fname, const char* uname, const char* perm, Packet* out);

#endif