
#include "getAddInfo.h"
#include "log.h"

void file_info(Client *c, const char *filename, int fd)
{
    if (!filename)
    {
        Packet outp;
        outp.client_id = c->client_id;
        printf("file_info: filename is NULL\n");
        printf("%s Filename:%s\n", ERR_NOT_FOUND, "(null)");
        nameserver_log("INFO_REQUEST invalid_filename user=%s id=%d", c->client_name, c->client_id);
        snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|");
        send(fd,&outp,sizeof(outp),0);
        return;
    }

    // Lookup file by path (supports folder-qualified input)
    FileMeta *file = find_file_by_path(filename);

    if (!file)
    {
        Packet outp;
        outp.client_id = c->client_id;
        printf("%s Filename:%s\n", ERR_NOT_FOUND, filename ? filename : "(null)");
        nameserver_log("INFO_FAIL not_found user=%s file=%s", c->client_name, filename ? filename : "(null)");
        snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", filename ? filename : "");
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // Validate file pointer
    int valid = 0;
    FileMeta *file_ptr = NULL;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i] == file)
        {
            valid = 1;
            file_ptr = files[i];
            break;
        }
    }

    if (!valid)
    {
        Packet outp;
        outp.client_id = c->client_id;
        printf("%s FileName:%s\n", ERR_STALE_POINTER, filename);
        nameserver_log("INFO_FAIL stale_pointer user=%s file=%s", c->client_name, filename);
        snprintf(outp.msg, sizeof(outp.msg), "ERR|stale|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    int ss_id = file_ptr->ss_id;
    if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id])
    {
        Packet outp;
        outp.client_id = c->client_id;
        printf("%s SS_ID:%d\n", ERR_SS_UNAVAILABLE, ss_id);
        nameserver_log("INFO_FAIL ss_unavailable user=%s file=%s ss=%d", c->client_name, filename ? filename : "(null)", ss_id);
        snprintf(outp.msg, sizeof(outp.msg), "ERR|ss_unavailable|%d", ss_id);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // Build info message to send to client
    char info_msg[2048];
    int offset = snprintf(info_msg, sizeof(info_msg),
                          "Filename: %s\n"
                          "Size: %ld bytes\n"
                          "Sentences: %d\n"
                          "Words: %d\n"
                          "Characters: %d\n"
                          "Last Modified: %s"
                          "Storage Server: %d\n"
                          "Owners and Permissions:\n",
                          file_ptr->filename, file_ptr->file_size,
                          file_ptr->sentence_count, file_ptr->word_count,
                          file_ptr->char_count, ctime(&file_ptr->wtime),
                          ss_id);

    // Append all owners and permissions
    FileOwnerPerm *owner = file_ptr->owners;
    while (owner && offset < sizeof(info_msg) - 100)
    {
        offset += snprintf(info_msg + offset, sizeof(info_msg) - offset,
                           "  - Client ID: %d, Name: %s, Permissions: %s\n",
                           owner->client_id, owner->client_name, owner->perm);
        owner = owner->next;
    }

    // Send this info to client
    Packet outp;
    outp.client_id = c->client_id;
    strncpy(outp.msg, info_msg, sizeof(outp.msg));
    send(fd, &outp, sizeof(outp), 0);
    nameserver_log("INFO_SENT user=%s id=%d file=%s", c->client_name, c->client_id, filename);

    printf("[Server] Sent file info to client %d for '%s'\n", c->client_id, filename);
}
