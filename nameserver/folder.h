#ifndef FOLDER_NS_H
#define FOLDER_NS_H
#include "../main.h"

// Create a folder (and intermediate parents must already exist except root). Returns message in Packet.
void create_folder(Client *c, const char *folder_path, Packet *outp);

// Move a file into an existing folder.
void move_file_to_folder(Client *c, const char *filename, const char *folder_path, Packet *outp);

// View all files directly inside a folder (non-recursive).
void view_folder(Client *c, const char *folder_path, Packet *outp);

// View recursively (optional future extension)
// void view_folder_recursive(Client *c, const char *folder_path, Packet *outp);

// Check if a folder exists (thread-safe). Returns 1 if exists, 0 otherwise.
int folder_exists(const char *folder_path);

#endif