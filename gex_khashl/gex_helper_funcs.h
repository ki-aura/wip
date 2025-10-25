#ifndef GEX_HELPER_FUNCS_H
#define GEX_HELPER_FUNCS_H

///////////////////////////////////////////////////////////////////////////////
// general
///////////////////////////////////////////////////////////////////////////////


unsigned long  popup_question(const char *qline1, const char *qline2, popup_types pt);
char byte_to_ascii(unsigned char b);

///////////////////////////////////////////////////////////////////////////////
// ascii / byte / nibble manipulation
///////////////////////////////////////////////////////////////////////////////

// t[3] = nibs_to_byte('4', '6');
unsigned char nibs_to_byte(const char hi, const char lo);

// apply_hinib_to_byte(t+2, '4');
void apply_hinib_to_byte(unsigned char *byte, const char hi);
void apply_lonib_to_byte(unsigned char *byte, const char lo);

//byte_to_nibs(t[8], &nibhi, &niblo);
void byte_to_nibs(const unsigned char byte, char *hi, char *lo);


///////////////////////////////////////////////////////////////////////////////
// file offset to / from screen calcs
///////////////////////////////////////////////////////////////////////////////

// hex screen is 3 x number of digits per col wide (hi lo space)
// screen offsets are based from 0 row, 0 col
// fo = file offset app.map[i]

// returns the row and col of the hi nibble of an offset byte + ascii col
bool file_offset_to_rc(int byte_offset, int *row, int *h_col, int *a_col);

// convert r/c to offset
int row_digit_to_offset(int row, int digit);

unsigned long cursor_full_file_offset(void);

const char *get_filename(const char *path);

#endif 






















