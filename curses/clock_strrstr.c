#include <ncurses.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void handle_sigint(int sig) {
    (void)sig;
    endwin();   // restore terminal on Ctrl-C
    exit(EXIT_SUCCESS);
}

const char *strrstr(const char *haystack, const char *needle, bool case_insensitive) {
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);

    if (nlen == 0) return haystack;   // empty needle matches start
    if (nlen > hlen) return NULL;     // impossible to match

    const char *p = haystack + hlen - nlen; // start at last possible match

    do {
        int cmp = case_insensitive
                  ? strncasecmp(p, needle, nlen)
                  : strncmp(p, needle, nlen);
        if (cmp == 0)
            return p;
    } while (p-- > haystack); // safely stop after checking start

    return NULL;
}

int main(void) {
    // Catch Ctrl-C to cleanup ncurses
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    initscr();
    cbreak();
    noecho();
    curs_set(0);
	start_color();
	use_default_colors(); 
   	init_pair(1, COLOR_RED, -1);
   	init_pair(2, COLOR_BLUE, -1);


    struct timespec now_monotonic, next_tick;

    // Align next_tick to the next whole second
    clock_gettime(CLOCK_MONOTONIC, &now_monotonic);
    next_tick.tv_sec  = now_monotonic.tv_sec + 1;
    next_tick.tv_nsec = 0;

    while (1) {
        // Display wall-clock time
        time_t t_real;
        struct tm *tm_real;
        t_real = time(NULL);
        tm_real = localtime(&t_real);

        char buf[64];
        strftime(buf, sizeof(buf), "[%Y-%m-%d] %H:%M (Sec:%S)", tm_real);
		// print in bold
        wattron(stdscr, A_BOLD);
        mvprintw(0, 0, "%s", buf);
        wattroff(stdscr, A_BOLD);
 
        // blue square brackets
        wattron(stdscr, COLOR_PAIR(2));
		mvaddnstr(0,strlen(buf) - strlen(strrstr(buf,"[",1)), 
				strrstr(buf,"[",0),1);
		mvaddnstr(0,strlen(buf) - strlen(strrstr(buf,"]",1)), 
				strrstr(buf,"]",0),1);
        wattroff(stdscr, COLOR_PAIR(2));   
        
        // print seconds in red non-bold
        wattron(stdscr, COLOR_PAIR(1));
		mvprintw(0,strlen(buf) - strlen(strrstr(buf,"(sec:",1)), 
				"%s", strrstr(buf,"(Sec:",0));
        wattroff(stdscr, COLOR_PAIR(1));   
        refresh();

        // Sleep until next whole second using monotonic clock
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        long sec  = next_tick.tv_sec  - now.tv_sec;
        long nsec = next_tick.tv_nsec - now.tv_nsec;
        if (nsec < 0) { sec--; nsec += 1000000000; }
        if (sec < 0) { sec = 0; nsec = 0; }

        struct timespec sleep_time = { sec, nsec };
        nanosleep(&sleep_time, NULL);

        // Schedule next tick
        next_tick.tv_sec += 1;
    }

    endwin();
    return 0;
}
