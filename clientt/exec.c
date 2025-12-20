#include "exec.h"
#include "../error_codes.h"
void handle_exec_response(int sockfd)
{
    Packet p;

    // Step 2: Read all EXEC output lines UNTIL "EXEC|END"
    while (1)
    {
        if (recv(sockfd, &p, sizeof(p), 0) <= 0)
        {
            // Connection closed by server while streaming exec output
            fprintf(stderr, "%s\n", ERR_NM_DISCONNECTED);
            return;
        }

        // EXEC termination
        if (strncmp(p.msg, "EXEC|END", 8) == 0)
        {
            printf("EXEC completed.\n");
            break;
        }

        // Otherwise it's a line of output
        printf("%s", p.msg);
    }
}
