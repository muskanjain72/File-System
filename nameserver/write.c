#include "write.h"
#include "log.h"
#include"nm.h"

int prev_term=-1;

SentenceNode *get_or_create_sentence(FileMeta *file,StorageServer *ss, int sentenceno, Packet *outp)
{
    pthread_mutex_lock(&file->sentence_mutex);
    printf("In get_or_create_sentence: looking for sentence %d\n", sentenceno);
    SentenceNode *curr = file->sentences;
    SentenceNode *prev = NULL;
    
    // Traverse list
    while (curr && curr->sentence_no < sentenceno)
    {
        prev = curr;
        curr = curr->next;
    }

    // Case 1: found existing node
    if (curr && curr->sentence_no == sentenceno)
    {   printf("Sentence %d found in the get_or_create_sentence\n", sentenceno);
        if (curr->locked)
        {
            snprintf(outp->msg, sizeof(outp->msg), "ERR|locked|%d", sentenceno);
            pthread_mutex_unlock(&file->sentence_mutex);
            printf("Sentence %d is locked, cannot proceed\n", sentenceno);
            nameserver_log("WRITE_LOCKED file=%s sentence=%d", file->filename, sentenceno);
            return NULL;
        }
        printf("Locking sentence %d for writing\n", sentenceno);
        nameserver_log("WRITE_LOCK file=%s sentence=%d", file->filename, sentenceno);
        curr->locked = 1;
        pthread_mutex_unlock(&file->sentence_mutex);
        prev_term=curr->is_terminated;
        snprintf(outp->msg, sizeof(outp->msg), "WRITE|%s|%s|%d|%d|%d|%d", file->filename, ss->ip_address, ss->port, file->inode_no, sentenceno, curr->is_terminated);
        return curr;
    }

    if (!curr && !prev)
    {
        if (sentenceno != 0)
        {
            snprintf(outp->msg, sizeof(outp->msg), "ERR|null_sentence|%d", sentenceno);
            pthread_mutex_unlock(&file->sentence_mutex);
            fprintf(stderr, "No sentences exist yet, can only create sentence 0. Returning NULL for sentence %d.\n", sentenceno);
            return NULL;
        }
        // Empty list, create first node
        SentenceNode *new_node = malloc(sizeof(SentenceNode));
        new_node->sentence_no = sentenceno;
        new_node->locked = 1;
        new_node->is_terminated = 0;
        new_node->next = NULL;
    file->sentences = new_node;
    /* first sentence for this file */
    file->sentence_count = 1;
        file->wtime = time(NULL);
        // file->rtime = time(NULL);

        prev_term=0;

        pthread_mutex_unlock(&file->sentence_mutex);

        nameserver_log("WRITE_NEW_SENTENCE file=%s sentence=%d", file->filename, sentenceno);
        snprintf(outp->msg, sizeof(outp->msg), "WRITE|%s|%s|%d|%d|%d|%d", file->filename, ss->ip_address, ss->port, file->inode_no, sentenceno, new_node->is_terminated);
        return new_node;
    }

    if(file->sentence_count+1<sentenceno)
    {
        pthread_mutex_unlock(&file->sentence_mutex);
        return NULL;
    }

    // Case 2: not found but last is not terminated → reject
    if (prev && !prev->is_terminated)
    {
        snprintf(outp->msg, sizeof(outp->msg), "ERR|null_sentence|%d", sentenceno);
        pthread_mutex_unlock(&file->sentence_mutex);
        fprintf(stderr, "Previous sentence %d is not terminated, cannot create sentence %d and returning NULL.\n", prev->sentence_no, sentenceno);
        return NULL;
    }

    // Case 3: allowed to create new node (previous is terminated)
    SentenceNode *new_node = malloc(sizeof(SentenceNode));
    new_node->sentence_no = sentenceno;
    new_node->locked = 1;
    new_node->is_terminated = 0;
    new_node->next = curr;
    prev_term=0;

    if (prev)
        prev->next = new_node;
    else
        file->sentences = new_node;

    file->sentence_count++;

    pthread_mutex_unlock(&file->sentence_mutex);

    snprintf(outp->msg, sizeof(outp->msg), "OK|new_sentence_added|%d|%d", sentenceno, new_node->is_terminated);
    return new_node;
}

void write_file(Client *c, const char *filename, int sentenceno, int fd)
{
    if (!filename)
    {
        fprintf(stderr, "write_file: filename is NULL\n");
        return;
    }
    printf("Processing WRITE request for file '%s', sentence %d from client ID %d [in the nameserver side]\n",
        filename, sentenceno, c->client_id);
    nameserver_log("WRITE_REQUEST user=%s id=%d ip=%s port=%d file=%s sentence=%d",
             c->client_name, c->client_id, c->ip_address, c->port, filename, sentenceno);
    // --- Step 1: Lookup file (supports folder-qualified input) ---
    FileMeta *file = find_file_by_path(filename);
    if (!file)
    {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|not_found|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    // --- Step 2: Validate file reference ---
    int valid = 0;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (files[i] == file)
        {
            valid = 1;
            break;
        }
    }
    if (!valid)
    {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|stale|%s", filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }
    printf("File reference validated for file '%s'\n", filename);
    // --- Step 3: Check SS availability ---
    int ss_id = file->ss_id;
    if (ss_id < 0 || ss_id >= MAX_STORAGE || !storages[ss_id])
    {
        Packet outp;
        outp.client_id = c->client_id;
        snprintf(outp.msg, sizeof(outp.msg), "ERR|ss_unavailable|%d", ss_id);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }

    StorageServer *ss = storages[ss_id];
    printf("Storage Server %d found for file '%s'\n", ss_id, filename);
    // --- Step 4: Check write permission ---
    Packet outp;
    outp.client_id = c->client_id;
    FileOwnerPerm *owner = file->owners;
    int has_permission = 0;
    while (owner)
    {
        // Prefer matching by stable client_name; support legacy entries by client_id
        if (strcmp(owner->client_name, c->client_name) == 0 || owner->client_id == c->client_id)
        {
            if (strchr(owner->perm, 'w') != NULL)
                has_permission = 1;
            break;
        }
        owner = owner->next;
    }
    if (!has_permission)
    {
        snprintf(outp.msg, sizeof(outp.msg), "ERR|no_permission|%s", filename);
        nameserver_log("WRITE_DENIED user=%s id=%d file=%s", c->client_name, c->client_id, filename);
        send(fd, &outp, sizeof(outp), 0);
        return;
    }
    
    printf("Write permission granted for client ID %d on file '%s'\n", c->client_id, filename);
    nameserver_log("WRITE_GRANTED user=%s id=%d file=%s", c->client_name, c->client_id, filename);
    // --- Step 5: Get or create the sentence node ---
    SentenceNode *target = get_or_create_sentence(file, ss, sentenceno, &outp);

    if (!target)
    {
        /*
         * get_or_create_sentence fills outp.msg with specific error codes
         * (for example: "ERR|locked|<n>", "ERR|null_sentence|<n>"). Forward
         * that message to the client instead of overwriting it with a
         * generic "ERR|sentence not found" which hides the real cause.
         */
        send(fd, &outp, sizeof(outp), 0);
        return;
    }
    pthread_mutex_lock(&connection_client_lock);
    send(fd, &outp, sizeof(outp), 0);
    nameserver_log("WRITE_INSTRUCT_SENT user=%s id=%d msg=\"%s\"", c->client_name, c->client_id, outp.msg);

    // === Step 6: Wait for client response after writing ===
    p1 inp;
    ssize_t bytes = recv(fd, &inp, sizeof(inp), 0);
    pthread_mutex_unlock(&connection_client_lock);
    if (bytes <= 0)
    {
        fprintf(stderr, "Client disconnected before response\n");
        nameserver_log("WRITE_CLIENT_DISCONNECT user=%s id=%d file=%s sentence=%d",
                       c->client_name, c->client_id, filename, sentenceno);
        target->locked = 0; // unlock if client vanished
        return;
    }

    int sentences_added = 0, words_added = 0, chars_added = 0, is_terminated = 0;
    printf("%s\n",inp.msg);
    if (sscanf(inp.msg, "RESULT|%d|%d|%d|%d", &sentences_added, &words_added, &chars_added, &is_terminated) == 4)
    {

     printf("Received write result from client ID %d for file '%s': sentences_added=%d, words_added=%d, chars_added=%d, is_terminated=%d\n",
         c->client_id, filename, sentences_added, words_added, chars_added, is_terminated);
     nameserver_log("WRITE_RESULT user=%s id=%d file=%s sent_added=%d words=%d chars=%d term=%d",
              c->client_name, c->client_id, filename, sentences_added, words_added, chars_added, is_terminated);
        pthread_mutex_lock(&file->sentence_mutex);

        file->prev_sentence_count=file->sentence_count;
        file->prev_word_count=file->word_count;
        file->prev_char_count=file->char_count;

        file->sentence_count += sentences_added;
        file->word_count += words_added;
        file->char_count += chars_added;
        file->file_size += chars_added;

        storages[ss_id]->used_bytes += chars_added;  

    // Update last-modified time so INFO shows correct recent write time
    file->wtime = time(NULL);

        // Update current sentence termination
        target->is_terminated = is_terminated;

        // Add new sentences if needed
        if (sentences_added > 0)
        {
            SentenceNode *last = file->sentences;
            while (last && last->next)
                last = last->next;

            for (int i = 0; i < sentences_added; i++)
            {
                SentenceNode *extra = malloc(sizeof(SentenceNode));
                extra->sentence_no = (last ? last->sentence_no + 1 : i);
                extra->locked = 0;
                extra->is_terminated = (i == sentences_added - 1 ? is_terminated : 1);
                extra->next = NULL;
                if (last)
                    last->next = extra;
                else
                    file->sentences = extra;
                last = extra;
            }
        }

        pthread_mutex_unlock(&file->sentence_mutex);
    }
    // printf("done with reading");
    // save_state("nm_state.dat");

    // === Step 7: Unlock sentence after completion ===
    target->locked = 0;
    nameserver_log("WRITE_UNLOCK file=%s sentence=%d", filename, sentenceno);
}