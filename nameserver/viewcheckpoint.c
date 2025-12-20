#include"viewcheckpoint.h"
#include "log.h"

void view_checkpoint(Client *c, const char *filename, const char* tag, Packet *outp, int fd) {
    outp->client_id = c->client_id;

    if (!filename || !*filename) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_args|%s", filename?filename:"");
        return;
    }
    nameserver_log("VIEWCHECKPOINT_REQUEST user=%s id=%d file=%s", c->client_name, c->client_id, filename);

    FileMeta *file = find_file_by_path(filename);
    if (!file) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", filename);
        nameserver_log("VIEWCHECKPOINT_FAIL not_found user=%s file=%s", c->client_name, filename);
        return;
    }

    // Validate against files[] table
    int valid = 0; for (int i=0;i<MAX_FILES;i++) if (files[i]==file) { valid=1; break; }
    if (!valid) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|stale|%s", filename);
        nameserver_log("VIEWCHECKPOINT_FAIL stale_pointer user=%s file=%s", c->client_name, filename);
        return;
    }

    // Permission check (require read) optional; enforce for consistency
    int has_permission = 0;
    for (FileOwnerPerm *owner = file->owners; owner; owner=owner->next) {
        if ((owner->client_id == c->client_id || strcmp(owner->client_name, c->client_name)==0) && strchr(owner->perm,'r')) {
            has_permission = 1; break;
        }
    }
    if (!has_permission) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|no_permission|%s", filename);
        nameserver_log("VIEWCHECKPOINT_DENIED user=%s file=%s", c->client_name, filename);
        return;
    }

    pthread_mutex_lock(&files_mutex); // protect traversal
    CheckpointTag *cp = file->checkpoints;
    while(cp){
        if(strcmp(cp->tag, tag)==0){
            // Found the tag
            pthread_mutex_unlock(&files_mutex);
            // Now read from storage server
            int ss_id = file->ss_id;
            if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id]) {
                snprintf(outp->msg, sizeof(outp->msg), "ERR|ss_unavailable|%d", ss_id);
                nameserver_log("VIEWCHECKPOINT_FAIL ss_unavailable user=%s file=%s ss=%d", c->client_name, filename, ss_id);
                return;
            }
            StorageServer *ss = storages[ss_id];
            // Call client read function to get checkpoint data
            // read_checkpoint_from_storage(ss->ip_address, ss->port, file->inode_no, tag);
            char destpath[256];
            snprintf(destpath, sizeof(destpath), "%d/%d_%s.txt.chk", ss->ss_id, file->inode_no, tag);
            // Respond to client with SS info
            snprintf(outp->msg, sizeof(outp->msg), "VIEWCHECKPOINT|%s|%d|%s", ss->ip_address, ss->port, destpath);
            nameserver_log("VIEWCHECKPOINT_OK user=%s file=%s tag=%s", c->client_name, filename, tag);
            return;
        }
        cp = cp->next;
    }

}
