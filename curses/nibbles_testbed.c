#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <panel.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <menu.h>
#include <assert.h>
#include "khash.h"



// IMPORTANT NOTE
// nibbles are printable chars 0123456789ABCDEF
// bytes are unsigned chars - they come from a file

// helper function for byte_to_nibs only
unsigned char HELPER_hexnib_to_char(unsigned char hex_nibble) {
    if (hex_nibble < 10) return hex_nibble + '0';
    else return hex_nibble - 10 + 'A';
}
// helper function for bit to byte functions only 
int HELPER_nib_to_hexval(const char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	assert(0 && "invalid hex digit found");
	return -1; // error
}

// MAIN FUNCTIONS HEX & ASCII CONVERSION FUNCTION //

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

int main()
{
	unsigned char *t;
	char nib1, nib2;
	t = malloc(100);
	strcpy((char *)t, "this is A pretend binary file");
	
	byte_to_nibs(t[8], &nib1, &nib2);
	printf("nibbles from map location 8 %c %c \n", nib1, nib2);
	
	apply_hinib_to_byte(t+2, '4');
	apply_lonib_to_byte(t+2, '1');
	printf("%s\n", t);

	t[3] = nibs_to_byte('4', '6');
	printf("%s\n", t);
	
	nib1=HELPER_nib_to_hexval('G');
	
	free(t);
	return 0;
}
