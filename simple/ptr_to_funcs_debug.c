#include <stdio.h>

// three simple functions
int f0(int x) { return x + 10; }
int f1(int x) { return x + 20; }
int f2(int x) { return x + 30; }

void dmm_debug(int (**arr)(int)) {
    int arg = 5;

    printf("Function return values:\n");
    printf("arr[0] = %d\n", arr[0](arg));
    printf("arr[1] = %d\n", arr[1](arg));
    printf("arr[2] = %d\n", arr[2](arg));

    printf("\nPointer arithmetic:\n");
    printf("arr      = %p\n", (void*)arr);
    printf("arr + 1  = %p\n", (void*)(arr + 1));
    printf("arr + 2  = %p\n", (void*)(arr + 2));
    printf("arr + 3  = %p  <-- one past the array\n", (void*)(arr + 3));

    // Accessing memory past the original array
    printf("\nAccessing arr[3] (past original array):\n");
    printf("arr[3] = %p\n", (void*)arr[3]);  // undefined behavior for &mm
    
    printf("sizeof(arr)  = %zu\n", sizeof(arr));   // size of the parameter itself (pointer to function pointer)
printf("sizeof(*arr) = %zu\n", sizeof(*arr));  // size of what arr points to (a single function pointer)

    char *byte_ptr = (char*)arr;           // treat arr as raw bytes
    char *next_ptr = byte_ptr + sizeof(arr); // add size of pointer variable (sizeof(arr) = 8)
    
    printf("byte_ptr      = %p\n", byte_ptr);
    printf("next_ptr      = %p  <-- computed manually\n", next_ptr);

}

// debug version of dmm
void ddmm_debug(int (**arr)(int)) {
    int arg = 5;

    printf("Function return values:\n");
    printf("arr[0] = %d\n", arr[0](arg));
    printf("arr[1] = %d\n", arr[1](arg));
    printf("arr[2] = %d\n", arr[2](arg));

    printf("\nPointer arithmetic:\n");
    printf("arr      = %p\n", (void*)arr);
    printf("arr + 1  = %p\n", (void*)(arr + 1));
    printf("arr + 2  = %p\n", (void*)(arr + 2));
printf("arr + 3  = %p  <-- one past the array\n", (void*)(arr + 3));

    printf("\nSizes:\n");
    printf("sizeof(arr) = %zu (size of pointer to function pointer)\n", sizeof(arr));
    printf("sizeof(*arr) = %zu (size of function pointer)\n", sizeof(*arr));
}

int main(void) {
    // array of 3 function pointers
    int (*mm[3])(int) = { f0, f1, f2 };

    printf("Calling with mm:\n");
    dmm_debug(mm);      // ✅ correct

    printf("\nCalling with &mm:\n");
    dmm_debug(&mm);     // ⚠️ undefined behavior
}
