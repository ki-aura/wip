#ifndef GEX_FILE_HANDLING_H
#define GEX_FILE_HANDLING_H

bool open_file(int argc, char *argv[]);
void close_file(void);
void save_changes(void);
void abandon_changes(void);
void insert_bytes(void);
void delete_bytes(void);
	

#endif
