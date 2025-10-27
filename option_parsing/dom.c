#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>

#include "long_opt.h"


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

