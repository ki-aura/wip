#include "gex.h"

// IMPORTANT NOTE
// nibbles are printable chars 0123456789ABCDEF
// bytes are unsigned chars - they come from a file

// helper function for byte_to_nibs only
static unsigned char HELPER_hexnib_to_char(unsigned char hex_nibble) {
    if (hex_nibble < 10) return hex_nibble + '0';
    else return hex_nibble - 10 + 'A';
}
// helper function for bit to byte functions only 
static int HELPER_nib_to_hexval(const char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	// if we get here bad things have happened...
	snprintf(tmp, 200, "invalid hex digit found %c %u", c, c );
	popup_question(tmp, "", PTYPE_CONTINUE);
	assert(0 && "invalid digit in hex");
	return -1; // error
}

// MAIN FUNCTIONS HEX & ASCII CONVERSION FUNCTIONS //

// convert nibbles to a byte. e.g. set byte to 0x41 (char 'A'): byte= nibs_to_byte('4', '1');
unsigned char nibs_to_byte(const char hi, const char lo) {
	return (unsigned char)((HELPER_nib_to_hexval(hi) << 4) |HELPER_nib_to_hexval(lo));
}
// takes a binary file byte and updates the hi nibble
void apply_hinib_to_byte(unsigned char *byte, const char hi) {
	// set hi nibble to zero
	*byte = *byte & 0x0F; 	// byte[0] = byte[0] & 0x0F;
	// apply hi nibble
	*byte = ((*byte) | (HELPER_nib_to_hexval(hi) << 4));
}
// takes a binary file byte and updates the low nibble
void apply_lonib_to_byte(unsigned char *byte, const char lo) {
	// set hi nibble to zero
	*byte = *byte & 0xF0;
	// set lo nibble to passed in nibble	
	*byte = ((*byte) | (HELPER_nib_to_hexval(lo)));
}

// deconstruct byte to it's hi and low displayable nibbles: hex_to_nibs(byte, &hi, &lo);
void byte_to_nibs(const unsigned char byte, char *hi, char *lo) {
    *hi = HELPER_hexnib_to_char((byte >> 4) & 0xF);
    *lo = HELPER_hexnib_to_char(byte & 0xF);
}

char byte_to_ascii(unsigned char b) {
    return (isprint(b) ? (char)b : '.');
}

// MAIN SCREEN CONVERSION FUNCTIONS //

bool file_offset_to_rc(int byte_offset, int *row, int *h_col, int *a_col){
	if((hex.v_start + byte_offset) >= app.fsize) return false;  // >= as offset is zero based so must be 1 less than size
	*row = byte_offset / ascii.width;
	*a_col = byte_offset - (ascii.width * (*row));
	*h_col = (*a_col) * 3;
	return true;
}

int row_digit_to_offset(int row, int digit){
	return (row * ascii.width) + digit;
}

unsigned long cursor_full_file_offset(void){
	// get current location on screen + grid offset
    unsigned long idx = hex.v_start + row_digit_to_offset(hex.cur_row, hex.cur_digit);
    // and adjust if there's not enough file to fill the grid
	if(idx >= app.fsize) idx = app.fsize-1;
	return idx;
}

// STRING HANDLING
const char *get_filename(const char *path){
    if (path == NULL) return NULL;

    const char *last = strrchr(path, '/');
    if (last) return last + 1;  // character after last '/'
    return path;          // no slash, whole string is filename
}

unsigned long  popup_question(const char *qline1, const char *qline2, popup_types pt) {
	int ch, qlen, oldcs1, oldcs2;
	char *endptr;
	unsigned long answer;

	// make sure we size to the longer of the question lines (and at least 21 so a 20byte long can be typed)
	qlen = (int)((strlen(qline1) > strlen(qline2)) ? strlen(qline1) : strlen(qline2));
	qlen = (qlen < 21) ? 21 : qlen;
	// Create window and panel
	WINDOW *popup = newwin(4, (qlen+2), ((app.rows - 4) / 2), 
					((app.cols - (qlen+2)) / 2));
	PANEL  *panel = new_panel(popup);
	keypad(popup, TRUE); // Enable keyboard input for the window
	
	// Draw border and message
	box(popup, 0, 0);
	wattron(popup, A_BOLD);
	mvwprintw(popup, 1, 1, "%s", qline1);
	mvwprintw(popup, 2, 1, "%s", qline2);
	wattroff(popup, A_BOLD);
	
	// Show it
	oldcs1 = curs_set(0);
	update_panels();
	doupdate();
	
	switch(pt){
	case PTYPE_YN:	// don't end until y or n typed
		do {
			ch = wgetch(popup);
		} while ((ch != 'y') && (ch != 'n'));
		answer = (unsigned long)(ch == 'y');
		break;

	case PTYPE_CONTINUE: 	// end after any key
		wgetch(popup);	
		answer = (unsigned long)true;
		break;
	
	case PTYPE_UNSIGNED_LONG:	// get a new file location (or default to 0 if invalid input)
		// Move the cursor to the input position and get input
		echo(); oldcs2 = curs_set(2);
		mvwgetnstr(popup, 2, 1, tmp, 16); // 20 is max length of a 64bit unsigned long
		noecho(); curs_set(oldcs2);
		
		// Convert string to unsigned long using strtoul
		errno = 0; // Clear errno before the call
		answer = strtoul(tmp, &endptr, 10);
		
		// Check for conversion errors
		if (tmp[0] == '-' || endptr == tmp || *endptr != '\0' || errno == ERANGE)
			answer = 0;
		break;
	}

	// Clean up panel
	curs_set(oldcs1);
	hide_panel(panel);
	update_panels();
	doupdate();
	del_panel(panel);
	delwin(popup);

	return answer;
}




