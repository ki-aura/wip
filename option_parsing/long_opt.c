#define _POSIX_C_SOURCE 200809L
#include "long_opt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>


// ===============================
// option definitions
// ===============================

// set long options
static struct option long_options[] = {
	{"help",    no_argument,       0, 'h'},
	{"version", required_argument, 0, 'V'},
	{"quiet",   required_argument, 0, 'q'},
	{"depth",   required_argument, 0, 'd'},
	{"iterate", no_argument,       0, 'i'},
	{"pattern", required_argument, 0, 'p'},
	{"exclude", required_argument, 0, 'e'},
	// don't include anything in here for -v as it has no long option
	{"woo", 	no_argument, 	   0, 1234}, // 1234 in place of a short option
	{0, 0, 0, 0}
};

//set short options
//NOTE: short options that need an argument must be followed by a :
const char short_options[] = "hVq:d:ip:e:v"; // nothing here for --woo as no short option
    
// ===============================
// Internal Functions
// ===============================

static void print_help(Options* opts, const char *prog_name) {
    printf("Usage: %s [OPTIONS] FILE...\n", prog_name);
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help message and exit\n");
    printf("  -V, --version           Show version and exit\n");
    printf("  -q, --quiet	          !MANDATORY! Set Quiet to 1 or 2\n");
    printf("  -d, --depth=NUM         Set depth (1-6)\n");
    printf("  -i, --iterate           Enable iteration mode\n");
    printf("  -p, --pattern=STRING    Set pattern (max 32 chars)\n");
    printf("  -e, --exclude=STRING    Exclude pattern (can be repeated)\n");
    printf("  -v                      verbose (example with no long option)\n");
    printf("      --woo               Enable WOO! mode (example with no short option)\n");
    printf("\nAt least one FILE operand is required.\n");
	free_options(opts);
	exit(EXIT_SUCCESS);
}

static void print_version(Options* opts, const char *prog_name) {
    printf("%s version: %s\n", prog_name, PROG_VERSION);
	free_options(opts);
	exit(EXIT_SUCCESS);
}

static void free_string_array(char **array, int count) {
    if (array) {
        for (int i = 0; i < count; i++) {
            free(array[i]);
        }
        free(array);
    }
}

static void check_int_argument(Options* opts, const char* optarg, const char* opt_name, int *int_value, int opt_min, int opt_max){
	char *end = NULL;
	errno = 0;
	long parsed = strtol(optarg, &end, 10);

	if (errno != 0 || end == optarg || *end != '\0') {
		fprintf(stderr, "Error: %s must be a whole number (got \"%s\")\n", opt_name, optarg);
		free_options(opts);
		exit(EXIT_FAILURE);
	}
	
	if (parsed < opt_min || parsed > opt_max) {
		fprintf(stderr, "Error: %s must be between %d and %d (got %ld)\n", opt_name, opt_min, opt_max, parsed);
		free_options(opts);
		exit(EXIT_FAILURE);
	}

	*int_value = (int)parsed;
	return;
}

// ===============================
// Public API functions
// ===============================

Options* parse_options(int argc, char *argv[]) {

	// Create opts structure and set any non-zero defaults
    Options *opts = calloc(1, sizeof(Options));
    if (!opts) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    // Initialize NON-ZERO defaults (calloc sets all to 0)
    opts->depth = MAX_DEPTH; // set default for if depth option not specified
    
    int opt; // iterator as we parse the options
    int option_index = 0; // used by getopt_long - ignored by us unless we want to know which option is being processed
    
    while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (opt) {
			// ------ standard help & version options --------
            case 'h': print_help(opts, argv[0]); break;
            case 'V': print_version(opts, argv[0]); break;

			// ------ simple bool options --------
            case 'v':  opts->verbose = true; break;
            case 'i':  opts->iterate = true; break;
            case 1234: opts->woo = true; break;
                
			// ------ int options --------
			case 'q':
				check_int_argument(opts, optarg, "quiet", &(opts->quiet), 1, 2);
				break;

			case 'd':
				check_int_argument(opts, optarg, "depth", &(opts->depth), 1, 6);
				break;
                            
			// ------ string options --------
			// handle one simple string
            case 'p':
				// Reject empty pattern string
               	if (optarg == NULL || optarg[0] == '\0') {
                    fprintf(stderr, "Error: pattern can not be empty\n");
                    free_options(opts);
                    exit(EXIT_FAILURE);
               	}
               	// reject too long pattern
               	if (strlen(optarg) > MAX_PATTERN_LEN) {
                    fprintf(stderr, "Error: pattern exceeds maximum length of %d characters\n", MAX_PATTERN_LEN);
                    free_options(opts);
                    exit(EXIT_FAILURE);
               	}
               	// otherwise copy the argument
               	strncpy(opts->pattern, optarg, MAX_PATTERN_LEN);
                opts->pattern[MAX_PATTERN_LEN] = '\0';
                break;
            
            // handle repetitive string options 
			case 'e': {
				// Reject empty exclude strings
				if (optarg == NULL || optarg[0] == '\0') {
					fprintf(stderr, "Error: exclude cannot be empty\n");
					free_options(opts);
					exit(EXIT_FAILURE);
				}
			
				// Reallocate the excludes array to add one more entry
				char **new_excludes = realloc(opts->excludes,
											  (opts->exclude_count + 1) * sizeof(char *));
				if (!new_excludes) {
					fprintf(stderr, "Error: Memory allocation failed for excludes\n");
					free_options(opts);
					exit(EXIT_FAILURE);
				}
				opts->excludes = new_excludes;
			
				// Duplicate the exclude string
				opts->excludes[opts->exclude_count] = strdup(optarg);
				if (!opts->excludes[opts->exclude_count]) {
					fprintf(stderr, "Error: Memory allocation failed for exclude string\n");
					free_options(opts);
					exit(EXIT_FAILURE);
				}
			
				opts->exclude_count++;
				break;
			}
                
			// ------ standard handler options --------
            case '?':
            	// we only get here if getopt_long has already found an error and printed error message
                free_options(opts);
                exit(EXIT_FAILURE);
                
            default:
   				fprintf(stderr, "Internal error: unexpected getopt_long return value: %d\n", opt);
                free_options(opts);
                exit(EXIT_FAILURE);
        }
    }
    
    // check quiet - it's a mandatory option, so if not set then an error
     if (opts->quiet == 0) {
        fprintf(stderr, "Error: -q / --quiet MUST be set (use -h for help)\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
       
	// optind tells us where the first non-option argument is (i.e., the first operand)
	// If there are no operands, optind == argc
    opts->operand_count = argc - optind;
    
	// NOTE: getopt_long re-orders argv so that options come first, operands at end:
	//    demo -d 1 *.c --exclude "fred" *h --woo
	// is reordered to:
	//    demo -d 1 --exclude "fred" --woo *.c *.h
	// So optind would be 6, pointing to *.c at argv[6], even though 
	// *.c was originally at argv[3] on the command line

    
    // Collect operands
    if (opts->operand_count < 1) {
        fprintf(stderr, "Error: at least one FILE operand is required (use -h for help)\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
    
    if (opts->operand_count > MAX_OPERANDS) {
        fprintf(stderr, "Error: too many operands (max %d)\n", MAX_OPERANDS);
        free_options(opts);
        exit(EXIT_FAILURE);
    }
    
    opts->operands = malloc(opts->operand_count * sizeof(char*));
    if (!opts->operands) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_options(opts);
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < opts->operand_count; i++) {
        opts->operands[i] = strdup(argv[optind + i]);
        if (!opts->operands[i]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            // Clean up already allocated operands - partial cleanup before calling free_options
            // We need to set operand_count to i so free_options knows how many to free
            opts->operand_count = i;
            free_options(opts);
            exit(EXIT_FAILURE);
        }
    }
    
    return opts;
}

void free_options(Options *opts) {
    if (opts) {
        free_string_array(opts->operands, opts->operand_count);
        free_string_array(opts->excludes, opts->exclude_count);
        free(opts);
    }
}

