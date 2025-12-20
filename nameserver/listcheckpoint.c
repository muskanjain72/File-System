#include "listcheckpoint.h"
#include "log.h"

void list_checkpoints(Client *c, const char *filename, Packet *outp) {
    outp->client_id = c->client_id;

    if (!filename || !*filename) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|invalid_args|%s", filename?filename:"");
        return;
    }
    nameserver_log("LISTCHECKPOINT_REQUEST user=%s id=%d file=%s", c->client_name, c->client_id, filename);

    FileMeta *file = find_file_by_path(filename);
    if (!file) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|not_found|%s", filename);
        nameserver_log("LISTCHECKPOINT_FAIL not_found user=%s file=%s", c->client_name, filename);
        return;
    }

    // Validate against files[] table
    int valid = 0; for (int i=0;i<MAX_FILES;i++) if (files[i]==file) { valid=1; break; }
    if (!valid) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|stale|%s", filename);
        nameserver_log("LISTCHECKPOINT_FAIL stale_pointer user=%s file=%s", c->client_name, filename);
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
        nameserver_log("LISTCHECKPOINT_DENIED user=%s file=%s", c->client_name, filename);
        return;
    }

    // Build listing
    // Format: LISTCHECKPOINT|filename|count|tag|epoch|tag|epoch|...
    char buffer[1024];
    size_t used = 0;
    int count = 0;

    // Reserve prefix
    int n = snprintf(buffer+used, sizeof(buffer)-used, "LISTCHECKPOINT|%s|", filename);
    if (n<0) { snprintf(outp->msg,sizeof(outp->msg),"ERR|internal"); return; }
    used += (size_t)n;

    // We'll fill count later; remember position
    size_t count_pos = used;
    n = snprintf(buffer+used, sizeof(buffer)-used, "XXXX|");
    used += (size_t)n;

    pthread_mutex_lock(&files_mutex); // protect traversal
    for (CheckpointTag *cp = file->checkpoints; cp; cp = cp->next) {
        // Each entry: tag|timestamp|
        char entry[256];
        int en = snprintf(entry, sizeof(entry), "%s|%ld|", cp->tag, (long)cp->when);
        if (en < 0) continue;
        if (used + (size_t)en >= sizeof(buffer)) { // stop if would overflow
            nameserver_log("LISTCHECKPOINT_TRUNCATE file=%s count=%d", filename, count);
            break;
        }
        memcpy(buffer+used, entry, (size_t)en);
        used += (size_t)en;
        count++;
    }
    pthread_mutex_unlock(&files_mutex);

    // Replace placeholder XXXX with actual count (ensure same width or shift left)
    // Simple approach: rewrite from count_pos.
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", count);
    // Shift buffer contents right if count_str length !=4 and room exists
    size_t placeholder_len = 4; // 'XXXX'
    size_t count_len = strlen(count_str);
    if (count_len <= placeholder_len) {
        // overwrite and keep trailing '|'
        memcpy(buffer+count_pos, count_str, count_len);
        // pad remaining with nothing; leave existing '|' after placeholder
    } else {
        // Need to expand; simplest: rebuild prefix cleanly
        char rebuild[1024];
        snprintf(rebuild, sizeof(rebuild), "LISTCHECKPOINT|%s|%d|", filename, count);
        size_t new_used = strlen(rebuild);
        // Append rest excluding old prefix up to after placeholder
        char *after_placeholder = strchr(buffer+count_pos, '|');
        if (after_placeholder) after_placeholder++; // move past '|'
        if (after_placeholder) {
            size_t tail_len = used - (after_placeholder - buffer);
            if (new_used + tail_len < sizeof(rebuild)) {
                memcpy(rebuild+new_used, after_placeholder, tail_len);
                new_used += tail_len;
            }
        }
        memcpy(buffer, rebuild, new_used);
        used = new_used;
    }

    // Finalize
    if (used >= sizeof(outp->msg)) {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|too_large|%s", filename);
    } else {
        memcpy(outp->msg, buffer, used);
        outp->msg[used < sizeof(outp->msg) ? used : sizeof(outp->msg)-1] = '\0';
    }
    nameserver_log("LISTCHECKPOINT_OK user=%s file=%s count=%d", c->client_name, filename, count);
}
