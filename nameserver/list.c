#include "list.h"
#include "log.h"

void list_files(Client *c, Packet *outp)
{
    pthread_mutex_lock(&clients_mutex);
    outp->msg[0] = '\0';
    /* start response */
    strncat(outp->msg, "Clients present:\n", sizeof(outp->msg) - strlen(outp->msg) - 1);
    for(int i=0;i<MAX_CLIENTS;i++)
    {
        if(clients[i])
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "Name: %s\n",
                     clients[i]->client_name);
            strncat(outp->msg, buffer, sizeof(outp->msg) - strlen(outp->msg) - 1);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    nameserver_log("LIST_REQUEST user=%s id=%d", c->client_name, c->client_id);
}
