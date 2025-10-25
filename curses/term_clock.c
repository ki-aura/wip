#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <curses.h>
#include <term.h>

// --- Volatile Flags for Signal Handlers ---
// sig_atomic_t is the standard type for variables accessed by signal handlers.
// It guarantees that read/write operations are atomic (cannot be interrupted).
static volatile sig_atomic_t done = 0;          // Set to 1 by SIGINT (Ctrl-C) to exit.
static volatile sig_atomic_t tick = 0;          // Set to 1 by SIGALRM (timer) to redraw.
static volatile sig_atomic_t resize_pending = 0; // Set to 1 by SIGWINCH (resize) to redraw.

// --- Signal Handlers ---
static void sigalrm_handler(int signo) { (void)signo; tick = 1; }
static void sigwinch_handler(int signo) { (void)signo; resize_pending = 1; }
static void sigint_handler(int signo) { (void)signo; done = 1; } // Handler for Ctrl-C

// --- Draw Clock ---
// This function contains the critical terminal manipulation section.
static void draw_clock(void) {
    struct timeval tv;
    struct tm tm;
    char tbuf[20], dbuf[20];

    if (gettimeofday(&tv, NULL) == -1) return;
    localtime_r(&tv.tv_sec, &tm); // POSIX re-entrant function for thread safety
    strftime(dbuf, sizeof dbuf, "%d-%m-%Y", &tm);
    strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm);

    // Query current terminal width using POSIX ioctl (Input/Output Control)
    struct winsize ws;
    int cols = 80; // fallback
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0)
        cols = ws.ws_col;

    int row = 0;
    int dcol = cols - (int)strlen(dbuf) - 1;
    if (dcol < 0) dcol = 0;
    int tcol = cols - (int)strlen(tbuf) - 1;
    if (tcol < 0) tcol = 0;

    // Load necessary terminfo capabilities using tigetstr
    char *sc   = tigetstr("sc");	// save cursor
    char *rc   = tigetstr("rc"); // restore cursor
    char *vi   = tigetstr("vi"); // cursor Invisible (Hide cursor)
    char *ve   = tigetstr("ve"); // cursor Visible (Show cursor)
    char *el   = tigetstr("el"); // Erase to end of Line
    char *cup  = tigetstr("cup"); //Cursor Position (row, col). Handled via tparm.
    char *setaf = tigetstr("setaf"); // Set Foreground Color. Handled via tparm.
    char *bold = tigetstr("bold"); // Turn on Bold/Bright
    char *sgr0 = tigetstr("sgr0"); // Reset All Attributes (Color, Bold, etc.)

    // CRITICAL SECTION: Sending escape sequences to the terminal
    // This sequence MUST complete without interruption to leave the terminal state clean.
    if (sc) putp(sc);
    if (vi) putp(vi); // Hide cursor during update to prevent flicker
    if (bold) putp(bold); 
    if (setaf) putp(tparm(setaf, 4)); // Set foreground color (4 = Blue)
    if (cup) putp(tparm(cup, row, dcol)); // Move cursor to date position
    fputs(dbuf, stdout);
    if (setaf) putp(tparm(setaf, 1)); // Set foreground color (1 = Red)
    if (cup) putp(tparm(cup, row+1, tcol)); // Move cursor to time position
    fputs(tbuf, stdout);
    if (sgr0) putp(sgr0);	// Reset attributes (crucial to not leave color/bold active)
    if (el) putp(el);
    if (rc) putp(rc); // Restore cursor position
    if (ve) putp(ve); // Show cursor again

    fflush(stdout);
    // END CRITICAL SECTION
}

// --- Main ---
int main(void) {
    // Initialize terminfo library to read terminal capabilities
    if (setupterm(NULL, STDOUT_FILENO, NULL) == ERR) {
        fprintf(stderr, "setupterm failed\n");
        return 1;
    }

    // --- Signal Mask Setup ---
    sigset_t block_mask, old_mask;
    // Initialize an empty signal set
    sigemptyset(&block_mask);
    // Add SIGALRM and SIGWINCH to the set to be blocked
    sigaddset(&block_mask, SIGALRM);
    sigaddset(&block_mask, SIGWINCH);
    // Note: SIGINT is generally NOT blocked so that Ctrl-C can always exit quickly.

    // --- Setup signals ---
    // The simple signal() is POSIX-compliant for these tasks
    signal(SIGALRM, sigalrm_handler);
    signal(SIGWINCH, sigwinch_handler);
    signal(SIGINT, sigint_handler); 

    // --- Timer: 1 second ---
    struct itimerval it = {0};
    it.it_interval.tv_sec = 1; // Repeat every 1 second
    it.it_value.tv_sec = 1;    // First alarm in 1 second
    // Set a POSIX interval timer that sends SIGALRM
    setitimer(ITIMER_REAL, &it, NULL);

    // --- Main loop ---
    while (!done) { 
        // pause() suspends execution until a signal (SIGALRM, SIGWINCH, SIGINT) is received.
        // It prevents the program from spinning and consuming CPU.
        pause();

        // Check for immediate exit if SIGINT was received while paused
        if (done) break;

        // --- Critical Update Block Start ---
        if (tick || resize_pending) {
            
            // BLOCK: Use sigprocmask to block SIGALRM and SIGWINCH delivery.
            // SIG_BLOCK adds signals in block_mask to the blocked set.
            // old_mask saves the current signal mask, crucial for restoration.
            if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1) {
                perror("sigprocmask block");
            }

            // Check flags and clear them *before* the redraw
            if (resize_pending) {
                resize_pending = 0;
            }

            if (tick) {
                tick = 0;
            }
            
            // Perform the terminal update safely. No signal can interrupt this.
            draw_clock(); 

            // UNBLOCK: Restore the original signal mask.
            // SIG_SETMASK restores the signal set to what was saved in old_mask.
            // Any SIGALRM or SIGWINCH signals that arrived while blocked are delivered here.
            if (sigprocmask(SIG_SETMASK, &old_mask, NULL) == -1) {
                perror("sigprocmask unblock");
            }
        }
        // --- End Critical Update Block ---
    }
    
    // --- Cleanup and Exit ---
    // If the clock was the only thing hiding the cursor, it should now be visible.
    // If more complex terminal setup was used, cleanup would happen here.
    
    return 0;
}