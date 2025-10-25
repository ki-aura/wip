#include <ncurses.h>
#include <stdlib.h>

int main(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Enable all mouse events including motion
    mousemask(BUTTON1_PRESSED | BUTTON1_RELEASED | BUTTON1_CLICKED | REPORT_MOUSE_POSITION, NULL);
    nodelay(stdscr, TRUE); // non-blocking getch

    MEVENT ev;
    int ch;

    mvprintw(0,0,"Mouse debug program. Press 'q' to quit.");
    mvprintw(2,0,"Click, drag, and release. See ev.bstate output below:");

    while (1) {
        ch = getch();
        if (ch == 'q') break;

        if (ch == KEY_MOUSE) {
            if (getmouse(&ev) == OK) {
mvprintw(4,0,"Mouse event at y=%d x=%d bstate=0x%x   ",
         ev.y, ev.x, ev.bstate);

                clrtoeol();
            }
        }
        refresh();
    }

    endwin();
    return 0;
}
