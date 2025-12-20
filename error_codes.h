#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// ---------- Universal Error Codes ----------

// Command / Input Errors
#define ERR_INVALID_CMD         "ERR|INVALID_CMD|Invalid or unsupported command."
#define ERR_MISSING_ARGUMENTS   "ERR|MISSING_ARGUMENTS|Required arguments not provided."
#define ERR_INVALID_FILENAME    "ERR|INVALID_FILENAME|Invalid filename format."
#define ERR_INVALID_INODE       "ERR|INVALID_INODE|Inode does not refer to any file."
#define ERR_SOCKET_FAIL     "ERR|SOCKET_FAIL|Unable to create socket."
#define ERR_SS_INVALID_IP      "ERR|INVALID_IP|Storage server IP is invalid."
#define ERR_NM_INVALID_IP      "ERR|INVALID_IP|Name server IP is invalid."

// File Existence / Metadata
#define ERR_NOT_FOUND           "ERR|NOT_FOUND|File does not exist."
#define ERR_STALE_POINTER       "ERR|STALE_POINTER|File metadata corrupted or stale."
#define ERR_METADATA_CORRUPT    "ERR|METADATA_CORRUPT|File metadata corrupted."
#define ERR_EMPTY_FILE          "ERR|EMPTY_FILE|File is empty."

// Permission / Access
#define ERR_NO_PERMISSION       "ERR|NO_PERMISSION|Operation not allowed for this user."
#define ERR_ACCESS_DENIED       "ERR|ACCESS_DENIED|User not authorized to perform this operation."
#define ERR_FILE_LOCKED         "ERR|FILE_LOCKED|File currently locked for writing."
#define ERR_FILE_IN_USE         "ERR|FILE_IN_USE|File is busy, try again later."


#define ERR_SS_UNAVAILABLE      "ERR|SS_UNAVAILABLE|Storage server offline or unreachable."
#define ERR_SS_DOWN             "ERR|SS_DOWN|Storage server disconnected during operation."
#define ERR_NM_UNAVAILABLE      "ERR|NM_UNAVAILABLE|Name server offline or unreachable."
#define ERR_NM_DOWN             "ERR|NM_DOWN|Name server disconnected during operation."
#define ERR_SS_CONNECTION_FAIL     "ERR|CONNECTION_FAIL|Failed to connect to storage server."
#define ERR_NM_CONNECTION_FAIL     "ERR|CONNECTION_FAIL|Failed to connect to name server."
#define ERR_READ_FAIL           "ERR|READ_FAIL|Unable to read file from storage server."
#define ERR_WRITE_FAIL          "ERR|WRITE_FAIL|Failed to write data to storage server."
#define ERR_CREATE_FAIL         "ERR|CREATE_FAIL|Failed to create file on storage server."
#define ERR_DELETE_FAIL         "ERR|DELETE_FAIL|Unable to delete the file."
#define ERR_STORAGE_IO          "ERR|STORAGE_IO|I/O error occurred on storage server."
#define ERR_UNDO_FAIL          "ERR|UNDO_FAIL|Failed to revert to previous file version."
#define ERR_EXEC_FAIL          "ERR|EXEC_FAIL|Failed to execute file on storage server."
#define ERR_SS_DISCONNECTED     "ERR|SS_DISCONNECTED|Storage server disconnected unexpectedly."
#define ERR_NM_DISCONNECTED     "ERR|NM_DISCONNECTED|Name server disconnected unexpectedly."
#define ERR_SS_RECV_FAIL          "ERR|RECV_FAIL|Error receiving data from Storage Server."
#define ERR_NM_RECV_FAIL          "ERR|RECV_FAIL|Error receiving data from Name Server."
#define ERR_NO_CLIENT         "ERR|NO_CLIENT|Specified client does not exist."
#define ERR_FILE_EXISTS         "ERR|FILE_EXISTS|File with the same name already exists."
#define ERR_FOLDER_EXISTS       "ERR|FOLDER_EXISTS|Folder with the same name already exists."
#define ERR_FT_FULL            "ERR|FILE_TABLE_FULL|Maximum file limit reached on name server."
#define ERR_SS_FULL            "ERR|STORAGE_FULL|Storage server has insufficient space."

// Success Messages
#define OK_SUCCESS              "OK|SUCCESS"
#define OK_EXEC_DONE            "OK|EXEC_DONE"
#define OK_FILE_CREATED         "OK|FILE_CREATED"
#define OK_WRITE_COMPLETE       "OK|WRITE_COMPLETE"
#define OK_DELETE_SUCCESS       "OK|DELETE_SUCCESS"
#define OK_UNDO_SUCCESS         "OK|UNDO_SUCCESS"   
#define OK_STREAM_STARTED       "OK|STREAM_STARTED"
#define OK_READ_COMPLETE        "OK|READ_COMPLETE"
#define OK_ACCESS_GRANTED      "OK|ACCESS_GRANTED"
#define OK_ACCESS_REVOKED      "OK|ACCESS_REVOKED"
#define OK_PERMISSION_UPDATED  "OK|PERMISSION_UPDATED"
#define OK_VERSION_RESTORED    "OK|VERSION_RESTORED"
#define SS_DISCONNECTED       "SS_DISCONNECTED SUCCESSFULLY"


// Additional suggested error codes (added for more granularity)
// Networking / socket lifecycle
#define ERR_BIND_FAIL          "ERR|BIND_FAIL|Failed to bind socket to address/port."
#define ERR_LISTEN_FAIL        "ERR|LISTEN_FAIL|Failed to listen on server socket."
#define ERR_ACCEPT_FAIL        "ERR|ACCEPT_FAIL|Failed to accept incoming connection."
#define ERR_SETSOCKOPT_FAIL    "ERR|SETSOCKOPT_FAIL|setsockopt failed."
#define ERR_GETPEERNAME_FAIL   "ERR|GETPEERNAME_FAIL|Could not determine peer address."


// Persistence / state management
#define ERR_STATE_SAVE_FAIL    "ERR|STATE_SAVE_FAIL|Failed to persist server state to disk."
#define ERR_STATE_LOAD_FAIL    "ERR|STATE_LOAD_FAIL|Failed to load server state from disk."
#define ERR_PERSIST_IO         "ERR|PERSIST_IO|I/O error while persisting state."


// Storage / inode related
#define ERR_INODE_INVALID      "ERR|INVALID_INODE|Inode number invalid or unassigned."
#define ERR_INODE_UNASSIGNED   "ERR|INODE_UNASSIGNED|Inode not assigned to any storage server."
#define ERR_STORAGE_TIMEOUT    "ERR|STORAGE_TIMEOUT|Timeout while contacting storage server."

// File / folder / path issues
#define ERR_INVALID_FOLDER     "ERR|INVALID_FOLDER|Invalid folder path or characters."
#define ERR_MISSING_PARENT     "ERR|MISSING_PARENT|Parent folder does not exist."
#define ERR_AMBIGUOUS_NAME     "ERR|AMBIGUOUS_NAME|Filename is ambiguous across folders."

// Cache / hash table
#define ERR_CACHE_MISS         "ERR|CACHE_MISS|Requested entry not present in cache."
#define ERR_CACHE_STALE        "ERR|CACHE_STALE|Cache entry is stale or dangling."

// Client lifecycle / identity
#define ERR_DUPLICATE_CLIENT   "ERR|DUPLICATE_CLIENT|A client with same name already connected."
#define ERR_CLIENT_NOT_CONN    "ERR|CLIENT_NOT_CONNECTED|Target client is not connected."
#define ERR_INVALID_CLIENT_ID  "ERR|INVALID_CLIENT_ID|Client id is invalid."

// Permissions / access
#define ERR_OWNER_MISMATCH     "ERR|OWNER_MISMATCH|Operation allowed only by file owner."
#define ERR_PERMISSION_CONFLICT "ERR|PERMISSION_CONFLICT|Requested permission conflicts with existing rules."

// Timeouts / locking / concurrency
#define ERR_LOCK_TIMEOUT       "ERR|LOCK_TIMEOUT|Timeout waiting for lock."
#define ERR_CONCURRENT_MOD     "ERR|CONCURRENT_MOD|Concurrent modification detected."

// Execution / command errors
#define ERR_EXEC_NO_OUTPUT     "ERR|EXEC_NO_OUTPUT|Executed command produced no output."

// Generic / internal
#define ERR_INTERNAL           "ERR|INTERNAL|Internal server error."
#define ERR_CONFIG_INVALID     "ERR|CONFIG_INVALID|Invalid server configuration."

// Versioning / undo
#define ERR_VERSION_NOT_FOUND  "ERR|VERSION_NOT_FOUND|Requested version for undo not found."


// Note: Add new codes as needed; keep messages short and parseable by clients.
#endif