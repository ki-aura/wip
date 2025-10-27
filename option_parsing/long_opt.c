#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>

// =========== TO COMPILE WITH DEMO MAIN CODE =========================
// cc -o options_demo long_opt.c -DDEMO
// -g -O0 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-qual -fsanitize=address,undefined
// ====================================================================

#define _INCLUDE_INTERNAL_OPT_FUNCTIONS
#include "long_opt.h"

// internal function prototypes
static void print_help(Options *opts, const char *prog_name, const char *default_operand);
static void print_version(Options *opts, const char *prog_name);
static void free_string_array(char **array, int count);
static void validate_option_int(Options *opts, const char *arg, const char *error_name,int *arg_value, int opt_min, int opt_max);
static void validate_option_simple_string(Options *opts, const char *arg, const char *error_name, char **arg_value, bool can_be_empty, int max_length);
static void validate_option_repeated_string(Options *opts, const char *arg, const char *error_name, char ***arg_array, int *array_len,bool can_be_empty, int max_length);
static void parse_options_basic_validation(Options *opts, int argc, char *argv[], const char *default_operand);
static void parse_options_complex_validation(Options *opts);
static void parse_options_collate_operands(Options *opts, int argc, char *argv[], const char *default_operand);

// ===============================
// Option definitions 
// ===============================

// ============ set LONG options ===============
static struct option long_options[] = {
	{"help",    no_argument,       	0, 'h'},
	{"version", no_argument, 		0, 'v'},
	{"quiet",   required_argument, 	0, 'q'},
	{"depth",   required_argument, 	0, 'd'},
	{"iterate", no_argument,       	0, 'i'},
	{"pattern", required_argument, 	0, 'p'},
	{"exclude", required_argument, 	0, 'e'},
	// don't include anything in here for -V (verbose) as it has no long option
	{"woo", 	no_argument, 	   	0, 1234}, // 1234 in place of a short option
	{0, 0, 0, 0}
};

// ============ set SHORT options ===============
//NOTE: short options that need an argument must be followed by a : & be consistent with longs
const char short_options[] = "hvq:d:ip:e:V"; // nothing here for --woo as no short option
  
// ============ HELP FUNCTION ===================
static void print_help(Options* opts, const char *prog_name, const char*default_operand) {
    printf("Usage: %s [OPTIONS] FILE...\n", prog_name);
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help message and exit\n");
    printf("  -v, --version           Show version and exit\n");
    printf("  -q, --quiet	          !MANDATORY! Set Quiet to 1 or 2\n");
    printf("  -d, --depth=NUM         Set depth (1-6). Default is 6\n");
    printf("  -i, --iterate           Enable iteration mode\n");
    printf("  -p, --pattern=STRING    Set pattern (max 10 chars)\n");
    printf("  -e, --exclude=STRING    Exclude pattern (can be repeated)\n");
    printf("  -V                      verbose (example with no long option)\n");
    printf("      --woo               Enable WOO! mode (example with no short option)\n");
	if(default_operand) {
		printf("\nIf no FILE operand is specified, %s will be default.\n", default_operand);
	} else {
    	printf("\nAt least one FILE operand is required.\n");
	}
	free_options(opts);
	exit(EXIT_SUCCESS);
}

// ============ BASIC OPTION PARSING ===============
static void parse_options_basic_validation(Options* opts, int argc, char *argv[], const char*default_operand) {
    int opt, option_index = 0; 

    // Initialize any NON-ZERO defaults (initial option calloc sets everything to 0 / NULL)
    opts->depth = OPT_D_MAX_DEPTH; // set default for if depth option not specified
    
	// parse the options
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
			// ------ standard help & version options --------
            case 'h': print_help(opts, argv[0], default_operand); break;
            case 'v': print_version(opts, argv[0]); break;

			// ------ simple bool options --------
            case 'V':  opts->verbose = true; break;
            case 'i':  opts->iterate = true; break;
            case 1234: opts->woo = true; break;
                
			// ------ int options --------
			case 'q':
				validate_option_int(opts, optarg, "quiet", &(opts->quiet), 1, 2); 
				break;
			case 'd':
				validate_option_int(opts, optarg, "depth", &(opts->depth), 1, OPT_D_MAX_DEPTH);
				break;
                            
			// ------ string options --------
			// handle one simple string
            case 'p':
				validate_option_simple_string(opts, optarg, "pattern", &(opts->pattern), 
							false, OPT_PATTERN_MAX_LEN); // don't allow empty pattern, limit length
                break;
            
            // handle repetitive string options 
			case 'e':
				validate_option_repeated_string(opts, optarg, "exclude", &opts->excludes, &opts->exclude_count, 
							true, NO_MAX_LEN); // allow empty excludes, no max size
				break;
                
			// ------ standard handler options --------
            case '?':
            	// we only get here if getopt_long has already found an error and printed error message
                free_options(opts);
                exit(EXIT_FAILURE);
            default:
            	// only way here is if we've messed up coding - e.g. have a -x option and no handler for it
   				fprintf(stderr, "Internal error: unexpected getopt_long return value: %d\n", opt);
                free_options(opts);
                exit(EXIT_FAILURE);
        }
    }
}

// ============ COMPLEX (PROGRAMME SPECIFIC) OPTION PARSING ===============
static void parse_options_complex_validation(Options* opts){
    // example of mandatory option: check quiet - it's a mandatory option, so if not set then an error
     if (opts->quiet == 0) {
        fprintf(stderr, "Error: -q / --quiet MUST be set (use -h for help)\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
	// if we had -start and -end and need both or neither, the logic for that would be here	
	// and so on..
	// if you don't need any, just replace with: (void)opts;
}

// ============ VERSION FUNCTION ===================
static void print_version(Options* opts, const char *prog_name) {
	// If you want to hard code the programme name, add the line (void)prog_name; to stop compiler warnings
    printf("%s version: %s\n", prog_name, PROG_VERSION);
	free_options(opts);
	exit(EXIT_SUCCESS);
}


// ===============================
// Other helper Functions (shouldn't need editing unless adding new functionality)
// ===============================

static void free_string_array(char **array, int count) {
    if (array) {
        for (int i = 0; i < count; i++) {
            free(array[i]);
        }
        free(array);
    }
}

static void validate_option_int(Options* opts, const char* arg, const char* error_name, 
								int *arg_value, int opt_min, int opt_max){
	// EXAMPLE of a check function. Could be for just max/just min / float / string / etc
	char *end = NULL;
	errno = 0;
	long parsed = strtol(arg, &end, 10);

	if (errno != 0 || end == arg || *end != '\0') {
		fprintf(stderr, "Error: %s must be a whole number (got \"%s\")\n", error_name, arg);
		free_options(opts);
		exit(EXIT_FAILURE);
	}
	
	if (parsed < opt_min || parsed > opt_max) {
		fprintf(stderr, "Error: %s must be between %d and %d (got %ld)\n", error_name, opt_min, opt_max, parsed);
		free_options(opts);
		exit(EXIT_FAILURE);
	}

	*arg_value = (int)parsed;
	return;
}

static void validate_option_simple_string(Options* opts, const char* arg, const char* error_name, 
								char **arg_value, bool can_be_empty, int max_length){
	// this first should never trigger but guards for next check
    if (arg == NULL) {
        fprintf(stderr, "Error: %s missing argument\n", error_name);
        free_options(opts);
        exit(EXIT_FAILURE);
    }
 	// Check/Reject empty pattern string 
	if (!can_be_empty && arg[0] == '\0') {
		fprintf(stderr, "Error: %s can not be empty\n", error_name);
		free_options(opts);
		exit(EXIT_FAILURE);
	}
	// reject too long pattern
	if (max_length > 0 && (int)strlen(arg) > max_length) {
		fprintf(stderr, "Error: %s exceeds maximum length of %d characters\n", error_name, max_length);
		free_options(opts);
		exit(EXIT_FAILURE);
	}
	// otherwise copy the argument
	*arg_value = strdup(arg);
	if (!(*arg_value)) {
		fprintf(stderr, "Error: Memory allocation failed for %s string\n", error_name);
		free_options(opts);
		exit(EXIT_FAILURE);
	}
}

static void validate_option_repeated_string(Options *opts, const char *arg, const char *error_name,
    							char ***arg_array, int *array_len, bool can_be_empty, int max_length){
    // Reallocate array for one more string
    char **new_array = realloc(*arg_array, (size_t)(*array_len + 1) * sizeof(char *));
    if (!new_array) {
        fprintf(stderr, "Error: Memory allocation failed for %s array\n", error_name);
        free_options(opts);
        exit(EXIT_FAILURE);
    }
    *arg_array = new_array;

    // Reuse the existing single-string validator to check and strdup
    validate_option_simple_string(opts, arg, error_name, &((*arg_array)[*array_len]), can_be_empty, max_length);
    // we now have one more in the array
    (*array_len)++;
}

static void parse_options_collate_operands(Options *opts, int argc, char *argv[], const char *default_operand){
	// optind tells us where the first non-option argument is (i.e., the first operand)
	// NOTE: getopt_long re-orders argv so that options come first, operands at end:
	//    demo -d 1 *.c --exclude "fred" *h --woo
	// is reordered to:
	//    demo -d 1 --exclude "fred" --woo *.c *.h
	// So optind would be 6, pointing to *.c at argv[6], even though 
	// *.c was originally at argv[3] on the command line

	// If there are no operands, optind == argc
    opts->operand_count = argc - optind;
   
    // Collect operands    
    if (opts->operand_count > MAX_OPERANDS) {
        fprintf(stderr, "Error: too many operands (max %d)\n", MAX_OPERANDS);
        free_options(opts);
        exit(EXIT_FAILURE);
    }
	
	// fail if there's nothing after the options AND no default has been specified
    if (opts->operand_count == 0 && default_operand == NULL) {
        fprintf(stderr, "Error: at least one FILE operand is required (use -h for help)\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
	
	// flag to use the default if there's no target specified 
	bool use_default = false;
    if (opts->operand_count == 0) {
    	use_default = true;
    	opts->operand_count = 1;
    }

    opts->operands = malloc((size_t)opts->operand_count * sizeof(char*));
    if (!opts->operands) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < opts->operand_count; i++) {
        opts->operands[i] = strdup(use_default ? default_operand : argv[optind + i]);
        if (!opts->operands[i]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            // Clean up already allocated operands - partial cleanup before calling free_options
            // We need to set operand_count to i so free_options knows how many to free
            opts->operand_count = i;
            free_options(opts);
            exit(EXIT_FAILURE);
        }
    }
}

// ===============================
// Public API functions
// ===============================

Options* parse_options(int argc, char *argv[], const char *default_operand) {
	// Create opts structure using calloc to set ALL to a default of zero or NULL
    Options *opts = calloc(1, sizeof(Options));
    if (!opts) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    
    // do the first pass of options just to get the values passed & complete basic validataion
    parse_options_basic_validation(opts, argc, argv, default_operand);
 	// perform any programme specific validation (e.g. mandatory options or pairs)
    parse_options_complex_validation(opts);
    // parse all of the non-option operands
    parse_options_collate_operands(opts,  argc, argv, default_operand);
       
    return opts;
}

void free_options(Options *opts) {
	// ===============================
	// THIS FUNCTION MUST BE UPDATED FOR EVERY CHAR / CHAR ARRAY IN THE OPTIONS DEFINITION
	// ===============================
    if (opts) {
    	// free single chars
    	if(opts->pattern) free(opts->pattern);
    	// free arrays of chars
        free_string_array(opts->operands, opts->operand_count);
        free_string_array(opts->excludes, opts->exclude_count);
        // finally free the overall opt container
        free(opts);
    }
}

// ===============================
// OPTIONAL DEMO MAIN
// ===============================
#ifdef DEMO

int main(int argc, char *argv[]) {
//    Options *opts = parse_options(argc, argv, ".");		  //set a default operand
    Options *opts = parse_options(argc, argv, NULL); 	  // no default operand
    
    // Print parsed options
    printf("Parsed Options:\n");
    printf("  quiet:    %d\n", opts->quiet);
    printf("  depth:    %d\n", opts->depth);
    printf("  iterate:  %s\n", opts->iterate ? "true" : "false");
    printf("  pattern:  \"%s\"\n", opts->pattern? opts->pattern : "(not set)");
    printf("  verbose:  %s\n", opts->verbose ? "true" : "false");
    printf("  woo:      %s\n", opts->woo ? "true" : "false");

    printf("\nExcludes (%d):\n", opts->exclude_count);
    for (int i = 0; i < opts->exclude_count; i++) {
        printf("  [%d] \"%s\"\n", i, opts->excludes[i]);
    }
    printf("\nOperands (%d):\n", opts->operand_count);
    for (int i = 0; i < opts->operand_count; i++) {
        printf("  [%d] \"%s\"\n", i, opts->operands[i]);
    }
    
    free_options(opts);
    return EXIT_SUCCESS;
}

#endif
