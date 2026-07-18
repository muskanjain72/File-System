# LangOS Document Collaboration - File-System MVP

## Introduction

The system is built as a **distributed text-oriented file system with collaboration capabilities**, consisting of **User Clients**, a centralized **Name Server**, and multiple **Storage Servers**. It allows multiple users to concurrently create, read, edit, and manage text-based documents while enforcing access control, consistency, and durability.

## System Architecture

The system is composed of three core components:
### 🔹 User Clients
- Interface through which users interact with the system
- Supports file operations such as create, read, write, delete, stream, and execute
- Multiple clients can operate concurrently
- Each client is associated with a username for access control

### 🔹 Name Server (NM)
- Acts as the **central coordinator**
- Maintains metadata including file locations, access control lists, and registrations
- Handles efficient file lookup and caching
- Coordinates communication between clients and storage servers

### 🔹 Storage Servers (SS)
- Responsible for physical storage of files
- Ensure persistence and durability
- Support concurrent read/write access
- Maintain undo history and sentence-level locking


##  File Model

- Files are **text-only**
- Organized as words → sentences → file
- Sentence delimiters: `.`, `!`, `?`
- Unlimited file size and growth
- Sentence-level locking ensures safe concurrent edits
- Storage files are created as `<ss_id>/<inode>.txt` on the Storage Server.
- Backup files use the storage server backup helpers for undo support.
- Checkpoints are stored as `<ss_id>/<inode>_<tag>.txt.chk`.

## Prerequisites

- POSIX-compatible environment such as Linux, macOS, WSL, or another Unix-like
  environment
- GCC
- Make
- Local TCP networking enabled

This project uses POSIX APIs (`unistd.h`, `arpa/inet.h`, pthreads, Unix-style
paths, and `popen()`), so native Windows builds need a POSIX layer such as WSL or
Cygwin, or code changes for WinSock/Windows threading.

## Build 

```sh
git clone git@github.com:muskanjain72/File-System.git
cd File-System
make
```

To remove generated binaries and object files:

```sh
make clean
```

## Run

Open three terminals from the repository root.

### 1. Start the Name Server

```sh
./nameserver/nm
```

Prompts:

```text
Enter server IP to bind:
Enter server port to bind:
```

Example values for local testing: 127.0.0.1, 9000

### 2. Start a Storage Server

```sh
./storageserver/ss
```

Prompts:

```text
Enter main server IP:
Enter main server port:
Enter storage server IP:
Enter storage server port:
```

Use the Name Server IP/port from step 1 for the main server. For local testing,
the storage server IP can also be `127.0.0.1`; choose a different port, for
example `9001`.

### 3. Start a Client

```sh
./clientt/client
```

Prompts:

```text
Enter your name:
Enter server IP:
Enter server port:
```

Provide client name, server IP and port

## Component Handshake 

Clients and Storage Servers connect to the Name Server over TCP.  The handshake is minimal and deterministic:

1. The connector process sends a 32-bit integer type:
   - `0` for Client
   - `1` for Storage Server
2. If type == 0 (Client):
	 - Client sends a `Client` struct containing client_name, ip_address and port.
	 - Nameserver registers the client, assigns a `client_id` (int) and replies by sending the assigned client id (int) back. The client then uses this id in subsequent Packet messages.
3. If type == 1 (StorageServer):
	 - StorageServer sends a `StorageServer` struct (contains ss_id, ip_address, port, used_bytes and an inode list).
	 - Nameserver registers the storage server, assigns an `ss_id`, and replies with the assigned id.

Core structs are defined in `structs.h`.

## Error Handling  
All error responses from the Nameserver use the ERR|<CODE>|<DETAIL> convention defined in `error_codes.h`.

## Client Commands (alphabetical order)

Commands are entered in the interactive client shell.

| Command | Usage |
| --- | --- |
| `ADDACCESS` | `ADDACCESS <permission> <filename> <clientname>` |
| `APPROVE` | `APPROVE <filename> <username> [permission]` |
| `CHECKPOINT` | `CHECKPOINT <filename> <tag>` |
| `CREATE` | `CREATE <filename>` |
| `CREATEFOLDER` | `CREATEFOLDER <path>` |
| `DELETE` | `DELETE <filename>` |
| `DENY` | `DENY <filename> <username> [permission]` |
| `EXEC` | `EXEC <filename>` |
| `INFO` | `INFO <filename>` |
| `LIST` | `LIST` |
| `LISTCHECKPOINT` | `LISTCHECKPOINT <filename> [tag]` |
| `MOVE` | `MOVE <filename> <folder>` |
| `READ` | `READ <filename>` |
| `REMACCESS` | `REMACCESS <filename> <clientname>` |
| `REQUESTACCESS` | `REQUESTACCESS -R|-W|-RW <filename>` |
| `REVERT` | `REVERT <filename> <tag>` |
| `STREAM` | `STREAM <filename>` |
| `UNDO` | `UNDO <filename>` |
| `VIEW` | `VIEW`, `VIEW -l`, `VIEW -a`, or `VIEW -al` |
| `VIEWCHECKPOINT` | `VIEWCHECKPOINT <filename> <tag>` |
| `VIEWFOLDER` | `VIEWFOLDER <folder>` |
| `VIEWREQUESTS` | `VIEWREQUESTS [filename]` |


### CREATE

Creates file metadata in the Name Server, assigns an inode, selects a Storage
Server, and asks the Storage Server to create the backing inode file.

### READ

Purpose is to read the complete content of a file. The Name Server first checks metadata and permissions, then replies with:

```text
READ|<filename>|<ss_ip>|<ss_port>|<inode>
```

The client then connects directly to the Storage Server and sends `READ <inode>`. 

### WRITE

Usage:

```text
WRITE <filename> <sentence_no>
```

The Name Server validates write permission and sentence locking, then replies
with Storage Server location details. The client connects directly to the
Storage Server and sends the new content.

### STREAM

Similar to `READ`, but the client asks the Storage Server to stream file contents.

### EXEC

First name server verifies permissions and then requests file contents from the Storage Server and executes the
content with `popen()`, streaming output back to the client between:

```text
EXEC|BEGIN|<filename>
EXEC|END
```

Only run `EXEC` on files whose contents you trust.

### DELETE 

Delete a file and its associated data. Name Server first validates file existence and permissions.

### UNDO

Restores the last backed-up version of the file on the Storage Server.

### CHECKPOINT, VIEWCHECKPOINT, and REVERT

- `CHECKPOINT <filename> <tag>` creates a persistent snapshot of a file’s contents on the Storage Server.
- `VIEWCHECKPOINT <filename> <tag>` reads checkpoint contents.
- `REVERT <filename> <tag>` restores a file from a checkpoint.
- `LISTCHECKPOINT <filename>` lists checkpoint tags and timestamps associated with a file. 

### FOLDERS

Folders are metadata paths on the Name Server. They do not create corresponding
directories on the Storage Server.

- `CREATEFOLDER <folder>`
- `CREATEFOLDER docs/tutorials` 
- `MOVE <filename> <folder>` moves a file's metadata under a different folder path
- `VIEWFOLDER <folder>`

Folder paths cannot start or end with `/`; nested parent folders must already exist.

### ACCESS CONTROL

Direct grant/revoke:

```text
ADDACCESS R <filename> <clientname>
ADDACCESS W <filename> <clientname>
REMACCESS <filename> <clientname>
```

Request flow:

```text
REQUESTACCESS -R|-W|-RW <filename>
VIEWREQUESTS [filename]
APPROVE <filename> <username> [permission]
DENY <filename> <username> [permission]
```

Only the file owner can approve, deny, grant, or remove another user's access. Name server verifies the ownership first.All the requests are recorded in `pending requests` for a specefic file.


### LIST
lists files accessible to the client 

### INFO 
Show metadata about a file, including size, owner, inode, storage server, word/character counts, and timestamps.

## Name Server to Storage Server Protocol

The Nameserver talks to StorageServers via a simple text command protocol over the TCP connection that SS establishes to NM during registration. The Name Server sends commands such as:

- `CREATE <ss_id>/<inode>.txt`
- `READ <inode>`
- `WRITE <inode> <sentence_no>`
- `STREAM <inode>`
- `EXEC <inode>`
- `UNDO <inode>`
- `DELETE <ss_id>/<inode>.txt`
- `CHECKPOINT <inode> <tag>`

Some client-facing operations, such as read/write/stream/checkpoint viewing and
revert, involve the client connecting directly to the Storage Server after the
Name Server returns the Storage Server IP, port, inode, or checkpoint path.

## Logging

The code writes operational logs to:

- `loggings/nameserver.log`
- `loggings/storageserver.log`

Additional local runtime files such as `nm.log`, `ss.log`, `backup.txt`, storage
directories named by Storage Server ID, and compiled binaries/object files may be created while running the system.  These logs include high-level events (CREATE_OK, DELETE_SENT_TO_SS, DELETE_OK, EXEC_RUN, etc.) that are useful for auditing and debugging request flows.

## Troubleshooting

- If the client or Storage Server cannot connect, confirm that `nameserver/nm` is running and that the IP/port values match.
- If a direct file operation fails, confirm that `storageserver/ss` is running and that the Storage Server port entered during startup is reachable.
- If a permission command fails, use `INFO <filename>`, `VIEWREQUESTS`, and `ADDACCESS` or `APPROVE` to inspect and update access.
- If folder operations fail, create parent folders before nested folders.

## Current Limitations

- The Name Server is a single point of failure.
- Identity is based on the provided username.
- There is no encrypted transport.
- Replication and heartbeat-based recovery are not fully implemented.
- The project currently builds with warnings under `-Wall`.
