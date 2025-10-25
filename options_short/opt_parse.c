#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include "opt_parse.h"

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

typedef struct {
    const opt_spec_t     *specs;
    size_t                 n_specs;
    operand_policy_t       policy;
    parse_result_t         result;
    char                  *shorts;
} opts_ctx_t;

static opts_ctx_t G = {0};

static int is_stdin_pipe(void) { return isatty(STDIN_FILENO) ? 0 : 1; }
static int schema_index_by_short(char c) {
    for (size_t i = 0; i < G.n_specs; i++)
        if (G.specs[i].short_opt == c) return (int)i;
    return -1;
}

static int append_slist(struct OptVal *ov, const char *val) {
    char **p = realloc(ov->slist, (size_t)(ov->slist_len + 1) * sizeof(char*));
    if (!p) return -1;
    ov->slist = p;
    ov->slist[ov->slist_len] = strdup(val ? val : "");
    if (!ov->slist[ov->slist_len]) return -1;
    ov->slist_len++;
    return 0;
}

static void free_optval(struct OptVal *ov) {
    if (ov->s) free(ov->s);
    if (ov->slist) {
        for (int i = 0; i < ov->slist_len; i++) free(ov->slist[i]);
        free(ov->slist);
    }
}

static void init_defaults(struct OptVal *ov, const opt_spec_t *s) {
    switch (s->type) {
        case OPT_BOOL:  ov->b = s->default_bool; break;
        case OPT_INT:   ov->i = s->default_int; break;
        case OPT_FLOAT: ov->f = s->default_flt; break;
        case OPT_STRING:ov->s = s->default_str ? strdup(s->default_str) : NULL; break;
    }
}

static int parse_number(const opt_spec_t *s, const char *arg, struct OptVal *ov) {
    errno = 0;
    char *end = NULL;
    double v = (s->type == OPT_INT) ? strtoll(arg, &end, 10) : strtod(arg, &end);
    if (errno || end == arg || *end != '\0' ||
        (s->min_val <= s->max_val && (v < s->min_val || v > s->max_val)))
        return -1;
    if (s->type == OPT_INT) ov->i = (long long)v; else ov->f = v;
    return 0;
}

static int set_operands_from_single(const char *val) {
    G.result.paths = malloc(sizeof(char*));
    if (!G.result.paths) return -1;
    G.result.paths[0] = (char*)val;
    G.result.num_paths = 1;
    return 0;
}

int set_opts(const opt_spec_t *specs, size_t num_specs, const operand_policy_t *policy) {
    destroy_opts();

    G.specs  = specs;
    G.n_specs = num_specs;
    G.policy = *policy;
    G.result.opt = calloc(num_specs, sizeof(*G.result.opt));
    if (!G.result.opt) return -1;

    size_t cap = num_specs * 2 + 1;
    G.shorts = malloc(cap);
    if (!G.shorts) return -1;
    size_t pos = 0;
    for (size_t i = 0; i < num_specs; i++) {
        char so = specs[i].short_opt;
        if (so != '\0') {
            G.shorts[pos++] = so;
            if (specs[i].type != OPT_BOOL) G.shorts[pos++] = ':';
        }
    }
    G.shorts[pos] = '\0';

    for (size_t i = 0; i < num_specs; i++)
        init_defaults(&G.result.opt[i], &specs[i]);

    G.result.num_paths = 0;
    G.result.paths = NULL;
    G.result.stdin_is_pipe = is_stdin_pipe();
    optind = 1;
#if !defined(__GLIBC__)
    opterr = 0;
#endif
    return 0;
}

static int set_val_from_arg(size_t idx, const char *arg) {
    const opt_spec_t *s = &G.specs[idx];
    struct OptVal *ov   = &G.result.opt[idx];

    switch (s->type) {
        case OPT_BOOL: ov->b = 1; break;
        case OPT_INT:
        case OPT_FLOAT:
            if (!arg || parse_number(s, arg, ov) != 0) return -1;
            break;
        case OPT_STRING:
            if (!arg) return -1;
            if (s->repeatable) {
                if (append_slist(ov, arg) != 0) return -1;
            } else {
                free(ov->s);
                ov->s = strdup(arg);
                if (!ov->s) return -1;
            }
            break;
    }
    ov->present = 1;
    ov->count++;
    return 0;
}

static int is_help_or_version(const opt_spec_t *s) {
    return s->type == OPT_BOOL && (s->short_opt == 'h' || s->short_opt == 'v');
}

int parse_opts(int argc, char **argv) {
    int c, err = 0;
    while ((c = getopt(argc, argv, G.shorts)) != -1) {
        if (c == '?' || c == ':') return -1;
        int idx = schema_index_by_short((char)c);
        if (idx < 0) return -1;
        err = set_val_from_arg((size_t)idx, optarg);
        if (err) return err;
    }

    int rem = argc - optind;
    if (rem > 0) {
        G.result.paths = malloc((size_t)rem * sizeof(char*));
        if (!G.result.paths) return -1;
        for (int i = 0; i < rem; i++) G.result.paths[i] = argv[optind + i];
        G.result.num_paths = rem;
    } else if (G.policy.allow_stdin && G.result.stdin_is_pipe) {
        if (set_operands_from_single("-") != 0) return -1;
    } else if (G.policy.default_if_none) {
        if (set_operands_from_single(G.policy.default_if_none) != 0) return -1;
    }

    /* Skip required checks if help or version was requested */
    for (size_t j = 0; j < G.n_specs; j++) {
        if (is_help_or_version(&G.specs[j]) &&
            G.result.opt[j].present && G.result.opt[j].b)
            return 0;
    }

    /* Enforce required flags for non-bool options */
    for (size_t i = 0; i < G.n_specs; i++) {
        const opt_spec_t *s = &G.specs[i];
        if (s->required && s->type != OPT_BOOL && !G.result.opt[i].present)
            return -1;
    }

    /* Enforce required operands */
    if (G.policy.required && G.result.num_paths == 0)
        return -3;

    return 0;
}

const parse_result_t* get_parse_result(void) { return &G.result; }

static const char *type_names[] = { "bool", "int", "float", "string" };
const char* type_name(opt_type_t t) {
    return (t >= 0 && t <= OPT_STRING) ? type_names[t] : "?";
}

void destroy_opts(void) {
    if (G.result.opt) {
        for (size_t i = 0; i < G.n_specs; i++)
            free_optval(&G.result.opt[i]);
        free(G.result.opt);
    }
    free(G.result.paths);
    free(G.shorts);
    memset(&G, 0, sizeof(G));
}

static void print_range_default(FILE *out, const opt_spec_t *s) {
    char buf[128];
    size_t pos = 0;
    int n;

    if (s->type == OPT_INT && s->min_val <= s->max_val) {
        n = snprintf(buf + pos, sizeof(buf) - pos, " [%lld..%lld]", (long long)s->min_val, (long long)s->max_val);
        if (n > 0) pos += (size_t)n;
    } else if (s->type == OPT_FLOAT && s->min_val <= s->max_val) {
        n = snprintf(buf + pos, sizeof(buf) - pos, " [%.6g..%.6g]", s->min_val, s->max_val);
        if (n > 0) pos += (size_t)n;
    }

    n = snprintf(buf + pos, sizeof(buf) - pos, " (default: ");
    if (n > 0) pos += (size_t)n;

    switch (s->type) {
        case OPT_BOOL:
            n = snprintf(buf + pos, sizeof(buf) - pos, "%s", s->default_bool ? "true" : "false");
            break;
        case OPT_INT:
            n = snprintf(buf + pos, sizeof(buf) - pos, "%lld", s->default_int);
            break;
        case OPT_FLOAT:
            n = snprintf(buf + pos, sizeof(buf) - pos, "%.6g", s->default_flt);
            break;
        case OPT_STRING:
            n = snprintf(buf + pos, sizeof(buf) - pos, "%s", s->default_str ? s->default_str : "NULL");
            break;
    }
    if (n > 0) pos += (size_t)n;

    n = snprintf(buf + pos, sizeof(buf) - pos, ")%*s", pos < 30 ? (int)(30-pos) : 1, "");
    if (n > 0) pos += (size_t)n;

    fprintf(out, "%s", buf);
}


void show_help(FILE *out, const char *progname) {
    if (!out) out = stdout;
    if (!progname) progname = "prog";
    fprintf(out, "Usage: %s [OPTIONS] [--] [OPERANDS...]\n\n", progname);
    fprintf(out, "Options:\n");

    for (size_t i = 0; i < G.n_specs; i++) {
        const opt_spec_t *s = &G.specs[i];
		fprintf(out, "  -%c   ", s->short_opt);
        fprintf(out, "%-8s", type_name(s->type));
        print_range_default(out, s);
        if (s->help && *s->help)
            fprintf(out, "  - %s", s->help);
        if (s->repeatable && s->type == OPT_STRING)
            fprintf(out, " (repeatable)");
        if (s->required && s->type != OPT_BOOL)
            fprintf(out, " [required]");
        fputc('\n', out);
    }
    show_version(stdout, progname);
}

void show_version(FILE *out, const char *progname) {
    if (!out) out = stdout;
    fprintf(out, "\n%s version: %s\n", progname, DISPLAY_VERSION);
}
