#include "access_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// extern FileMeta *files[MAX_FILES];
// extern pthread_mutex_t files_mutex;
// extern pthread_mutex_t clients_mutex;

void request_access(Client *c, const char *perm, const char *fname, Packet *outp)
{
    nameserver_log("REQUESTACCESS user=%s file=%s perm=%s",
                   c->client_name, fname, perm);
    printf("In request access function.Requested by %s\n", c->client_name);
    pthread_mutex_lock(&files_mutex);
    FileMeta *file = find_file_by_path(fname);

    if (!file)
    {
        pthread_mutex_unlock(&files_mutex);
        /* Inform server operator on stdout and send an error back to client */
        printf("REQUESTACCESS: file '%s' not found for user '%s'\n", fname ? fname : "(null)", c->client_name);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|not_found|%s", fname ? fname : "");
        nameserver_log("REQUESTACCESS_FAIL user=%s file=%s reason=not_found",
                       c->client_name, fname);
        return;
    }

    // Prevent owners from requesting access to their own file
    if (strcmp(file->created_by, c->client_name) == 0)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|You are the owner of this file");
        nameserver_log("REQUESTACCESS_FAIL user=%s file=%s reason=owner_request",
                       c->client_name, fname);
        return;
    }

    /* Check duplicate requests: if the same client already has a pending request
       for this file, return an informative error instead of inserting another. */
    AccessRequest *r = file->pending_requests;
    while (r)
    {
        if (strcmp(r->requester, c->client_name) == 0)
        {
            pthread_mutex_unlock(&files_mutex);
            /* Inform client that a request is already pending for this file */
            snprintf(outp->msg, sizeof(outp->msg), "ERR|REQUEST_PENDING|%s", fname ? fname : "");
            nameserver_log("REQUESTACCESS_FAIL user=%s file=%s reason=duplicate",
                           c->client_name, fname);
            return;
        }
        r = r->next;
    }

    // Insert request
    AccessRequest *nr = malloc(sizeof(AccessRequest));
    strcpy(nr->requester, c->client_name);
    strcpy(nr->perm, perm);
    nr->next = file->pending_requests;
    file->pending_requests = nr;

    pthread_mutex_unlock(&files_mutex);

    snprintf(outp->msg, sizeof(outp->msg),
             "OK|Request submitted");
    nameserver_log("REQUESTACCESS_OK user=%s file=%s perm=%s",
                   c->client_name, fname, perm);
    printf("Appropriate msg sent to the client\n");
    return;
}

void view_requests(Client *c, const char *fname, Packet *outp)
{
    pthread_mutex_lock(&files_mutex);
    FileMeta *file = find_file_by_path(fname);

    if (!file)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|not_found|%s", fname ? fname : "");
        return;
    }

    if (strcmp(file->created_by, c->client_name) != 0)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|Only owner '%s' can view requests", file->created_by);
        return;
    }

    // Build output
    char buf[2048] = "";
    AccessRequest *r = file->pending_requests;

    if (!r)
    {
        snprintf(outp->msg, sizeof(outp->msg),
                 "No pending requests");
        pthread_mutex_unlock(&files_mutex);
        return;
    }

    while (r)
    {
        char line[256];
        /* New output format per request: nameOfrequester:<name> permissionrequested:<perm> */
        snprintf(line, sizeof(line), "nameOfrequester:%s permission-requested:%s\n", r->requester, r->perm);
        strcat(buf, line);
        r = r->next;
    }

    pthread_mutex_unlock(&files_mutex);
    strcpy(outp->msg, buf);
    printf("Appropriate msg sent to the client\n");
    return;
}

void approve_request(Client *c, const char *fname, const char *uname, const char *perm, Packet *outp)
{
    nameserver_log("APPROVE user=%s file=%s target=%s",
                   c->client_name, fname, uname);

    pthread_mutex_lock(&files_mutex);
    FileMeta *file = find_file_by_path(fname);

    if (!file)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|not_found|");
        return;
    }

    if (strcmp(file->created_by, c->client_name) != 0)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|Only owner '%s' can approve", file->created_by);
        return;
    }

    AccessRequest *cur = file->pending_requests;
    AccessRequest *prev = NULL;

    while (cur)
    {
        if (strcmp(cur->requester, uname) == 0 && (perm == NULL || strcmp(cur->perm, perm) == 0))
        {
            // Found request — approve it
            char perm_to_apply[4];
            strcpy(perm_to_apply, cur->perm); // "R" / "W" / "RW"

            // Remove from list
            if (prev == NULL)
                file->pending_requests = cur->next;
            else
                prev->next = cur->next;

            free(cur);
            pthread_mutex_unlock(&files_mutex);

            // Now call existing permission logic
            add_access(c, perm_to_apply, fname, uname, outp);

            nameserver_log("APPROVE_OK owner=%s file=%s target=%s perm=%s",
                           c->client_name, fname, uname, perm_to_apply);
            return;
        }
        prev = cur;
        cur = cur->next;
    }

    pthread_mutex_unlock(&files_mutex);

    snprintf(outp->msg, sizeof(outp->msg), "ERR|No such request");
    printf("Appropriate msg sent to the client\n");
    return;
}

/***************
 * DENY
 ***************/
void deny_request(Client *c, const char *fname, const char *uname, const char *perm, Packet *outp)
{
    nameserver_log("DENY user=%s file=%s target=%s",
                   c->client_name, fname, uname);

    pthread_mutex_lock(&files_mutex);
    FileMeta *file = find_file_by_path(fname);

    if (!file)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|not_found|");
        return;
    }

    if (strcmp(file->created_by, c->client_name) != 0)
    {
        pthread_mutex_unlock(&files_mutex);
        snprintf(outp->msg, sizeof(outp->msg),
                 "ERR|Only owner '%s' can deny", file->created_by);
        return;
    }

    AccessRequest *cur = file->pending_requests;
    AccessRequest *prev = NULL;

    while (cur)
    {
        if (strcmp(cur->requester, uname) == 0 && (perm == NULL || strcmp(cur->perm, perm) == 0))
        {
            if (prev == NULL)
                file->pending_requests = cur->next;
            else
                prev->next = cur->next;

            free(cur);
            pthread_mutex_unlock(&files_mutex);

            snprintf(outp->msg, sizeof(outp->msg),
                     "OK|Denied access for '%s'", uname);
            nameserver_log("DENY_OK owner=%s file=%s target=%s",
                           c->client_name, fname, uname);
                    return;
        }
        prev = cur;
        cur = cur->next;
    }

    pthread_mutex_unlock(&files_mutex);

    snprintf(outp->msg, sizeof(outp->msg), "ERR|No such request");
    printf("Appropriate msg sent to the client\n");
    return;
}
