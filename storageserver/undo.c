#include"undo.h"
#include "log.h"

#define MAX_FILEPATH 256

// ----------------------
// Undo last change (restore 1-level undo)
// ----------------------
int undo_last_change(const char *filepath) {
    char backup[MAX_FILEPATH];
    get_backup_path(filepath, backup);

    // Check if backup exists
    if (access(backup, F_OK) != 0)
    {
        storageserver_log("UNDO_FAIL no_backup filepath=%s", filepath);
        return 0; // no undo available
    }
    if (!copy_file(backup, filepath)) {
        storageserver_log("UNDO_FAIL copy_error filepath=%s backup=%s", filepath, backup);
        return 0;
    }
    storageserver_log("UNDO_APPLIED filepath=%s backup=%s", filepath, backup);
    return 1;
}
