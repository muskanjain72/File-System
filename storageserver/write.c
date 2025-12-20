#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "write.h"
#include "log.h"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_FILEPATH 256

#define MAX_SENTENCE_LEN 2048
#define TMP_FILE "tmp_replace.txt"

// Write new_sentence to tmp, translating the token "<NL>" into a real newline
static void write_processed(FILE *tmp, const char *s)
{
    for (size_t i = 0; s[i]; ) {
        if (s[i] == '<' && s[i+1] == 'N' && s[i+2] == 'L' && s[i+3] == '>') {
            fputc('\n', tmp);
            i += 4;
        } else {
            fputc(s[i], tmp);
            i++;
        }
    }
}

// ----------------------
// Helper: copy file contents
// ----------------------
int copy_file(const char *src, const char *dest) {
    FILE *fsrc = fopen(src, "r");
    if (!fsrc) {
        printf("1");
        return 0;
    }
    FILE *fdest = fopen(dest, "w");
    if (!fdest) {
        fclose(fsrc);
        printf("2");
        return 0;
    }

    char buffer[1024];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), fsrc)) > 0)
        fwrite(buffer, 1, n, fdest);

    fclose(fsrc);
    fclose(fdest);
    return 1;
}

// ----------------------
// Get backup file path
// ----------------------
void get_backup_path(const char *filepath, char *backup) {
    snprintf(backup, MAX_FILEPATH, "%s.bak", filepath);
    printf("Backup path for '%s' is '%s'\n", filepath, backup);
    storageserver_log("BACKUP_PATH src=%s backup=%s", filepath, backup);
}

// ----------------------
// Push current version before update (1-level undo)
// ----------------------
int push_version(const char *filepath) {
    char backup[MAX_FILEPATH];
    get_backup_path(filepath, backup);
    return copy_file(filepath, backup);
}

// Helper to detect sentence end
int is_sentence_end(char c) {
    return (c == '.' || c == '!' || c == '?');
}

int get_sentence_from_file(const char *filepath, int sentence_no, char *out) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("Error opening file");
        return 0;
    }

    int count = 0, c, idx = 0, in_sentence = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (!in_sentence && !isspace(c)) {
            in_sentence = 1;
            count++;
        }

        if (count == sentence_no && in_sentence) {
            if (idx < MAX_SENTENCE_LEN - 1)
                out[idx++] = (char)c;
        }

        if (is_sentence_end(c)) {
            if (count == sentence_no)
                break;
            in_sentence = 0;
        }
    }

    // Handle the case: sentence_no is next after last sentence
    if (count < sentence_no) {
        // EOF reached before reaching requested sentence → return empty string
        out[0] = '\0';
        fclose(fp);
        return 1; // still “success”, just empty
    }

    out[idx] = '\0';
    fclose(fp);
    return 1;
}

int replace_sentence_in_file(const char *filepath, int sentence_no, const char *new_sentence) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("Error opening file");
        return 0;
    }

    FILE *tmp = fopen(TMP_FILE, "w");
    if (!tmp) {
        perror("Error opening temp file");
        fclose(fp);
        return 0;
    }

    int count = 0, c, in_sentence = 0;
    int replaced = 0;

    while ((c = fgetc(fp)) != EOF) {
        if (!in_sentence && !isspace(c)) {
            in_sentence = 1;
            count++;
        }

        if (count == sentence_no && in_sentence && !replaced) {
            // Skip current sentence
            while ((c != EOF) && !is_sentence_end(c)) {
                c = fgetc(fp);
            }
            if (c != EOF) fgetc(fp); // consume delimiter
            /* write new sentence, translating any <NL> tokens to actual newlines */
            write_processed(tmp, new_sentence);
            fputc(' ', tmp);
            replaced = 1;
            in_sentence = 0;
            continue;
        }

        fputc(c, tmp);

        if (is_sentence_end(c))
            in_sentence = 0;
    }

    // If requested sentence number is next after last, append it
    if (!replaced && count + 1 == sentence_no) {
        write_processed(tmp, new_sentence);
        fputc(' ', tmp);
    }

    fclose(fp);
    fclose(tmp);

    // Replace old file with new one
    remove(filepath);
    rename(TMP_FILE, filepath);
    return 1;
}

void write_file_to_client(const char *filepath, int sentence_no, int fd){
    char sentence[MAX_SENTENCE_LEN];
    // Validate sentence number
    sentence_no++;
    // Push a backup ONLY if the file exists and has non-zero size (avoid backing up empty new files)
    {
        FILE *check = fopen(filepath, "r");
        if (check) {
            fseek(check, 0, SEEK_END);
            long sz = ftell(check);
            fclose(check);
            if (sz > 0) {
                push_version(filepath); // Push current version for undo
            }
        }
    }
    if (sentence_no <= 0) {
        const char *msg = "Error: Invalid sentence number.";
        send(fd, msg, strlen(msg), 0);
        close(fd);
        return;
    }

    if (get_sentence_from_file(filepath, sentence_no, sentence)) {
        size_t len = strlen(sentence);
        if (len == 0) {
            const char *empty_marker = "~";
            if(send(fd, empty_marker, strlen(empty_marker), 0) <= 0) {
                perror("send empty sentence failed");
                close(fd);
                return;
            }
            printf("⚠️ Retrieved sentence %d is empty.\n", sentence_no);
        }

        else{
        ssize_t sent = send(fd, sentence, len, 0);
            if (sent <= 0) {
                // If send returned 0 it isn't necessarily an errno case; provide clearer diagnostics
                if (sent == 0)
                    fprintf(stderr, "send returned 0 while sending sentence (possibly closed by peer)\n");
                else
                    perror("send sentence failed");
                close(fd);
                return;
            }
        }
        printf("✅ Retrieved sentence %d from file %s.\n", sentence_no, filepath);
            storageserver_log("SENTENCE_RETRIEVED filepath=%s sentence=%d to_fd=%d", filepath, sentence_no, fd);
    } else {
        const char *msg = "Error: Sentence not found.";
        send(fd, msg, strlen(msg), 0);
        close(fd);
        storageserver_log("SENTENCE_NOT_FOUND filepath=%s sentence=%d", filepath, sentence_no);
        return;
    }

    int prev_len=strlen(sentence);

    // Receive updated sentence from client
    ssize_t n = recv(fd, sentence, sizeof(sentence) - 1, 0);
    if (n <= 0) {
        if (n == 0)
            fprintf(stderr, "recv returned 0 while waiting for updated sentence (peer closed)\n");
        else
            perror("recv ack failed");
        close(fd);
        storageserver_log("WRITE_RECV_FAIL filepath=%s sentence=%d err=%s", filepath, sentence_no, n==0?"peer_closed":strerror(errno));
        return;
    }
    char final[64]="WRITE_SUCCESS";
    if(send(fd, &final, sizeof(final) - 1, 0) <= 0)
    {
        perror("send write success failed");
        storageserver_log("WRITE_ACK_SEND_FAIL filepath=%s sentence=%d err=%s", filepath, sentence_no, strerror(errno));
        close(fd);
        return;
    }
    sentence[n] = '\0';
    replace_sentence_in_file(filepath, sentence_no, sentence);
    printf("✅ Sent sentence %d to client and updated file.\n", sentence_no);
        storageserver_log("WRITE_APPLIED filepath=%s sentence=%d delta=%d", filepath, sentence_no, (int)(strlen(sentence)-prev_len));
    int now=strlen(sentence);
    s.used_bytes += (now - prev_len);
    close(fd);
}