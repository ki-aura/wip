#include <sys/resource.h>
#include <stdio.h>



// this code will fail with a stack segmentation fault unless you set the soft limit 
// to something above 20mb before running using ulimit -s 20480 (20mb)
// HOWEVER!! 20mb exact (20480) fails, but 20500 works, so something larger is necessary for 
// the rest of the runtime environment 

#define REQUIRED_STACK_SIZE (20 * 1024 * 1024) // 20 MB in bytes


int main(void) {
    // Now, your code that requires a large stack can run here
    char big_buffer[REQUIRED_STACK_SIZE]; 
    big_buffer[0] = 'A'; // Access the memory to test it
    printf("Successfully allocated and accessed a 20MB local buffer.\n");

    return 0;
}