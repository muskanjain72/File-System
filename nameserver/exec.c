#include "read.h"
#include "log.h"
#include"nm.h"

void exec(Client *c, const char *filename, int fd)
{
    // 1. Basic sanity checks
    if (!filename) {
        fprintf(stderr, "read_file: filename is NULL\n");
        Packet errp;
        errp.client_id = c->client_id;
        snprintf(errp.msg, sizeof(errp.msg), "ERR|invalid_filename");
        send(fd, &errp, sizeof(errp), 0);
        return;
    }

    // 2. Resolve file
    printf("Looking up file: %s\n", filename);
    nameserver_log("EXEC_REQUEST user=%s id=%d file=%s", c->client_name, c->client_id, filename);
    FileMeta *file = find_file_by_path(filename);
    if (!file) {
        fprintf(stderr, "%s Filename:%s\n", ERR_NOT_FOUND, filename);
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // Validate that file pointer is one of global files[]
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++)
        if (files[i] == file) valid = 1;

    if (!valid) {
        fprintf(stderr, "%s FileName:%s\n", ERR_STALE_POINTER, filename);
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|stale|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // 3. Check storage server availability
    int ss_id = file->ss_id;
    if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id]) {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|ss_unavailable|%d", ss_id);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    StorageServer *ss = storages[ss_id];

    // 4. Permission check
    int has_permission = 0;
    FileOwnerPerm *owner = file->owners;
    while (owner) {
        if (strcmp(owner->client_name, c->client_name) == 0 ||
            owner->client_id == c->client_id)
        {
            if (strchr(owner->perm, 'r'))
                has_permission = 1;
            break;
        }
        owner = owner->next;
    }

    if (!has_permission) {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|no_permission|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // 5. Ask storage server to EXEC the file
    int ss_sock = ss->fd;

    char cmd_buf[64];
    snprintf(cmd_buf, sizeof(cmd_buf), "EXEC %d", file->inode_no);

    pthread_mutex_lock(&ss->lock);
    if (send(ss_sock, cmd_buf, strlen(cmd_buf), 0) <= 0) {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|ss_unreachable|%d", ss_id);
        send(fd, &outp, sizeof(outp), 0);
        nameserver_log("EXEC_SEND_FAIL ss=%d file=%s cmd=\"%s\"", ss_id, filename, cmd_buf);
        return;
    }

    // 6. Receive from storage server the actual shell command
    p1 resp_buf;
    ssize_t n = recv(ss_sock, &resp_buf, sizeof(resp_buf), 0);
    pthread_mutex_unlock(&ss->lock);
    if (n <= 0) {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|ss_no_response|%d", ss_id);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // 7. Build final executable shell command
    // Use only the first non-empty line from the storage server response as
    // the command to execute. Trim whitespace and escape double-quotes and
    // backslashes so the string can be safely embedded into sh -c "...".
    char shell_cmd[512];
    const char *shell_fmt = "sh -c \"%s\" 2>&1";

    // Extract first line
    char first_line[1024];
    first_line[0] = '\0';
    {
        const char *p = resp_buf.msg;
        // Skip leading newlines/spaces
        while (*p && (*p == '\n' || *p == '\r')) p++;
        // Copy up to newline or end
        size_t i = 0;
        while (*p && *p != '\n' && *p != '\r' && i < sizeof(first_line) - 1) {
            first_line[i++] = *p++;
        }
        first_line[i] = '\0';
        // Trim leading spaces
        char *s = first_line;
        while (*s && isspace((unsigned char)*s)) s++;
        // Trim trailing spaces
        char *end = s + strlen(s);
        while (end > s && isspace((unsigned char)*(end - 1))) end--;
        *end = '\0';
        // Move trimmed into first_line[0]
        if (s != first_line) memmove(first_line, s, strlen(s) + 1);
    }

    if (first_line[0] == '\0') {
        // Nothing sensible to execute
        Packet packet;
        packet.client_id = c->client_id;
        snprintf(packet.msg, sizeof(packet.msg), "ERR|exec_no_command|%s", filename);
        send(fd, &packet, sizeof(packet), 0);
        nameserver_log("EXEC_FAIL_no_command user=%s file=%s", c->client_name, filename);
        return;
    }

    // Escape double-quotes and backslashes for embedding in a double-quoted string
    char esc[400];
    size_t ei = 0;
    for (size_t i = 0; first_line[i] != '\0' && ei + 2 < sizeof(esc); i++) {
        unsigned char ch = (unsigned char)first_line[i];
        if (ch == '"' || ch == '\\') {
            esc[ei++] = '\\';
            esc[ei++] = ch;
        } else {
            esc[ei++] = ch;
        }
    }
    esc[ei] = '\0';

    // Build the final shell command
    snprintf(shell_cmd, sizeof(shell_cmd), shell_fmt, esc);

    printf("Executing shell cmd: %s\n", shell_cmd);
    nameserver_log("EXEC_RUN file=%s shell=\"%s\"", filename, shell_cmd);

    // 8. Start streaming output to client
    Packet packet;
    packet.client_id = c->client_id;

    // BEGIN marker
    snprintf(packet.msg, sizeof(packet.msg), "EXEC|BEGIN|%s\n", filename);
    send(fd, &packet, sizeof(packet), 0);
    nameserver_log("EXEC_BEGIN_SENT user=%s file=%s", c->client_name, filename);

    // Open process for reading
    FILE *fp = popen(shell_cmd, "r");
    if (!fp) {
        snprintf(packet.msg, sizeof(packet.msg), "ERR|exec_failed\n");
        send(fd, &packet, sizeof(packet), 0);
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        snprintf(packet.msg, sizeof(packet.msg), "%s", line);
        send(fd, &packet, sizeof(packet), 0);
    }

    pclose(fp);

    // END marker
    snprintf(packet.msg, sizeof(packet.msg), "EXEC|END\n");
    send(fd, &packet, sizeof(packet), 0);
    nameserver_log("EXEC_END user=%s file=%s", c->client_name, filename);

    // Update read time
    file->rtime = time(NULL);
}
