#include"view.h"
#include "log.h"

void view(Client* c,Packet* outp){
    pthread_mutex_lock(&clients_mutex);
    outp->msg[0] = '\0'; // clear previous data
    printf("view(): enter\n");
    for(int i=0;i<MAX_FILES;i++){
        if(files[i]){
            FileOwnerPerm* op=files[i]->owners;
            while(op){
                if(op->client_id==c->client_id){
                    char temp[256];
                    snprintf(temp, sizeof(temp), "File: %s\n", files[i]->filename);
                    strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                }
                op=op->next;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    nameserver_log("VIEW_REQUEST user=%s id=%d", c->client_name, c->client_id);
    printf("view(): exit\n");
}

void viewa(Packet* outp){
    pthread_mutex_lock(&clients_mutex);
    outp->msg[0] = '\0'; // clear previous data
    for(int i=0;i<MAX_FILES;i++){
        if(files[i]){
            char temp[256];
            snprintf(temp, sizeof(temp), "File: %s\n", files[i]->filename);
            strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    nameserver_log("VIEWALL_REQUEST (admin)");
}

void viewl(Client* c, Packet* outp){
    printf("in viewl\n");
    pthread_mutex_lock(&clients_mutex);
    outp->msg[0] = '\0'; // clear previous data
    for(int i=0;i<MAX_FILES;i++){
        if(files[i]){
            FileOwnerPerm* op=files[i]->owners;
            while(op){
                if(op->client_id==c->client_id){
                    char temp[256];
                    snprintf(temp, sizeof(temp), "%s:\n",files[i]->filename);
                    strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    snprintf(temp, sizeof(temp), "%d sentences\n",files[i]->sentence_count);
                    strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    snprintf(temp, sizeof(temp), "%d words\n",files[i]->word_count);
                    strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    snprintf(temp, sizeof(temp), "%d characters\n",files[i]->char_count);
                    strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    if(files[i]->rtime==0){
                        snprintf(temp,sizeof(temp),"0: Last access time\n");
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    }
                    else{
                        time_t t = files[i]->rtime;
                        struct tm tm_info;
                        localtime_r(&t, &tm_info);
                        strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &tm_info);
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                        snprintf(temp, sizeof(temp), ": Last access time\n");
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    }
                    if(files[i]->ctime==0){
                        snprintf(temp,sizeof(temp),"0: Created time\n");
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    }
                    else{
                        time_t t = files[i]->ctime;
                        struct tm tm_info;
                        localtime_r(&t, &tm_info);
                        strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &tm_info);
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                        snprintf(temp, sizeof(temp), ": Created time\n");
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    }
                    if(files[i]->wtime==0){
                        snprintf(temp,sizeof(temp),"0: Modified time\n");
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    }
                    else{  
                        time_t t = files[i]->wtime;
                        struct tm tm_info;
                        localtime_r(&t, &tm_info);
                        strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &tm_info);
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                        snprintf(temp, sizeof(temp), ": Modified time\n");
                        strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                    }
                }
                op=op->next;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    nameserver_log("VIEW_LONG_REQUEST user=%s id=%d", c->client_name, c->client_id);
}

void viewal(Packet* outp){
    pthread_mutex_lock(&clients_mutex);
    outp->msg[0] = '\0'; // clear previous data
    for(int i=0;i<MAX_FILES;i++){
        if(files[i]){
            char temp[256];
            snprintf(temp, sizeof(temp), "Filename: %s\n",files[i]->filename);
            strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            snprintf(temp, sizeof(temp), "%d sentences\n",files[i]->sentence_count);
            strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            snprintf(temp, sizeof(temp), "%d words\n",files[i]->word_count);
            strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            snprintf(temp, sizeof(temp), "%d characters\n",files[i]->char_count);
            if(files[i]->rtime==0){
                snprintf(temp,sizeof(temp),"0: Last access time\n");
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            }
            else{
                time_t t = files[i]->rtime;
                struct tm tm_info;
                localtime_r(&t, &tm_info);
                strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &tm_info);
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                snprintf(temp, sizeof(temp), ": Last access time\n");
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            }
            if(files[i]->ctime==0){
                snprintf(temp,sizeof(temp),"0: Created time\n");
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            }
            else{
                time_t t = files[i]->ctime;
                struct tm tm_info;
                localtime_r(&t, &tm_info);
                strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &tm_info);
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                snprintf(temp, sizeof(temp), ": Created time\n");
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            }
            if(files[i]->wtime==0){
                snprintf(temp,sizeof(temp),"0: Modified time\n");
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            }
            else{  
                time_t t = files[i]->wtime;
                struct tm tm_info;
                localtime_r(&t, &tm_info);
                strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M", &tm_info);
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                snprintf(temp, sizeof(temp), ": Modified time\n");
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
            }
            strncat(outp->msg, "Owners:\n", sizeof(outp->msg) - strlen(outp->msg) - 1);
            FileOwnerPerm* op=files[i]->owners;
            while(op){
                snprintf(temp, sizeof(temp), " - %s : %s\n",op->client_name,op->perm);
                strncat(outp->msg, temp, sizeof(outp->msg) - strlen(outp->msg) - 1);
                op=op->next;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    nameserver_log("VIEW_ALL_LONG_REQUEST (admin)");
}