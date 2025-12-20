 #ifndef viewcheckpoint_
 #define viewcheckpoint_

 #include "../main.h"

 // Server-side: view checkpoint (nameserver uses this signature)
 void view_checkpoint(Client *c, const char *filename, const char* tag, Packet *outp, int fd);

 // Client-side helper: contact storage server and read checkpoint contents
 void view_check(const char* ip_address, int port, const char* destpath);

#endif