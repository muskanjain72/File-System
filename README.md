## LangOS Document Collaboration — File-System (MVP)

## 📖 Introduction

The system is built as a **distributed file system with collaboration capabilities**, consisting of **User Clients**, a centralized **Name Server**, and multiple **Storage Servers**. It allows multiple users to concurrently create, read, edit, and manage text-based documents while enforcing access control, consistency, and durability.

This project was developed under strict specifications with a tight delivery timeline, simulating a real-world engineering environment where **correctness, performance, and system reliability** are critical. The final system demonstrates key concepts from **Operating Systems, Distributed Systems, Networking, and Concurrency**.

## 🧩 System Architecture

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

## 📁 File Model

- Files are **text-only**
- Organized as words → sentences → file
- Sentence delimiters: `.`, `!`, `?`
- Unlimited file size and growth
- Sentence-level locking ensures safe concurrent edits

---
## 👤 User Functionalities

Includes:
- Viewing, reading, creating, writing, deleting files
- Streaming content word-by-word
- Undoing file changes
- Executing file contents as shell commands
- Access control management
- Listing users

---
## 🛠️ Installation & Execution

### Prerequisites
- Linux / Unix-based OS
- GCC, Make
- Networking-enabled environment

### Build & Run
```bash
git clone git@github.com:muskanjain72/File-System.git
cd File-System
make
./run_system.sh
```

---

Run (example):

1) Start the Name Server:

```sh
cd nameserver
./nm
```
it will prompt for server IP and port; enter e.g. 127.0.0.1 and 9000

2) Start a Storage Server (it registers with NM):

```sh
cd storageserver
./ss
```
it will prompt for nameserver ip,port and storage server ip , port 

3) Start a Client and enter a username when prompted:


```sh
cd clientt
./client
```
provide client name, server IP and port; then use commands described above


## Connection & Handshake (How components identify each other)

Both Clients and StorageServers connect using TCP to the Nameserver. The handshake is minimal and deterministic:

1. New TCP connection established.
2. Connector sends a 32-bit integer (host byte order used by the code) indicating type:
	 - 0 => Client
	 - 1 => StorageServer
3. If type == 0 (Client):
	 - Client sends a `Client` struct (see `structs.h`) containing client_name, ip_address and port.
	 - Nameserver registers the client, assigns a client_id (int) and replies by sending the assigned client id (int) back. The client then uses this id in subsequent Packet messages.

4. If type == 1 (StorageServer):
	 - StorageServer sends a `StorageServer` struct (contains ss_id, ip_address, port, used_bytes and an inode list).
	 - Nameserver registers the storage server, assigns an `ss_id`, and replies with the assigned id (htonl'd int is used when writing back in the code).

This simple handshake ensures the Nameserver knows whether it is talking to a client or a storage node and collects identifying metadata.

## Message and packet formats

Two structured message types are commonly used in the codebase:

- Packet: used for Nameserver<->Client requests and replies. Defined in `structs.h` as:

	typedef struct {
			int client_id;
			char msg[1024];
	} Packet;

	The `msg` field carries textual commands or structured replies using '|' as a delimiter. Examples:
	- Client -> Nameserver: "DELETE filename"
	- Nameserver -> Client: "OK|DELETE_SUCCESS|filename" or "ERR|not_found|filename"

- p1: a small control message used between Nameserver and StorageServer with the same `msg[1024]` buffer.

When the Nameserver needs file content operations it opens a control channel to the StorageServer by sending simple text commands (for example: "DELETE <path>", "EXEC <inode>", "READ <inode>", "WRITE <inode> <sentence_no>"). StorageServer replies are text sent back on the same socket.

## Error Handling  
All error responses from the Nameserver use the ERR|<CODE>|<DETAIL> convention defined in `error_codes.h`.

## Commands(alphabetical order)

### ADDACCESS :

``` sh
ADDACCESS <permission> <filename> <clientname>
```

- Note:Permission must be either `R` or `W` (no `RW`).
- Purpose: Grant a user explicit read or write permission on a file.
- Flow: 
1. Client → Name Server:  
   `Packet.msg = "ADDACCESS <perm> <filename> <clientname>"`
2. Name Server validates parameters and existence of the file and target client.  
   - If the file does not exist:  
     `ERR|not_found|<filename>`
   - If the client does not exist:  
     `ERR|no_such_client|<clientname>`
3. If validation passes and the requester is the owner, the Name Server updates file metadata and responds with `ADDACCESS_OK`.
---

### APPROVE
``` 
APPROVE <filename> <username> <permission>
```
**Purpose:**  
Approve a pending access request made by another user.
**Flow:**
1. Client → Name Server: `APPROVE <filename> <username> <permission>`
2. Name Server verifies that the requester is the file owner.
3. Permission is applied and an `OK` or `ERR` response is returned.
---

### CHECKPOINT
``` 
CHECKPOINT <filename> <tag>
```
**Purpose:**  
Create a persistent snapshot of a file’s contents on the Storage Server.
**Flow:**
1. Name Server verifies file existence and user permissions.
2. Name Server instructs the Storage Server to create a checkpoint using `<inode> + <tag>`.
3. Checkpoint metadata is recorded and an `OK` or `ERR` response is sent to the client.
---

### CREATE
```
CREATE <filename>
```
**Purpose:**  
Create new file metadata and allocate storage.
**Flow:**
1. Name Server assigns a new inode.
2. A Storage Server is selected.
3. Metadata is written and `OK` is returned, or an error such as `FILE_EXISTS`.
---

### CREATEFOLDER
```
CREATEFOLDER <path>
```

**Purpose:**  
Create a folder in the namespace (metadata only).

**Flow:**
1. Name Server validates that the parent folder exists.
2. Folder metadata is created.
3. Responds with `OK` or an appropriate error.

---

### DELETE
```
DELETE <filename>
```

**Purpose:**  
Delete a file and its associated data.

**Flow:**
1. Client → Name Server: `DELETE <filename>`
2. Name Server validates file existence and permissions.
3. Metadata is removed and the Storage Server is instructed to delete the file.
4. Client receives `OK|DELETE_SUCCESS|<filename>` or an error.

---

### EXEC
```
EXEC <filename>
```

**Purpose:**  
Execute file contents as shell commands and stream output to the client.

**Flow:**
1. Client → Name Server: `EXEC <filename>`
2. Name Server verifies permissions and Storage Server availability.
3. Name Server sends `EXEC <inode>` to the Storage Server.
4. Storage Server returns execution data.
5. Name Server executes commands using `popen()` and streams output to the client using:
   - `EXEC|BEGIN`
   - Output lines
   - `EXEC|END`

### INFO
```
INFO <filename>
```

**Purpose:**  
Show metadata about a file, including size, owner, inode, storage server, word/character counts, and timestamps.

**Flow:**
1. Client → Name Server: `INFO <filename>`
2. Name Server reads file metadata.
3. Name Server replies with a Packet containing formatted metadata information.

---

### LIST
```
LIST
```

**Purpose:**  
List files accessible to the client (short form).

**Flow:**
1. Client → Name Server: `LIST`
2. Name Server assembles the accessible file list from metadata.
3. Returns the list in a Packet response.

---

### LISTCHECKPOINT
```
LISTCHECKPOINT <filename> [tag]
```

**Purpose:**  
List checkpoint tags and timestamps associated with a file.

**Flow:**
1. Client → Name Server: `LISTCHECKPOINT <filename> [tag]`
2. Name Server reads checkpoint metadata for the file.
3. Returns a structured response containing checkpoint details.

---

### MOVE
```
MOVE <filename> <folder>
```

**Purpose:**  
Move a file’s metadata under a different folder path.

**Flow:**
1. Client → Name Server: `MOVE <filename> <folder>`
2. Name Server validates target folder existence.
3. Metadata is updated and an `OK` or `ERR` response is returned.

---

### READ
```
READ <filename>
```

**Purpose:**  
Read the complete contents of a file.

**Flow:**
1. Client → Name Server: `READ <filename>`
2. Name Server locates file metadata and the Storage Server holding the inode.
3. Name Server replies with:
   ```
   READ|<filename>|<ss_ip>|<ss_port>|<inode>
   ```
4. Client connects directly to the Storage Server and retrieves file content.

---

### REVERT
```
REVERT <filename> <tag>
```

**Purpose:**  
Restore a file’s contents to a previously created checkpoint.

**Flow:**
1. Client → Name Server: `REVERT <filename> <tag>`
2. Name Server instructs the Storage Server to restore checkpoint data.
3. Metadata is updated and an `OK` or `ERR` response is returned.

---

### REQUESTACCESS
```
REQUESTACCESS -R|-W|-RW <filename>
```

**Purpose:**  
Request read and/or write permission for a file from its owner.

**Flow:**
1. Client → Name Server: `REQUESTACCESS <perm> <filename>`
2. Name Server records the request in `pending_requests` for the file.
3. Confirmation or error response is returned.

---

### REMACCESS
```
REMACCESS <filename> <clientname>
```

**Purpose:**  
Remove a client’s access permissions for a file.

**Flow:**
1. Client → Name Server: `REMACCESS <filename> <clientname>`
2. Name Server verifies ownership.
3. Permission entry is removed and an `OK` or `ERR` response is returned.

---

### STREAM
```
STREAM <filename>
```

**Purpose:**  
Stream file contents to the client.

**Flow:**
1. Client → Name Server: `STREAM <filename>`
2. Name Server provides Storage Server location and inode.
3. Client connects to the Storage Server and receives streamed content.

---

### UNDO
```
UNDO <filename>
```

**Purpose:**  
Rollback the most recent change made to a file.

**Flow:**
1. Client → Name Server: `UNDO <filename>`
2. Name Server coordinates with the Storage Server to restore previous content state.
3. Client receives confirmation or error response.

## StorageServer protocol (Nameserver -> StorageServer)

The Nameserver talks to StorageServers via a simple text command protocol over the TCP connection that SS establishes to NM during registration. Common commands issued by NM include:

- "READ <inode>" — StorageServer responds with content or an address to the content file.
- "WRITE <inode> <sentence_no> <is_terminated>" — SS accepts a new sentence or content block.
- "DELETE <ss_id>/<inode>.txt" — SS deletes on-disk file and replies with OK or ERR.
- "EXEC <inode>" — SS returns a shell command or the immediate output to be executed by NM and streamed to the client.
- "STREAM <inode>" — instructs SS to stream content to the requester.

SS implements the actual filesystem operations (persisting files, making checkpoint copies, executing a script), and logs actions to `loggings/storageserver.log`.


## Troubleshooting
---------------

- "NM not reachable": ensure `nm` is running and client/SS use correct IP:port.
- "SS_UNAVAILABLE": verify SS is registered with NM and listening on the expected ports.
- Permission errors: use `INFO <filename>` to inspect ACLs and `ADDACCESS` to grant permissions.


## Design notes & limitations
-------------------------

- NM is a single point of failure by design for this MVP.
- Identity is username-only (no cryptographic auth).
- Replication and advanced fault tolerance are optional/bonus features.

## Logging

Both NM and SS write operation logs to `loggings/nameserver.log` and `loggings/storageserver.log` respectively. These logs include high-level events (CREATE_OK, DELETE_SENT_TO_SS, DELETE_OK, EXEC_RUN, etc.) that are useful for auditing and debugging request flows.

## Next steps that can be taken
---------------------

- Add secure authentication and encrypted communication.
- Implement replication and heartbeat-based failure detection.
- Add hierarchical folders and named checkpoints.


