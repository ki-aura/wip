#include "gex.h"


void size_windows(void) {
	// Get the current screen dimensions
	getmaxyx(stdscr, app.rows, app.cols);
	app.too_small = false;
	
	// Calculate window heights 
	status.height = 3; 
	hex.height = app.rows - status.height - 4; // 4 for the borders
	ascii.height = hex.height;					
	
	// calculate window widths
	hex.width = ((int)((app.cols-4) / 4) * 3); 
	ascii.width = (int)(hex.width / 3);
	status.width = hex.width + ascii.width + 2; // 2 for the internal borders of hex & ascii

	hex.grid = ascii.width * ascii.height;
	
	// Check for minimum screen size to prevent crashes
	// status.height (and hex) don't include borders, so +10 = 2 for status borders + 4+2 for hex 
	if (app.rows < (status.height + 8) || app.cols < 68) // 68 allows 16 bytes in each window
		// If the screen is too small, set the flag for window population
		app.too_small = true;
}

// Function to create and resize all windows
void create_windows(void) {
   // get sizes
   resizeterm(0, 0); 
   refresh();
   size_windows();
 
   // First, clear the entire screen to get rid of any artifacts
	delete_windows();
	clear();
	refresh();

  // Check for minimum screen size to prevent crashes
	if (app.too_small) {
		// If the screen is too small, just display a message and return
		mvprintw(app.rows / 2, (app.cols - 45) / 2, "Screen is too small. Please resize to continue.");
		refresh(); // refresh becuase i'm writing to main screen
	} else 	{
		// Create new windows & borders based on the new dimensions
		// newwin(num_rows, num_cols, start_row, start_col)
		status.border = newwin(status.height+2, status.width+2, 0, 0);
		hex.border = newwin(hex.height+2, hex.width+2, status.height+2, 0);
		ascii.border = newwin(ascii.height+2, ascii.width+2, status.height+2, hex.width+2);
			
		// Draw borders and content for each window
		box(status.border, 0, 0);
		box(hex.border, 0,0);
		box(ascii.border, 0, 0);
		
		status.win = newwin(status.height, status.width, 1, 1);
		hex.win = newwin(hex.height, hex.width, status.height+3, 1);
		ascii.win = newwin(ascii.height, ascii.width, status.height+3,(hex.width+3));
	}
	
	// simulate a move so that the changes in the grid size are reflected 
//	handle_keys(KEY_REFRESH
	
	// populate the windows
	update_all_windows();
}

// Function to delete all windows
void delete_windows(void) {
if (status.win != NULL) {delwin(status.win); status.win = NULL; }
if (hex.win != NULL) {delwin(hex.win); hex.win = NULL; }
if (ascii.win != NULL) {delwin(ascii.win); ascii.win = NULL; } 
if (status.border != NULL) {delwin(status.border); status.border = NULL; }
if (hex.border != NULL) {delwin(hex.border); hex.border = NULL; }
if (ascii.border != NULL) {delwin(ascii.border); ascii.border = NULL; } 
}

void refresh_status(void) {
	mvwprintw(status.win, 0, 0, "GEX %s [%s] Size:%lu Offset:%lu             ", 
			GEX_VERSION, get_filename(app.fname), app.fsize, cursor_full_file_offset());
	mvwprintw(status.win, 1, 0, "Grid offset %lu-%lu Screen:%dx%d Grid:%dx%d=%d           ", 
			hex.v_start, hex.v_start+hex.grid-1, app.rows, app.cols, ascii.width, hex.height, hex.grid);
	mvwprintw(status.win, 2, 0, 
			"cr%02d cc%02d cd%02d Hwin%d hinib%d lk%d lek%d chgs%03d         ",
			hex.cur_row, hex.cur_col, hex.cur_digit, app.in_hex, hex.is_hinib, 
			app.lastkey, app.lasteditkey, kh_size(app.edmap));
	box(status.border, 0, 0);
	wnoutrefresh(status.border);
	wnoutrefresh(status.win);
}

void refresh_grids(void){
	char hinib, lonib, a_char;
	int offset = 0, r=0, hc=0, ac=0;
	bool chg;
	
	while(file_offset_to_rc(offset, &r, &hc, &ac)) {	//while v_start + offset is < end of file
														// this also gives us grid coordinates
		// break the byte into displayable nibbles for the hex window
		byte_to_nibs(app.map[hex.v_start + offset], &hinib, &lonib);
		// check if there's an edit for this char
		slot = kh_get(app.edmap, (size_t)(hex.v_start + offset));
		if (slot != kh_end(app.edmap)) {
			// get the changed byte
			chg=true;
			byte_to_nibs(kh_val(app.edmap, slot), &hinib, &lonib);
			a_char = byte_to_ascii(kh_val(app.edmap, slot));
		} else {
			chg=false;
			a_char = byte_to_ascii(app.map[hex.v_start + offset]);
		}
		
		// update the grids
		if (chg) wattron(hex.win, COLOR_PAIR(1) | A_BOLD);	
		mvwprintw(hex.win, r, hc, "%c", hinib);
		mvwprintw(hex.win, r, hc+1, "%c", lonib);
		if (chg) wattroff(hex.win,COLOR_PAIR(1) |  A_BOLD);

		if (chg) wattron(ascii.win, COLOR_PAIR(1) | A_BOLD);	
		mvwprintw(ascii.win, r, ac, "%c", a_char);
		if (chg) wattroff(ascii.win,COLOR_PAIR(1) |  A_BOLD);
		offset++;			
	}
	
	box(hex.border, 0, 0);
	wnoutrefresh(hex.border);
	wnoutrefresh(hex.win);

	box(ascii.border, 0, 0);
	wnoutrefresh(ascii.border);
	wnoutrefresh(ascii.win);
}

void update_all_windows(void) {
	if (!app.too_small){	
		// refresh content
		refresh_grids();
		update_cursor();  // this does a doupdate();
	}
}

void update_cursor(void){
	// update status and debugging content
	refresh_status();
	// move the cursor
	if (app.in_hex) {
		wmove(hex.win, hex.cur_row, hex.cur_col);
		wnoutrefresh(hex.win);
	} else {
		wmove(ascii.win, hex.cur_row, hex.cur_digit);
		wnoutrefresh(ascii.win);	
	}
	doupdate(); 
}

