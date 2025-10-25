#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h> /* Present on macOS and Linux; getopt_long() guarded below */
#include <sys/types.h>
#include "opt_parse.h"


/* 	
    char        short_opt;       // e.g. 'h' or '\0' if none 
    const char *long_opt;        // e.g. "help" or "" - must NOT be NULL 
    opt_type_t  type;            // OPT_BOOL, OPT_INT, OPT_FLOAT, OPT_STRING 
    double      min_val;         // numeric only; ignored if min_val > max_val 
    double      max_val;         // numeric only; ignored if min_val > max_val 
    const char *help;            // help text (may be NULL) 
    int         required;        // if non-bool: must appear at least once? (0/1) 
    int         repeatable;      // for STRING only; collect multiple occurrences (0/1) 
    const char *default_str;     // default for STRING (NULL if none) 
    long long   default_int;     // default for INT 
    double      default_flt;     // default for FLOAT 
    int         default_bool;    // default for BOOL (0/1) 
*/

static const opt_spec_t DEMO_SPECS[] = {
    { 'h', "help",   OPT_BOOL,  0, 0, "show help",              0, 0, NULL, 0,     0.0, 0 },
    { 'v', "version",OPT_BOOL,  0, 0, "show version",           0, 0, NULL, 0,     0.0, 0 },
    { 'd', "depth",  OPT_INT,   1, 6, "max depth (1..6)",       0, 0, NULL, 3,     0.0, 0 },
    { 'p', "", 	     OPT_STRING,0, 0, "pattern to search for",  0, 0, NULL, 0,     0.0, 0 },
    { '\0', "moose", OPT_BOOL,  0, 0, "look for a moose",  		0, 0, NULL, 0,     0.0, 0 },
    { 't', "tax",    OPT_FLOAT, 0.0,1.0,"tax rate (0..1)",      1, 0, NULL, 0,     0.05,0 },
    { 'I', "Include",OPT_STRING,0, 0, "include path (repeat)",  0, 1, NULL, 0,     0.0, 0 },
};

static const operand_policy_t DEMO_POLICY = {
    .required = 0,
    .default_if_none = ".",
    .allow_stdin = 1
};

static void print_results(const parse_result_t *pr, const opt_spec_t *specs, size_t n) {
    printf("stdin_is_pipe: %s\n", pr->stdin_is_pipe ? "yes" : "no");
    for (size_t i = 0; i < n; i++) {
        const opt_spec_t *s = &specs[i];
        const char *lname = s->long_opt ? s->long_opt : "";
        const char shortbuf[3] = { s->short_opt ? '-' : 0, s->short_opt ? s->short_opt : 0, 0 };
        printf("[%zu] %s%s%s (%s): present=%d count=%d value=",
               i,
               s->short_opt ? shortbuf : "",
               (s->short_opt && lname[0]) ? "|" : "",
               lname[0] ? (const char [260]){0} : "",
               type_name(s->type),
               pr->opt[i].present, pr->opt[i].count);
        if (lname[0]) printf("--%s ", lname);
        switch (s->type) {
            case OPT_BOOL:  printf("%s", pr->opt[i].b ? "true" : "false"); break;
            case OPT_INT:   printf("%lld", pr->opt[i].i); break;
            case OPT_FLOAT: printf("%.6g", pr->opt[i].f); break;
            case OPT_STRING:
                if (s->repeatable) {
                    printf("[");
                    for (int k = 0; k < pr->opt[i].slist_len; k++) {
                        printf("%s\"%s\"", (k ? ", " : ""), pr->opt[i].slist[k]);
                    }
                    printf("]");
                } else {
                    printf("%s", pr->opt[i].s ? pr->opt[i].s : "(null)");
                }
                break;
        }
        printf("\n");
    }

    printf("operands (%d):", pr->num_paths);
    for (int i = 0; i < pr->num_paths; i++) {
        printf(" \"%s\"", pr->paths[i]);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    if (set_opts(DEMO_SPECS, sizeof(DEMO_SPECS)/sizeof(DEMO_SPECS[0]), &DEMO_POLICY) != 0) {
        return 2;
    }

    int rc = parse_opts(argc, argv);
    if (rc != 0) {
        fprintf(stderr, "Parse error (rc=%d). Try -h/--help.\n", rc);
        destroy_opts();
        return 2;
    }

    /* Demonstration: manual handling of help/version (no auto-injection behavior) */
    const parse_result_t *pr = get_parse_result();
    /* Find indices for help/version in demo schema */
    size_t idx_help = 0, idx_ver = 1; /* known positions in DEMO_SPECS */

    if (pr->opt[idx_help].present && pr->opt[idx_help].b) {
        show_help(stdout, argv[0]);
        destroy_opts();
        return 0;
    }
    if (pr->opt[idx_ver].present && pr->opt[idx_ver].b) {
        show_version(stdout, argv[0]);
        destroy_opts();
        return 0;
    }

    /* Dump parsed results */
    print_results(pr, DEMO_SPECS, sizeof(DEMO_SPECS)/sizeof(DEMO_SPECS[0]));

    destroy_opts();
    return 0;
}

