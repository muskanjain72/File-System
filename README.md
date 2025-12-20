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

- ADDACCESS <permission> <filename> <clientname :(PERMISSION ID EITHER R OR W ..NO RW)
	- Purpose: Grant a user explicit read/write permission on a file.
	- Flow:
		1.Client -> Nameserver: Packet.msg = "ADDACCESS <perm> <filename> <clientname>"
		2.Nameserver validates parameters and existence of file and target client.
			- If the file or client doesn't exist, NM returns an error Packet: "ERR|not_found|<filename>" or "ERR|no_such_client|<clientname>".
		3.If validation passes and the requester is owner, Nameserver updates file metadata and returns OK ("ADDACCESS_OK" backed in logs and reply).

- APPROVE <filename> <username> <perm?>
	- Purpose: Approve a pending access request made by another user.
	- Flow: NM checks that the approver is owner, applies the permission and responds with an OK/ERR Packet.

- CHECKPOINT <filename> <tag>
	- Purpose: create a persistent snapshot of the file contents on the StorageServer.
	- Flow:
		1. Nameserver ensures file exists and the requester has permission.
		2. NM sends a request to the StorageServer to create the checkpoint file (SS stores the checkpoint under a deterministic name that includes inode and tag).
		3. NM records the checkpoint tag in local metadata and replies to client with OK or ERR.

- CREATE <filename>
	- Purpose: create new file metadata (creates inode, allocates slot on a storage server).
	- Flow: NM assigns an inode, decides on a StorageServer, writes metadata and returns OK or an appropriate ERR (FILE_EXISTS, MISSING_PARENT, etc.).

- CREATEFOLDER <path>
	- Purpose: create a folder (nameserver metadata only).
	- Flow: NM validates parent exists, creates FolderMeta, returns OK or error.

- DELETE <filename>
	- Purpose: remove a file from metadata and instruct StorageServer to delete content.
	- Flow:
		1. Client -> NM: "DELETE <filename>"
		2. NM validates file existence & permissions.
		3. NM removes local metadata entry, sends a "DELETE <ss_id>/<inode>.txt" request to the StorageServer and marks the inode free.
		4. NM replies to client with: "OK|DELETE_SUCCESS|<filename>" on success, or an ERR on failure.

- EXEC <filename>
	- Purpose: ask the StorageServer to execute the script/program stored in file content and stream stdout back to the client via the Nameserver.
	- Flow:
		1. Client -> NM: "EXEC <filename>"
		2. NM verifies permissions and availability of the StorageServer holding the file.
		3. NM sends an "EXEC <inode>" command to the StorageServer.
		4. StorageServer responds with the shell command to run (or direct output). NM builds a safe shell invocation and runs it using popen(), streaming output lines back to the client as Packet messages framed with "EXEC|BEGIN" and "EXEC|END" markers.
		5. NM streams the process output line-by-line to the client as separate Packet messages.

- INFO <filename>
	- Purpose: show metadata about a file (size, owner, inode, storage server, counts, timestamps).
	- Flow: NM reads metadata and replies with a Packet containing printable info.

- LIST
	- Purpose: list files accessible to the client (short form).
	- Flow: NM assembles a list from its metadata and returns in a Packet.

- LISTCHECKPOINT <filename> [tag]
	- Purpose: list checkpoint tags and timestamps for a file.
	- Flow: NM reads checkpoint list from metadata and returns a structured response.

- MOVE <filename> <folder>
	- Purpose: move file metadata under a different folder path.
	- Flow: NM validates target folder and modifies metadata, then replies OK/ERR.

- READ <filename>
	- Purpose: read a file's content.
	- Flow:
		1. Client -> NM: "READ <filename>"
		2. NM finds file metadata and the StorageServer holding the inode.
		3. NM replies with a response message that contains StorageServer IP, port and inode (e.g. "READ|<filename>|<ss_ip>|<ss_port>|<inode>").
		4. The client then connects directly to the StorageServer and requests the file content.

- REVERT <filename> <tag>
	- Purpose: restore a file's contents to a previously checkpointed tag.
	- Flow: NM instructs the StorageServer to copy the checkpoint file back to the live file and updates metadata, replying with OK/ERR.

- REQUESTACCESS -R|-W|-RW <filename>
	- Purpose: request permission to read/write a file from its owner.
	- Flow: NM records the request in `pending_requests` on the FileMeta and replies with a confirmation or an error if file doesn't exist.

- REMACCESS <filename> <clientname>
	- Purpose: remove access for a client from a file.
	- Flow: NM checks owner permissions and removes the permission entry; replies OK/ERR.

- STREAM <filename>
	- Purpose: stream the file contents (NM coordinates or the client connects to SS per implementation).
	- Flow: NM communicates the SS location and inode; the client connects to StorageServer for streaming.

- UNDO <filename>
	- Purpose: rollback last local change (metadata + storage content interaction).
	- Flow: NM coordinates with the StorageServer to restore previous sentence/contents.


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


