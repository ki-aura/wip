#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

// ===============================
// Constants used to validate options
// ===============================
// general
#define MAX_OPERANDS	256
#define NO_MAX_LEN -1

// option specific
#define OPT_D_MAX_DEPTH 6
#define OPT_PATTERN_MAX_LEN 10

// version to use in -v
#define PROG_VERSION	"1.x.x"


// ===============================
// Public Struct for Options
// ===============================
typedef struct {
	// we don't need to populate these variable as the functions handling them immediately 
	// exit, but they could included here if needed
/*
	bool help;
	bool version;	
*/
	// DO NOT REMOVE these are required to hold ALL the command line operands (excluding the options)
	// operands - as many as are specified on the command line
	char **operands;
	int operand_count;

	// USER MAINTAINED these are where the validated command line options are moved to
	// option variables
	int quiet;
	int depth;
	bool iterate;
	bool verbose;
	bool woo;
	// pattern will ony accept a single max sized char
	char *pattern;
	// excludes will accept repetitive options, so needs a count as well
	char **excludes;
	int exclude_count;
} Options;

// ===============================
// Public API prototypes
// ===============================

// Parses command-line options into a dynamically allocated Options struct.
// Exits with error on invalid input.
Options *parse_options(int argc, char *argv[], const char *default_operand, bool operands_are_not_required);

// Frees the Options structure and all its dynamically allocated members.
void free_options(Options *opts);


#endif // OPTIONS_H
