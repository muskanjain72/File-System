CC = gcc
CFLAGS = -Wall -pthread

CLIENT_SRC = clientt/client.c clientt/read.c clientt/write.c clientt/stream.c clientt/exec.c clientt/viewcheckpoint.c clientt/revert.c
NM_SRC = nameserver/nm.c nameserver/create.c nameserver/read.c nameserver/view.c nameserver/write.c nameserver/getAddInfo.c nameserver/delete.c nameserver/undo.c nameserver/list.c nameserver/addaccess.c nameserver/stream.c nameserver/exec.c nameserver/removeaccess.c nameserver/folder.c nameserver/path.c nameserver/log.c nameserver/checkpoint.c nameserver/listcheckpoint.c nameserver/viewcheckpoint.c nameserver/revert.c nameserver/access_control.c
SS_SRC = storageserver/ss.c storageserver/read.c storageserver/write.c storageserver/delete.c storageserver/undo.c storageserver/stream.c storageserver/exec.c storageserver/persist.c storageserver/log.c

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
NM_OBJ = $(NM_SRC:.c=.o)
SS_OBJ = $(SS_SRC:.c=.o)

all: client nm ss

# --- Client ---
client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o clientt/client $(CLIENT_OBJ)

# --- NameServer ---
nm: $(NM_OBJ)
	$(CC) $(CFLAGS) -o nameserver/nm $(NM_OBJ)

# --- StorageServer ---
ss: $(SS_OBJ)
	$(CC) $(CFLAGS) -o storageserver/ss $(SS_OBJ)

# --- Compile rule for .o files ---
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Clean ---
clean:
	rm -f $(CLIENT_OBJ) $(NM_OBJ) $(SS_OBJ)
	rm -f clientt/client nameserver/nm storageserver/ss

.PHONY: all clean
