#include <ncurses.h>
#include <locale.h>

int main() {
    setlocale(LC_ALL, ""); // Required for wide characters with ncurses

    // Initialize the ncurses screen
    initscr();
    cbreak();          // Line buffering disabled. Pass everything to the program
    noecho();          // Do not echo input characters to the screen
    keypad(stdscr, TRUE); // Enable special keys like F1, arrow keys, etc.

    printw("Press any key to see its decimal value.\n");
    printw("Press Ctrl+C to exit.\n\n");
    refresh();

    int ch;
    while ((ch = getch()) != 3) { // 3 is the decimal value for Ctrl+C
        printw("Decimal value: %d\n", ch);
        refresh();
    }

    // Clean up ncurses
    endwin();

    return 0;
}

