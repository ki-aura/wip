#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "opt_parse.h"

/* -------------------------------------------------------------------------
 * DEMO PROGRAM
 * -------------------------------------------------------------------------
 * Demonstrates usage of the short-option parser.
 *   - shows help/version handling
 *   - prints all parsed values in a readable format
 * ------------------------------------------------------------------------- */

/*
    char        short_opt;       // e.g. 'h'
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
    { 'h', OPT_BOOL,   0, 0, "show help",              0, 0, NULL, 0, 0.0, 0 },
    { 'v', OPT_BOOL,   0, 0, "show version",           0, 0, NULL, 0, 0.0, 0 },
    { 'd', OPT_INT,    1, 6, "depth (1–6)",            0, 0, NULL, 3, 0.0, 0 },
    { 'p', OPT_STRING, 0, 0, "pattern to search for",  0, 0, NULL, 0, 0.0, 0 },
    { 't', OPT_FLOAT,  0, 1, "tax rate (0–1)",         1, 0, NULL, 0, 0.05,0 },
    { 'I', OPT_STRING, 0, 0, "include path (repeatable)", 0, 1, NULL, 0, 0.0, 0 },
};

#define N_OPTS (sizeof(DEMO_SPECS)/sizeof(DEMO_SPECS[0]))

static const operand_policy_t DEMO_POLICY = {
    .required = 0,
    .default_if_none = ".",
    .allow_stdin = 1
};

/* -------------------------------------------------------------------------
 * Helper: find index of a short option in DEMO_SPECS
 * ------------------------------------------------------------------------- */
static int find_opt(char c) {
    for (size_t i = 0; i < N_OPTS; i++)
        if (DEMO_SPECS[i].short_opt == c) return (int)i;
    return -1;
}

/* -------------------------------------------------------------------------
 * Helper: print parsed results in readable form
 * ------------------------------------------------------------------------- */
static void print_results(const parse_result_t *pr) {
    printf("\nParsed results:\n");
    printf("  stdin_is_pipe: %s\n\n", pr->stdin_is_pipe ? "yes" : "no");

    for (size_t i = 0; i < N_OPTS; i++) {
        const opt_spec_t *s = &DEMO_SPECS[i];
        const struct OptVal *ov = &pr->opt[i];

        printf("  -%c (%s): ", s->short_opt, type_name(s->type));
        if (!ov->present) {
            printf("not provided (default ");
            switch (s->type) {
                case OPT_BOOL:  printf("%s", s->default_bool ? "true" : "false"); break;
                case OPT_INT:   printf("%lld", s->default_int); break;
                case OPT_FLOAT: printf("%.6g", s->default_flt); break;
                case OPT_STRING:printf("%s", s->default_str ? s->default_str : "NULL"); break;
            }
            printf(")\n");
            continue;
        }

        switch (s->type) {
            case OPT_BOOL:  printf("%s", ov->b ? "true" : "false"); break;
            case OPT_INT:   printf("%lld", ov->i); break;
            case OPT_FLOAT: printf("%.6g", ov->f); break;
            case OPT_STRING:
                if (s->repeatable) {
                    printf("[");
                    for (int k = 0; k < ov->slist_len; k++)
                        printf("%s\"%s\"", k ? ", " : "", ov->slist[k]);
                    printf("]");
                } else {
                    printf("%s", ov->s ? ov->s : "(null)");
                }
                break;
        }
        printf("  (count=%d)\n", ov->count);
    }

    printf("\nOperands (%d):", pr->num_paths);
    for (int i = 0; i < pr->num_paths; i++)
        printf(" \"%s\"", pr->paths[i]);
    printf("\n\n");
}

/* -------------------------------------------------------------------------
 * MAIN
 * ------------------------------------------------------------------------- */
int main(int argc, char **argv) {

    if (set_opts(DEMO_SPECS, N_OPTS, &DEMO_POLICY) != 0) {
        fprintf(stderr, "Failed to initialize option parser.\n");
        return EXIT_FAILURE;
    }

    int rc = parse_opts(argc, argv);
    if (rc != 0) {
        fprintf(stderr, "Parse error (rc=%d). Try -h.\n", rc);
        destroy_opts();
        return EXIT_FAILURE;
    }

    const parse_result_t *pr = get_parse_result();

    int idx_help = find_opt('h');
    int idx_ver  = find_opt('v');

    if (idx_help >= 0 && pr->opt[idx_help].present && pr->opt[idx_help].b) {
        show_help(stdout, argv[0]);
        destroy_opts();
        return EXIT_SUCCESS;
    }
    if (idx_ver >= 0 && pr->opt[idx_ver].present && pr->opt[idx_ver].b) {
        show_version(stdout, argv[0]);
        destroy_opts();
        return EXIT_SUCCESS;
    }

    printf("\n=== Option Parser Demo ===\n\n");
    print_results(pr);
    destroy_opts();
    return EXIT_SUCCESS;
}
