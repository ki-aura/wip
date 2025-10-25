#include "gex.h"

void handle_click(clickwin win, int row, int col){
	// if we clicked outside hex or ascii windows then don't care
	if(win == WIN_OTHER) return;

	// handle_full_grid_clicks 		
	if(win == WIN_HEX) {
		// this so far only handles full panes
		app.in_hex = true;
		hex.cur_row = row;
		if(col==0){
			hex.cur_digit = 0;
			hex.cur_col   = 0;
			hex.is_hinib  = true;
		} else {
			hex.cur_digit = col/3;
			hex.cur_col = col; 
			// move back 1 if we clicked on a space in the grid
			if((hex.cur_col%3)==2)hex.cur_col--;
			// work out which nibble we're on
			if((hex.cur_col%3)==0)hex.is_hinib=true; else hex.is_hinib=false;
		}
	}
	
	if(win == WIN_ASCII){
		// this so far only handles full panes
		app.in_hex = false;
		hex.cur_row = row;
		if(col==0){
			hex.cur_digit = 0;
			hex.cur_col   = 0;
			hex.is_hinib  = true;
		} else {
			hex.cur_digit = col;
			hex.cur_col = (col *3);		
			hex.is_hinib=true;
		}
//snprintf(tmp,200,"(A) col %d digit %d row %d HiNib %d",hex.cur_col, hex.cur_digit, row, (int)hex.is_hinib); popup_question(tmp, "", PTYPE_CONTINUE);
	}
	return;
}

void k_left(void){
	if(app.in_hex){
		if (!hex.is_hinib){
			// we can safely move to left nib. don't move ascii cursor
			hex.cur_col--;
			hex.is_hinib=true;
		} else {
			// we're on hi nib. move back 2 unless at start of row, 
			// otherwise wrap back to end of row
			if(hex.cur_digit > 0){
				hex.cur_col-=2;
				hex.cur_digit--;
				hex.is_hinib=false;
			} else {
				hex.cur_col=((ascii.width-1)*3); //this time as go to hi nibble
				hex.cur_digit=ascii.width-1;	// first hex digit (takes 3 spaces)
				hex.is_hinib = true;	// left nibble of that digit
			}
		}
	} else { // we're in ascii pane
		if(hex.cur_digit > 0){
			hex.cur_col-=3;
			hex.cur_digit--;
			hex.is_hinib = true;
		} else { // wrap col
				hex.cur_col=((ascii.width-1)*3);
				hex.cur_digit=ascii.width-1;	// last hex digit (takes 3 spaces)
				hex.is_hinib = true;	// left nibble of that digit
		}
	}
}

void k_right(void){
	if(app.in_hex){
		if (hex.is_hinib){
			// we can safely move to right nib. don't move ascii cursor
			hex.cur_col++;
			hex.is_hinib=false;
		} else {
			// we're on right nib. move 2 unless at end of row, 
			// otherwise wrap back to start of row
			if(hex.cur_digit < (ascii.width-1)){
				hex.cur_col+=2;
				hex.cur_digit++;
				hex.is_hinib=true;
			} else {
				hex.cur_col=0;
				hex.cur_digit=0;	// first hex digit (takes 3 spaces)
				hex.is_hinib = true;	// left nibble of that digit
			}
		}		
	} else { // we're in ascii pane
		if(hex.cur_digit < (ascii.width-1)){
			hex.cur_col+=3;
			hex.cur_digit++;
			hex.is_hinib = true;
		} else { // wrap col
				hex.cur_col=0;
				hex.cur_digit=0;	// first hex digit (takes 3 spaces)
				hex.is_hinib = true;	// left nibble of that digit
		}
	}
}

void handle_in_screen_movement(int k){
app.lastkey = k;
int idx;

	switch (k){
	case KEY_TAB:
	//ensure on left nib on ascii pane return
		if(!hex.is_hinib){
			hex.cur_col--;
			hex.is_hinib=true;
		}
		// flip panes
		app.in_hex = !app.in_hex;
		break; 
	
	case KEY_NCURSES_BACKSPACE:
	case KEY_MAC_DELETE:
	case KEY_OTHER_DELETE:
	case KEY_LEFT:
		// do key left
		k_left();
		// maybe delete (i.e. undo)
		if(k != KEY_LEFT){
			// if we're not on a high-nibble, then move left again
			if(!hex.is_hinib) k_left();
			// find out where we are; check if there's an edit; if so delete it
			idx = row_digit_to_offset(hex.cur_row, hex.cur_digit);
			slot = kh_get(app.edmap, (size_t)(hex.v_start + idx));
            if (slot != kh_end(app.edmap)) // we have an edit; get rid of it
                    kh_del(app.edmap, slot);
			// update windows as highlights will  have changed
			update_all_windows();
		}
		break;
		
	case KEY_RIGHT:
		k_right();
		break;

	case KEY_HOME:
		hex.cur_col = 0;
		hex.cur_digit = 0;
		hex.cur_row = 0;
		hex.is_hinib = true;
		break;
		
	case KEY_END:
		hex.cur_col = hex.width - 3;
		hex.cur_digit = ascii.width-1;
		hex.cur_row = hex.height-1;
		hex.is_hinib = true;
		break;
	}
	update_cursor();
}

void handle_scrolling_movement(int k){

	switch (k){
	case KEY_UP:
		if(hex.cur_row > 0) {
			hex.cur_row--;
			update_cursor();
		} else {
			if(hex.v_start > (size_t)ascii.width) hex.v_start -= ascii.width;
			else hex.v_start = 0;
			update_all_windows();
		}
		break;
		
	case KEY_DOWN:
		if(hex.cur_row < (hex.height-1)) {
			hex.cur_row++;
			update_cursor();
		} else {
			// if there's not enough file to fill the grid, stay put
			if((size_t)hex.grid > app.fsize) hex.v_start = 0;
			// otherwise calc move
			else if((hex.v_start + hex.grid + ascii.width) < app.fsize) hex.v_start += ascii.width; 
				else hex.v_start = app.fsize - hex.grid;
			update_all_windows();			
		}
		break;
		
	case KEY_NPAGE:
		// if there's not enough file to fill the grid, stay put
		if((size_t)hex.grid > app.fsize) hex.v_start = 0;
		// if 2 grids of move < file size fine, otherwise move less. 2 grids to avoid half screens	
		else if(hex.v_start + (2 * hex.grid) < app.fsize) hex.v_start += hex.grid; 
			else hex.v_start = app.fsize - hex.grid;
		update_all_windows();
		break;
		
	case KEY_PPAGE:
		// if there's not enough file to fill the grid, stay put
		if((size_t)hex.grid > app.fsize) hex.v_start = 0;
		// otherwise calc how far to move back
		else if(hex.v_start > (size_t)hex.grid) hex.v_start -= hex.grid;
			else hex.v_start = 0;
		update_all_windows();
		break;
	
	// GOTO event
	case KEY_MOVE: // this is what the goto function will throw
        snprintf(tmp, 60, "Goto Byte? (0-%lu)", (unsigned long)app.fsize-1);
        // hex.v_start = will either be a new valid value or 0
        hex.v_start = popup_question(tmp, "", PTYPE_UNSIGNED_LONG);
		// BOUNDARY CHECKS 
		//if there's not enough file to fill the grid, stay put
		if((size_t)hex.grid >= app.fsize) hex.v_start = 0;
		// otherwise check result
		else if((hex.v_start + hex.grid) > app.fsize) 
			hex.v_start = app.fsize - hex.grid;
        update_all_windows();		
		//break;
	}
		
}

void handle_edit_keys(int k){
    int idx;
    unsigned char full_file_byte, full_edit_byte;
    bool valid_edit = false;
    
    // find out where we are
    idx = row_digit_to_offset(hex.cur_row, hex.cur_digit);
    
    // first off... if we're not within the bounds of the file then no edits
    if(hex.v_start + idx >= app.fsize) return;
    
    if (!app.in_hex) { // we're in ascii pane
        if (isprint(k)) {
            // if it's the same as the underlying file then get rid of any edit map
            if( k == app.map[hex.v_start + idx]){
                slot = kh_get(app.edmap, (size_t)(hex.v_start + idx));
                if (slot != kh_end(app.edmap))
                    kh_del(app.edmap, slot);
            }
            else {  // push the change onto the edit map
                slot = kh_put(app.edmap, (size_t)( hex.v_start + idx), &khret);
                kh_val(app.edmap, slot) = (unsigned char)k;
            }
            valid_edit = true;
        }
    } else {  // we are in hex pane
        if ((k >= '0' && k <= '9') || (k >= 'A' && k <= 'F') || (k >= 'a' && k <= 'f')) {
            // it's a valid nibble
            
            // get a copy of the underlying file byte to compare against
            full_file_byte = app.map[hex.v_start + idx];
            
            // if there's an existing change, we need to apply this nibble to that so that we 
            // can compare a byte that's had a hi & low edit done to it
            slot = kh_get(app.edmap, (size_t)(hex.v_start + idx));
            if (slot != kh_end(app.edmap))
            	// there's an existing change. get it and apply the nibble
            	full_edit_byte = kh_val(app.edmap, slot);
        	else 
            	// no existing change, so apply the the nibble to a copy of the file byte
            	full_edit_byte = full_file_byte;
            
			// now apply the nibble to the edited byte
			if (hex.is_hinib) apply_hinib_to_byte(&full_edit_byte, k);
			else apply_lonib_to_byte(&full_edit_byte, k);
			
            //is the edit the same as the underlying file?
            if( full_edit_byte == full_file_byte){
                // if it is, delete any change that's stored
                slot = kh_get(app.edmap, (size_t)(hex.v_start + idx));
                if (slot != kh_end(app.edmap))
                    kh_del(app.edmap, slot);
            } else { //otherwise, push the change
                slot = kh_put(app.edmap, (size_t)(hex.v_start + idx), &khret);
                kh_val(app.edmap, slot) = full_edit_byte;
            }
            valid_edit = true;
        }
    }
    if (valid_edit) {
    	app.lasteditkey = k;
        update_all_windows();
        handle_in_screen_movement(KEY_RIGHT);
    }
}
