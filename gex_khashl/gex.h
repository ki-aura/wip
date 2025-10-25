#ifndef GEX_MAIN_H
#define GEX_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <panel.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <signal.h>
#include <assert.h>
#include "khashl.h"

#define GEX_VERSION "9.4.0" // out of sequence version. 9 = khashL version 

// keys we need that aren't already defined by ncurses
#define KEY_ESCAPE 27
#define KEY_MAC_ENTER 10	// KEY_ENTER already defined as send key for terminal
#define KEY_TAB 9
#define KEY_SPACE 32
#define KEY_MAC_DELETE 127
#define KEY_OTHER_DELETE 8		// e.g. on debian 
#define KEY_LEFT_PROXY 222
#define KEY_NCURSES_BACKSPACE KEY_BACKSPACE // this is 263

// types of popup question
typedef enum { 	
	PTYPE_YN,
	PTYPE_CONTINUE,
	PTYPE_UNSIGNED_LONG,
} popup_types;

typedef enum {
	WIN_HEX,
	WIN_ASCII,
	WIN_OTHER,
} clickwin;


// Initialize khash: signed long integer key  → slots holding unsigned char
typedef struct khash_charmap khash_charmap; 
KHASHL_MAP_INIT(static kh_inline klib_unused, khash_charmap, kh, khint64_t, 
				unsigned char, kh_hash_uint64, kh_eq_generic)
/*
 * -----------------------------------------------------------------------
 * KHASHL.H DOCUMENTATION (Single Map, using 'kh' function prefix)
 * -----------------------------------------------------------------------
 *
 * The map name (NAME) is used only in type definition, not in function calls.
 * All functions use the simple 'kh_' prefix.
 *
 * -----------------------------------------------------------------------
 * TYPE DEFINITION AND INITIALIZATION
 * -----------------------------------------------------------------------
 *
 * Type Definition:
 * To define a map type (e.g., 'charmap' with KeyType khint64_t and ValueType unsigned char):
 * 1. typedef struct khash_NAME khash_NAME;
 * 2. KHASHL_MAP_INIT(static kh_inline klib_unused, khash_NAME, kh, KType, VType, HashFn, EqualFn)
 * Parameters Explained (Example: KHASHL_MAP_INIT(..., khash_charmap, kh, khint64_t, unsigned char, kh_hash_uint64, kh_eq_generic)):
 * --------------------------------------------------------------------------------------------------------------------------------
 * SCOPE:      The scope of the generated functions. Typically 'static kh_inline klib_unused' to keep symbols local and permit inlining.
 * HType:      The **struct name** declared in step 1 (e.g., khash_charmap). This is the type a pointer must be declared as.
 * prefix:     The **function prefix** for all generated API calls (e.g., 'kh'). This must be unique across all KHASHL maps in the project.
 * KType:      The C type for the **key** (e.g., khint64_t).
 * VType:      The C type for the **value** (e.g., unsigned char).
 * HashFn:     The hash function macro/call, which must accept KType and return khint_t (provided functions are: kh_hash_uint64, kh_hash_uint32, kh_hash_str, kh_hash_bytes, kh_hash_dummy).
 * EqualFn:    The key equality comparison function, which must accept two KType variables and return true (non-zero) if they are equal (provided functions are: kh_eq_generic, kh_eq_str).
 *
 * khash_NAME *h
 * Declares a pointer h to a hash table of the type given NAME.
 *
 * h = kh_init()
 * Allocates and initializes a new, empty hash table. Returns a pointer to the table.
 *
 * kh_destroy(h)
 * Frees all memory allocated by the hash table h.
 *
 * kh_clear(h)
 * Removes all elements from the table but keeps the table structure allocated,
 * resetting all slots to unused (empty).
 *
 * -----------------------------------------------------------------------
 * ITERATOR AND ACCESS
 * -----------------------------------------------------------------------
 *
 * khint_t k
 * An **integer index (slot)** to an element within a table. This is the generic
 * iterator type. It is returned by modification functions and used for access/iteration.
 * **SLOTS ARE PRE-ALLOCATED INTERNALLY AND ARE ALL INITIALLY EMPTY.**
 *
 * kh_key(h, k)
 * Accesses the **key** at slot k. Read-only usage for key type.
 *
 * kh_val(h, k)
 * Accesses the **value** at slot k. Used to set or retrieve the value.
 *
 * kh_exist(h, k)
 * Returns a non-zero value (true) if slot k contains a valid, active key/value pair.
 *
 * kh_size(h)
 * Returns the count of valid elements currently stored in the table.
 *
 * kh_end(h)
 * Returns the index just past the last slot. Used as the loop terminator value.
 *
 * -----------------------------------------------------------------------
 * CORE OPERATIONS
 * -----------------------------------------------------------------------
 *
 * k = kh_put(h, key, &ret)
 * Attempts to insert 'key'. Returns the slot index k.
 * 'ret' indicates the result:
 * 1: New key inserted (slot was previously unused).
 * 0: Key already existed (nothing was overwritten).
 * -1: Operation failed (e.g., memory allocation error).
 * **NOTE: This only secures the slot. You must use kh_val(h, k) to set the value.**
 *
 * k = kh_get(h, key)
 * Read-only look up for 'key'. Returns the slot index k if found, or kh_end(h) if absent.
 * **Does not modify the table or create a slot if the key is not found.**
 *
 * kh_del(h, k)
 * Deletes the key/value element at slot k.
 * **NOTE: khashl.h uses backward-shifting (no "deleted" tombstones) for deletion.**
 *
 * -----------------------------------------------------------------------
 * ITERATION
 * -----------------------------------------------------------------------
 *
 * kh_foreach(h, k)
 * **The concise iteration macro.** Iterates through all **existing elements** in
 * hash table 'h', assigning the current slot index to the loop variable 'k'.
 * The macro internally handles the check for active elements.
 */

// Overall (non-window) screen attributes & app status
typedef struct {
	// stdscr details
	int cols;
	int rows;
	bool too_small; 	// less than 2 hex rows and 16 hex chars wide
	// general 
	bool in_hex;		// track which pane we're in during edit
	// file & mem handling
	size_t fsize;		// file size
	char *fname;		// file name
	unsigned char *map;  	// mmap base
	int fd;			// file descriptor
	struct stat fs;		// file stat
	int lastkey; 		// debug : last actual OR simulated key
	int lasteditkey;	// debug : last key used in a byte edit
	// hash table for file updates
	khash_charmap *edmap;
} appdef;


// Window definitions
typedef struct {
	WINDOW *border;
	WINDOW *win;
	int height;	// grid height excluding border
	int width;	// grid width excluding border
	int grid;	// grid size in total hex / ascii digits (portion of file)
	// file handling and viewing
	unsigned long v_start;	// file offset location of start of grid
	unsigned long v_end;	// file location of end of grid

	int map_copy_len;
	int max_row;	// this is the max row we can edit if screen > file size
	int max_col; 	// this is the max col on the max row we can edit if screen > file size
	int max_digit; // this is the max digit on the max row we can edit if screen > file size
	int cur_row;	// cursor location (i.e. where to show it rather than where it is)
	int cur_col;
	int cur_digit;	// which hex digit (hinib lownib space) the cursor is on
	bool is_hinib;	// are we on the hi (left) nibble
} hex_windef;

typedef struct {
	WINDOW *border;
	WINDOW *win;
	int height;
	int width;
} ascii_windef;

typedef struct {
	int height;
	int width;
	WINDOW *border;
	WINDOW *win;
} status_windef;


void handle_global_keys(int k);
bool initial_setup(int argc, char *argv[]);
void final_close(int signum);
clickwin get_window_click(int *row, int *col);
bool create_main_menu(void);


extern appdef app;
extern status_windef status;
extern hex_windef hex;
extern ascii_windef ascii;
extern char *tmp;// makes debug panel usage easier
extern khint_t slot; // general hash usage
extern int khret;
extern MEVENT event;

// snprintf(tmp, 200, "msg %lu %d", app.fsize , hex.grid); DP(tmp); 


// this needs to be last as it relies on the typedefs above
#include "gex_helper_funcs.h"
#include "keyb_man.h"
#include "win_man.h"
#include "file_handling.h"

#endif
