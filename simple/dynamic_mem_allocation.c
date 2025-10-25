#include "mgstd.h"
#include "dynamic_mem_allocation.h"

/* 	
Run this command to test:
./hello fred $'\n'  mary \\n jim " " bob "it's no trick" trev $'it\'s a \n trick'
fred, mary, jim, bob & trev just delimit the real tests of:
NL char, escapped slash n, space, simple string, new line mid string
also run the no args ./hello
*/

// efficient single memory allocation
// handles all memory allocation in one go
char *join_args_one_malloc(int argc, char *argv[]) 
{

	size_t total_len = 0;
	
	// ignore argv[0] as we don't want prog name
	// +1 on total len to cater for space delimiter or final '\0'
	for (int i = 1; i < argc; i++) {
		total_len += strlen(argv[i]) + 1; 
	}
	
	if (total_len == 0) return NULL; // we didn't add any args
	
	char *result = malloc(total_len);
	result[0] = '\0';  // start empty string
	
	// build a single space delimited list
	// test argc -1 coz we don't want to add a space on the final one 
	for (int i = 1; i < argc; i++) {
	strcat(result, argv[i]);
	if (i < argc - 1) strcat(result, " ");
	}
	
	return result;
}

// inefficient multiple memory allocations, but shows method
// uses mem_cat
char *join_args_multi_malloc(int argc, char *argv[]) 
{

	// ignore argv[0] as we don't want prog name
	if (argc == 1) return NULL;		

	char *result = NULL;

	result = mem_cat(result, argv[1]);
	// do rest
	for (int i = 2; i < argc; i++) {
		result = mem_cat(result, " ");
		result = mem_cat(result, argv[i]);
	}
	return result;
}

// inefficient multiple memory allocations, but shows method
// uses mem_cat2
char *join_args_multi_malloc2(int argc, char *argv[]) 
{

	// ignore argv[0] as we don't want prog name
	if (argc == 1) return NULL;		

	char *result = NULL;

	mem_cat2(&result, argv[1]);
	// do rest
	for (int i = 2; i < argc; i++) {
		mem_cat2(&result, " ");
		mem_cat2(&result, argv[i]);
	}
	return result;
}

// helper function for string concat that handles memory allocation
// takes base and concatenates cat onto it with suitable checks
// WILL ERROR if base is read only memory e.g. char *x="hello" is passed in
char *mem_cat(char *base, char* cat)
{

	char *tmp = NULL;
	
	// cat could be null; if so return base
	if(!cat) {
		assert("nothing to cat"); 
		return base;
	}
	
	// base could be NULL so 2 options needed, deal with base NULL first
	if (!base){
		// create mem allocation, copy cat & return
		tmp = malloc(strlen(cat) + 1);
		if (!tmp) return NULL;
		strcpy(tmp, cat);
		//no memory to free as base was NULL
		assert("base was null, memory allocated");
		return tmp;
	
	} else { // base contains a string

		// create new memory allocation for the combined string
		tmp = malloc(strlen(base) + strlen(cat) + 1);
		if (!tmp) return NULL;
		
		//join them
		strcpy(tmp, base);
		strcat(tmp, cat);
		
		//free memory from the orignal base string
		free(base);
		base = NULL;
		return tmp;
	}
}

// helper function for string concat that handles memory allocation
// takes base and concatenates cat onto it with suitable checks
// this one directly manipulates base
void mem_cat2(char **base, char* cat)
{

	char *tmp = NULL;
	
	// cat could be null; if so nothing to do
	if(!cat) {
		assert("nothing to cat"); 
		return;
	}
	
	// base could be NULL so 2 options needed, deal with base NULL first
	if (!*base){
		// create mem allocation, copy cat & return
		*base = malloc(strlen(cat) + 1);
		if (!base) return;
		strcpy(*base, cat);
		//no memory to free as base was NULL
		assert("base was null, memory allocated");
		return;
	
	} else { // base contains a string
		// create new memory allocation for the combined string
		tmp = malloc(strlen(*base) + strlen(cat) + 1);
		if (!tmp) return;
		
		//join them
		strcpy(tmp, *base);
		strcat(tmp, cat);
		
		//free memory from the orignal base string and 
		free(*base);
		*base = tmp;
	}
}

// allocate memory to a NULL variable and set it to a char
char *mem_set(char *targ, const char* cop)
{
	if(targ){
		assert("targ not NULL"); 
		return NULL;
	}
	
	if(!cop){
		assert("cop is NULL"); 
		return NULL;
	}
	
	// assert("got this far");
	targ = malloc(strlen(cop)+1);
	strcpy(targ, cop);
	return targ;
}

// test harness for mem_cat2 
void mem_cat2_TH(int argc, char *argv[])
{
printf("Testing mem_cat2\n\n");
	char *b, *c;	// base and cat
	char *d, *e;	// NULL variables

	// test mem_set
	b = strdup("shouldn't be allocated");
	b = mem_set(b, "fail test");
	free(b); b = NULL;
	
	b = NULL;
	d = NULL;
	b = mem_set(b,d);
	printf("\n");

	// test mem_cat2 
	b = NULL;
	b = mem_set(b, "base1");
	c = NULL;
	c = mem_set(c, "cat1");
	mem_cat2(&b,c);
	printf ("1. both exist: %s\n\n", b);
	free(b); b = NULL;
	free(c); c = NULL;
	
	d = NULL;
	c = mem_set(c, "cat2");	
	mem_cat2(&d,c);
	printf ("2. base is null: %s\n\n", d);	
	free(d); d = NULL;
	free(c); c = NULL;

	b = mem_set(b, "base3");	
	d = NULL;
	mem_cat2(&b,d);
	printf ("3. cat is null: %s\n\n", b);
	free(b); b = NULL;
	free(d); d = NULL;

	d = NULL;
	e = NULL;	
	mem_cat2(&d,e);
	printf ("4. base and cat are null\n\n");
}

// test harness for mem_cat 
void mem_cat_TH(int argc, char *argv[])
{
printf("Testing mem_cat\n\n");
	char *b, *c;	// base and cat
	char *d, *e;	// NULL variables

	// test mem_set
	b = strdup("shouldn't be allocated");
	b = mem_set(b, "fail test");
	free(b); b = NULL;
	
	b = NULL;
	d = NULL;
	b = mem_set(b,d);
	printf("\n");

	// test mem_cat 
	b = NULL;
	b = mem_set(b, "base1");
	c = NULL;
	c = mem_set(c, "cat1");
	b = mem_cat(b,c);
	printf ("1. both exist: %s\n\n", b);
	free(b); b = NULL;
	free(c); c = NULL;
	
	d = NULL;
	c = mem_set(c, "cat2");	
	d = mem_cat(d,c);
	printf ("2. base is null: %s\n\n", d);	
	free(c); c = NULL;
	free(d); d = NULL;

	b = mem_set(b, "base3");	
	d = NULL;
	b = mem_cat(b,d);
	printf ("3. cat is null: %s\n\n", b);
	free(b); b = NULL;
	free(d); d = NULL;

	d = NULL;
	e = NULL;	
	d = mem_cat(d,e);
	printf ("4. base and cat are null\n\n");
}

int main(int argc, char *argv[])
{
	printf ("I am %s\n", argv[0]);

	// run test harnesses
	mem_cat_TH(argc, argv);
	mem_cat2_TH(argc, argv);

	// simplistic loop - doesn't understand new line chars. uses argv as array
	for (int n = 1; n < argc; n++) {
		printf ("%02d hello %s\n", n, argv[n]);
	}

	// this loop checks for new lines and doesn't add extra NLs when it finds them
	bool nl_needed;
	for (int n = 1; n < argc; n++) {
		// start by assuming there's NOT a NL char at the end of the string
		nl_needed = TRUE; 
		
		printf ("%02d howdy ", n);

		// loop char by char through the argument *p will be null at end of string and loop will end
		for (char *p = argv[n]; *p; p++) {

			// check if a \n has been passed as opposed to a sigle NL char
			// if so, then we don't need a NL after we've finished this arg
			if (*p == '\\' && *(p + 1) == 'n') {	
				printf ("[escaped slash n]");
				putchar('\n');
				p++; 			// skip past the 'n' in slash n
				nl_needed = FALSE; 

			// check if a real NL char has been passed
			} else if (*p == '\n') {		
				printf ("[NL char]");
            			putchar(*p);
            			nl_needed = FALSE;  		

			// otherwise just print the char
			} else {			
				putchar(*p);
				// set nl_needed back to true in case the NL was part way through a word
				nl_needed = TRUE;
			}
		}
		// we only need a new line if we've not already just printed it
		if (nl_needed) putchar('\n');	
	}

	// concat all args to a single string
	// join_args... returns NULL if there weren't any
	char *command_line = join_args_one_malloc(argc, argv);
	if (command_line) {
		printf("Single args: \'%s\'\n", command_line);
		free(command_line);
		command_line = NULL;
	} else {printf("No command line args\n");}

	// different version of same thing (diff memory allocation)
	// command_line memory has been freed and command_line set back to NULL above
	command_line = join_args_multi_malloc(argc, argv);
	if (command_line) {
		printf("Multi1 args: \'%s\'\n", command_line);
		free(command_line);
		command_line = NULL;
	} else {printf("No command line args\n");}

	// another different version of same thing  (uses mem_cat2)
	// command_line memory has been freed and command_line set back to NULL above
	command_line = join_args_multi_malloc2(argc, argv);
	if (command_line) {
		printf("Multi2 args: \'%s\'\n", command_line);
		free(command_line);
		command_line = NULL;
	} else {printf("No command line args\n");}

	return 0;
}


