int main() {
    const char message[] = "Hello, World!\n";
    
    // System call numbers for macOS (ARM64)
    // write syscall is 0x2000004
    long syscall_num = 0x2000004;
    long fd = 1;  // stdout file descriptor
    long len = 14; // length of our message
    
    // Inline assembly to make the system call (ARM64)
    __asm__ volatile (
        "mov x16, %0\n"     // syscall number into x16
        "mov x0, %1\n"      // fd into x0
        "mov x1, %2\n"      // buffer into x1
        "mov x2, %3\n"      // length into x2
        "svc #0x80\n"       // supervisor call
        :
        : "r"(syscall_num), "r"(fd), "r"(message), "r"(len)
        : "x0", "x1", "x2", "x16", "memory"
    );
    
    return 0;
}