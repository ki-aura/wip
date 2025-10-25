
char *join_args_one_malloc(int argc, char *argv[]);
char *join_args_multi_malloc(int argc, char *argv[]);

char *mem_cat(char *base, char* cat);
void mem_cat2(char **base, char* cat);

void mem_cat_TH(int argc, char *argv[]);
void mem_cat2_TH(int argc, char *argv[]);
