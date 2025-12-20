#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>   // mkdir()
#include <sys/types.h>  // mode_t
#include <errno.h>

void storageserver_log(const char *fmt, ...)
{
    // Create directory if it doesn't exist
    struct stat st = {0};

    if (stat("../loggings", &st) == -1) {
        // 0777 = rwxrwxrwx permissions
        if (mkdir("../loggings", 0777) == -1) {
            perror("Failed to create log directory");
            return;
        }
    }

    // Open log file inside the directory
    FILE *fp = fopen("../loggings/storageserver.log", "a");
    if (!fp) {
        perror("Failed to open StorageServer log file");
        return;
    }

    // Timestamp
    time_t now = time(NULL);
    struct tm tbuf, *t;
    t = localtime_r(&now, &tbuf);

    char stamp[64];
    strftime(stamp, sizeof(stamp), "[%Y-%m-%d %H:%M:%S]", t);

    // Format user message
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Write to log file
    fprintf(fp, "%s %s\n", stamp, msg);

    fclose(fp);
}
