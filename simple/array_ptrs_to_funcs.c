#include "mgstd.h"

// defining and passing arrays of pointers to functions around 
// also shows typedef and structs and enum

// define the constants for the math function numbers
// enum defaults = 0,1,2 etc but you could state MINUS = 7 etc if needed
typedef enum {
    plus,   // 0
    minus,  // 1
    times,  // 2
    number_of_maths_funcs  // 3 - used when we use a dynamically sized array later
} MathOp;

// this is the struct of what i want my maths terms to return
// i will be able to access each element using the . operation later
typedef struct {
	int result;
	const char *desc;
} op_return;

// this typedef defines func_array as an array or pointers to "calc" functions that take 
// 2 integers and return a structure of type op_return - i.e. our maths functions
// we'll call the calc function using the . operator 
typedef struct {
    op_return (*calc)(int,int);
} func_array;

// the math functions. The (op_return) is a compound literal that creates a temporary
// object of the correct structure for return. i.e. you don't need to write
// op_return x = {etc..}; return x;
op_return fPLUS(int a, int b){return (op_return){a+b,"addition"};}
op_return fMINUS(int a, int b){return (op_return){a-b,"subtraction"};}
op_return fTIMES(int a, int b){return (op_return){a*b,"multiplication"};}

// func just to show how to pass the array of pointers arround
// it could easily just take an int instead of MathOp, but this makes it clearer
void dosum(func_array mmath[], MathOp opt)
{
	switch(opt) {
	case minus: 
		printf("7-5 = %i\n", mmath[minus].calc(7,5).result);
		break;
	case times:
		printf("7*5 = %i\n", mmath[times].calc(7,5).result);
		break;
	default: 
		printf("something else\n");
		break;
	}
}


int main(int argc, char *argv[])
{
// set up pointer array
func_array mm[3];	// allocates mem on definition
func_array *zmm;	// we will point back to mm 
func_array *pmm;	// we will allocate mem later

// define which function each element of mm points to
mm[plus].calc = fPLUS; 
mm[minus].calc = fMINUS;
mm[times].calc = fTIMES;

// zmm is going to simply point to mm. no malloc needed as it's just a single pointer
zmm = mm;


// define pmm in same way as mm, but allocate memory first
// setting the size to one more that the number of functions so that we can add 
// an additional NULL at the end when iterating the array
pmm = malloc((number_of_maths_funcs + 1) * sizeof(func_array));

// point each element of pmm at the appropriate function
// the final one allows us to iterate below
pmm[plus].calc  = fPLUS;
pmm[minus].calc = fMINUS;
pmm[times].calc = fTIMES;
pmm[number_of_maths_funcs].calc = NULL;  


// mm, pmm & zmm can be used interchangably as they are 
// all pointers to the array of pointers to functions
// you can use the .result operator directly without defining an intermediate variable
printf("((7+3)*5)-8 = %i\n", 	
		pmm[minus].calc(				// outer sum -8
			mm[times].calc(				// mid sum *5
				zmm[plus].calc(7,3).result,	// inner sum 7+3
			5).result,
		8).result);


// demo pass function array to another func 
dosum(mm,minus);
dosum(pmm,times);
dosum(zmm,plus);


// cycle round the functions without knowning how many there are
// we will stop when the pointer to the calc function is NULL
op_return x; // use a temp variable of type op_return to make the code simpler
for (int i = 0; pmm[i].calc != NULL; i++){
	x = pmm[i].calc(7,11);
	printf("Dynamic call to function[%i]: 7 %s 11 = %i\n", i, x.desc, x.result);
}

// mm doesn't need to be freed as pre-allocated
// zmm doesn't as it's just a single pointer
free(pmm); pmm = NULL;
return 0;
}
