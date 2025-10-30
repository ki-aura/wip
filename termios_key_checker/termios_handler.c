#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

// Global storage for original terminal settings
struct termios saved_settings;

// Function to put the terminal into raw mode
void set_raw_mode() {
    struct termios new_settings;

    // 1. Get the current terminal settings
    tcgetattr(STDIN_FILENO, &saved_settings);

    // 2. Copy current settings to modify
    new_settings = saved_settings;

    // 3. Set the terminal to "raw" mode:
    //    - ICANON: Disable canonical mode (line buffering)
    //    - ECHO: Disable character echoing
    new_settings.c_lflag &= ~(ICANON | ECHO);

    // 4. Apply the new settings immediately
    //    TCSANOW means apply the change immediately
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
}

// Function to restore original terminal settings
void restore_terminal_mode() {
    // Restore the terminal to the settings saved in set_raw_mode()
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_settings);
}

// Function to check for key press ('q')
int check_for_q() {
    // Check if there is a character available in the standard input stream
    // This is a common idiom on POSIX for non-blocking read
    char c = 0;
    
    // Set VMIN=0 and VTIME=0 for non-blocking read (this is handled by the raw mode setup)
    // A simple, non-blocking check on a raw terminal can be done with select() or poll(), 
    // but for simple cases, a non-blocking read is often done by setting the file status flags 
    // (which is complex).

    // For a minimal example, we use the `read` function and assume the terminal 
    // is configured for non-blocking read in raw mode (c_cc[VMIN]=0, c_cc[VTIME]=0), 
    // or we'll simply perform a *blocking* read for a truly simple loop exit.
    //
    // Since you want to *break out* of a loop, a simple `getc()` will block your process.
    // To make it truly non-blocking, the best and simplest fix is to use the `poll()` function.
    
    // ----------------------------------------------------------------------------------
    // OPTION A: Non-Blocking Check using poll() (More Robust)
    // ----------------------------------------------------------------------------------
    #include <poll.h>
    
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    // Check for input with a timeout of 0ms (non-blocking)
    if (poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLIN)) {
        // Input is available, read the character
        read(STDIN_FILENO, &c, 1);
        if (c == 'q') {
            return 1; // 'q' pressed
        }
    }
    return 0; // No 'q' pressed or no key pressed
}

void init_termios(void){
	// Setup terminal on entry	
    set_raw_mode();
    // Ensure terminal is restored on exit, even if the program crashes
    atexit(restore_terminal_mode);

}
