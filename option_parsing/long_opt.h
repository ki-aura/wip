#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

// ===============================
// Constants
// ===============================
#define MAX_PATTERN_LEN 32
#define MAX_OPERANDS    256
#define MAX_DEPTH       6
#define PROG_VERSION	"1.x.x"

// ===============================
// Structs
// ===============================
typedef struct {
/* we don't populate these are they immediately exit, but they could be here if needed
    bool help;
    bool version;	
*/
    // options
    int quiet;
    int depth;
    bool iterate;
    char pattern[MAX_PATTERN_LEN + 1];
    bool verbose;
    bool woo;
    char **excludes;
    int exclude_count;
    // operands
    char **operands;
    int operand_count;
} Options;

// ===============================
// Public API prototypes
// ===============================

// Parses command-line options into a dynamically allocated Options struct.
// Exits with error on invalid input.
Options *parse_options(int argc, char *argv[]);

// Frees the Options structure and all its dynamically allocated members.
void free_options(Options *opts);

// ===============================
// Internal Function prototypes
// ===============================

static void print_help(Options* opts, const char *prog_name);
static void print_version(Options* opts, const char *prog_name);

// Frees an array of dynamically allocated strings (e.g. operands, excludes).
static void free_string_array(char **array, int count);

// Validates and parses an integer argument (helper for numeric options).
// Exits on error.
static void check_int_argument(Options *opts,
                        const char *optarg,
                        const char *opt_name,
                        int *int_value,
                        int opt_min,
                        int opt_max);

#endif // OPTIONS_H
