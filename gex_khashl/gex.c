#include "gex.h"

// Global variables
status_windef status = {.win = NULL, .border = NULL};
hex_windef hex = {.win = NULL, .border = NULL};
ascii_windef ascii = {.win = NULL, .border = NULL};
appdef app;
khint_t slot;
int khret; // return value from kh_put calls - says if already exists
MEVENT event;
char *tmp = NULL;



///////////////////////////////////////////////////
// startup and close down
///////////////////////////////////////////////////

bool initial_setup(int argc, char *argv[])
{
	// Initialize ncurses & set defaults 
	initscr();
	mousemask(BUTTON1_PRESSED | BUTTON1_RELEASED |
          BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED |
          BUTTON1_TRIPLE_CLICKED, NULL);
	start_color();
	use_default_colors(); 
	init_pair(1, COLOR_RED, -1);
	cbreak();		  // Line buffering disabled, Pass on everything
	noecho();		  // Don't echo input
	curs_set(2);		
	keypad(stdscr, true); 	 // Enable function keys (like KEY_RESIZE )
	set_escdelay(50);	 // speed up recognition of escape key - don't wait 1 sec for possible escape sequence
	putp(tigetstr("smcup")); // use alternative buffer

	// create edit map for edit changes and undos
	app.edmap = kh_init();

	// general app defaults

	// set up global variable for debug & popup panel
	tmp = malloc(256); strcpy(tmp, " ");
	
	// Hex window offset to start of file
	hex.v_start = 0;
	app.in_hex = true;	// start in hex screen
	hex.cur_row=0;
	hex.cur_col=0;
	hex.cur_digit=0;	// first hex digit (takes 3 spaces)
	hex.is_hinib = true;	// left nibble of that digit
	app.lasteditkey = 0;
	
	// show cursor
	curs_set(2);
	wmove(hex.win, hex.cur_row, hex.cur_col);
	wrefresh(hex.win);
	
	return open_file(argc,argv);
}


void final_close(int signum)
{
	// Clean up ncurses
	delete_windows();
	clear();
	refresh();
	putp(tigetstr("rmcup"));
	endwin();
	
	// free any globals
	free(tmp);
	
	// close out the hash
	kh_clear(app.edmap);
	kh_destroy(app.edmap);
	
	// close file
	close_file(); 
	
	// user message for forced close
	if (signum == SIGINT){
		fputs("Ended by Ctrl+C\n", stderr);
		exit(EXIT_FAILURE);}
	if (signum == SIGQUIT){
		fputs("Ended by Ctrl+\\\n", stderr);
		exit(EXIT_FAILURE);}
	if (signum == SIGTERM){
		fputs("Programme Killed\n", stderr);
		exit(EXIT_FAILURE);}
		
	exit(0);
}

void handle_global_keys(int k) {

	switch(k){
	case KEY_MOUSE: 
		if ((getmouse(&event) == OK)) {
		// Treat any of these as a "logical click"
			const mmask_t CLICKY = BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | 
						BUTTON1_TRIPLE_CLICKED | BUTTON1_PRESSED;
			if (event.bstate & CLICKY) {
				int row, col;
				clickwin win = get_window_click(&row, &col); // relative coords, or 'n'
				handle_click(win, row, col);
				update_cursor();
			}
		}
		break;

	case KEY_RESIZE:
		create_windows();
		handle_in_screen_movement(KEY_HOME); // reset cursor location
		update_all_windows();
		break;
	
	// in-screen key movement
	case KEY_NCURSES_BACKSPACE:
	case KEY_MAC_DELETE:
	case KEY_OTHER_DELETE:
	case KEY_LEFT:
	case KEY_RIGHT:
	case KEY_HOME:
	case KEY_END:
	case KEY_TAB:
		handle_in_screen_movement(k);
		break;
	
	// 
	case KEY_UP:
	case KEY_DOWN:
	case KEY_NPAGE:
	case KEY_PPAGE:
	//case MENU_GOTO:
		handle_scrolling_movement(k);
		break;
	
	//default checks for editing
	default: 
		handle_edit_keys(k);

	} // end switch

}


clickwin get_window_click(int *row, int *col) 
{
    int win_rs, win_cs, win_re, win_ce; // rs row start, rs row end, ...
	
	// get mouse row (y) and col (x)
    int mr = event.y;
    int mc = event.x;

    // Check hex window
    getbegyx(hex.win, win_rs, win_cs); // row and col start
    getmaxyx(hex.win, win_re, win_ce); // row and col end

    if (mr >= win_rs && mr < win_rs + win_re &&
        mc >= win_cs && mc < win_cs + win_ce) { 
        *row = mr - win_rs;			
        *col = (mc - win_cs);
        return WIN_HEX;
    }

    // Check ascii window
    getbegyx(ascii.win, win_rs, win_cs); // row and col start
    getmaxyx(ascii.win, win_re, win_ce); // row and col end

    if (mr >= win_rs && mr < win_rs + win_re &&
        mc >= win_cs && mc < win_cs + win_ce) { 
        *row = mr - win_rs;			
        *col = (mc - win_cs);
        return WIN_ASCII;
    }

    // Outside both windows
    *row = *col = -1;
    return WIN_OTHER;
}

int main(int argc, char *argv[]) 
{
signal(SIGINT, final_close);
signal(SIGQUIT, final_close);
signal(SIGTERM, final_close);

	// Initial app setup
	if(initial_setup(argc, argv)){
		// everything opened fine... crack on!
		create_windows();
	
		int ch = KEY_REFRESH; // doesn't trigger anything
		// Main loop to handle input
		while (ch != KEY_SEND) {
			// if reresh, handle keys before we wait for another char
			// used by multiple functions to force a screen refresh
			if(ch == KEY_REFRESH) handle_global_keys(ch);
			
			if(ch == KEY_ESCAPE) {
				if(create_main_menu()) { // true if quit is selected
					if (kh_size(app.edmap) != 0) {// there are unsaved changes
					    if(popup_question("Abandon unsaved changes?",
            					"This action can not be undone (y/n)", PTYPE_YN)) {
            				break; // we want to abandon changes
            			} else {
            				continue; // we don't want to abandon changes
            			}
            		} else {
            			break;  // there are no changes to abandon
            		}
            	}
            }
			
			ch = getch();
			app.lastkey = ch;
			handle_global_keys(ch);
		} 
	} else {
		// initial setup failed
		putp(tigetstr("rmcup"));
		endwin();
		fputs("File does not exist\n", stderr);
	}
	// tidy up
	final_close(0);		
	return 0;
}

// menu functions
 
 bool create_main_menu(void)
{
    const char *items[] = {
        "QUIT             (q)",
        "SAVE_Changes     (s)",
        "ABANDON_Changes  (a)",
        "GOTO_Byte        (g)",
        "INSERT_Bytes     (i)",
        "DELETE_Bytes     (d)"
    };
    int mi = sizeof(items)/sizeof(items[0]);
    int highlight = 0;
    int choice = -1;
    int c, oldcurs;

    PANEL *menu_panel;
    WINDOW *menu_win;

    int win_height = 10;
    int win_width  = 24;
    int starty = (LINES - win_height) / 2;
    int startx = (COLS - win_width) / 2;

    menu_win = newwin(win_height, win_width, starty, startx);
    keypad(menu_win, TRUE);

    menu_panel = new_panel(menu_win);
    box(menu_win, 0, 0);
    mvwprintw(menu_win, 1, 1, "Use arrows, Enter, ESC");
    oldcurs = curs_set(0); // store prev cursor state and turn it off

    do {
        // Draw menu with highlight
        for (int i = 0; i < mi; i++) {
            if (i == highlight) wattron(menu_win, A_REVERSE);
            else wattroff(menu_win, A_REVERSE);
			mvwprintw(menu_win, 3 + i, 2, "%s", items[i]);
		}
        update_panels();
        doupdate();

		c = wgetch(menu_win);
        switch (c) {
            case KEY_DOWN:
                highlight = (highlight + 1) % mi; break;
            case KEY_UP:
                highlight = (highlight - 1 + mi) % mi; break;
            case KEY_MAC_ENTER:  // Enter
                choice = highlight; break;
            case 'q': case 'Q':
                choice = 0; break;
            case 's': case 'S':
                choice = 1; break;
            case 'a': case 'A':
                choice = 2; break;
            case 'g': case 'G':
                choice = 3; break;
            case 'i': case 'I':
                choice = 4; break;
            case 'd': case 'D':
                choice = 5; break;
        }
        if (choice != -1) break; // break from do loop if valid choice
                    
    } while (c != KEY_ESCAPE && c != KEY_RESIZE);

	//put curser back
	curs_set(oldcurs);

    // Perform action
    switch (choice) {
        case 0: return TRUE; // QUIT
        case 1: save_changes(); break;
        case 2: abandon_changes(); break;
        case 3: handle_scrolling_movement(KEY_MOVE); break;
        case 4: insert_bytes(); break;
        case 5: delete_bytes(); break;
        default: break;
    }

    del_panel(menu_panel);
    delwin(menu_win);
    update_all_windows();
    handle_global_keys(KEY_RESIZE);
    return FALSE;
}
