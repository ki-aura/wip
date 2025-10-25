#ifndef OPT_PASRSE_H
#define OPT_PASRSE_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* ------------------------------ Public API ------------------------------ */

typedef enum { OPT_BOOL, OPT_INT, OPT_FLOAT, OPT_STRING } opt_type_t;

// see example at bottom of populated structure
typedef struct {
    char        short_opt;       /* e.g. 'h' */
    const char *long_opt;        /* e.g. "help" or "" - must NOT be NULL */ 
    opt_type_t  type;            /* BOOL, INT, FLOAT, STRING */
    double      min_val;         /* numeric only; ignored if min_val > max_val */
    double      max_val;         /* numeric only; ignored if min_val > max_val */
    const char *help;            /* help text (may be NULL) */
    int         required;        /* if non-bool: must appear at least once? (0/1) */
    int         repeatable;      /* for STRING only; collect multiple occurrences (0/1) */
    const char *default_str;     /* default for STRING (NULL if none) */
    long long   default_int;     /* default for INT */
    double      default_flt;     /* default for FLOAT */
    int         default_bool;    /* default for BOOL (0/1) */
} opt_spec_t;

typedef struct {
    int   required;              /* at least one operand required? (0/1) */
    const char *default_if_none; /* injected when no operands (e.g., ".") */
    int   allow_stdin;           /* if 1 and stdin is a pipe, inject "-" as operand when none present */
} operand_policy_t;

typedef struct {
    struct OptVal {
        int present;        /* 1 if provided by user (not just default) */
        int count;          /* occurrences */
        int b;              /* BOOL */
        long long i;        /* INT */
        double f;           /* FLOAT */
        char *s;            /* STRING (owned if present) */
        char **slist;       /* repeatable strings (owned) */
        int slist_len;
    } *opt;                 /* array length = num_specs */
    int num_paths;          /* operands count */
    char **paths;           /* pointers into argv or synthesized constants; array owned */
    int stdin_is_pipe;      /* !isatty(STDIN_FILENO) */
} parse_result_t;

/* API */
int  set_opts(const opt_spec_t *specs, size_t num_specs, const operand_policy_t *policy);
int  parse_opts(int argc, char **argv);
const parse_result_t* get_parse_result(void);
void show_help(FILE *out, const char *progname);
void show_version(FILE *out, const char *progname); /* prints VERSION if defined */
void destroy_opts(void);
const char* type_name(opt_type_t t);

#endif // OPT_PASRSE_H