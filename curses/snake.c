#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define MAX_TASKS 10
#define BOARD_W 20
#define BOARD_H 10
#define MAX_SNAKE 100

typedef void (*task_cb)(void);

typedef struct {
    task_cb callback;
    int interval_ms;
    struct timespec next_run;
} Task;

Task tasks[MAX_TASKS];
int task_count = 0;

typedef struct {
    int x, y;
} Point;

Point snake[MAX_SNAKE];
int snake_len = 3;
int grow_snake = 0;

Point food;

// Direction: 0=up,1=right,2=down,3=left
int dir = 1;
int next_dir = 1;

// ------------------- Task system --------------------
void timespec_add_ms(struct timespec *t, int ms) {
    t->tv_sec += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000;
    if (t->tv_nsec >= 1000000000) {
        t->tv_sec += 1;
        t->tv_nsec -= 1000000000;
    }
}

int timespec_cmp(const struct timespec *a, const struct timespec *b) {
    if (a->tv_sec != b->tv_sec) return (a->tv_sec > b->tv_sec) ? 1 : -1;
    if (a->tv_nsec != b->tv_nsec) return (a->tv_nsec > b->tv_nsec) ? 1 : -1;
    return 0;
}

void add_task(task_cb cb, int interval_ms) {
    if (task_count >= MAX_TASKS) return;
    Task *t = &tasks[task_count++];
    t->callback = cb;
    t->interval_ms = interval_ms;
    clock_gettime(CLOCK_MONOTONIC, &t->next_run);
    timespec_add_ms(&t->next_run, interval_ms);
}

// ------------------- Snake tasks ---------------------
/*
Yes — that nested loop is O(BOARD_W × BOARD_H × snake_len), which is inefficient, especially as the snake grows.
A few ways to improve:
Draw only what's changed
Instead of redrawing the whole board every frame, update only the cells that changed (snake head, old tail, new food).
Complexity drops to O(changes) per frame.
Use a 2D array (board matrix)
Maintain a char board[BOARD_H][BOARD_W] where each cell stores EMPTY, SNAKE, FOOD.
Drawing becomes a simple O(BOARD_W × BOARD_H) pass without checking the snake array for each cell.
Updating snake positions involves updating the board array at the old tail and new head.
Use a hash / set for snake positions
Store snake positions in a hash_set (or bitmap if BOARD_W × BOARD_H is small).
Then checking if a cell contains a snake is O(1) instead of looping through the snake array.

*/
void draw_board() {
    clear();
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            int printed = 0;
            for (int i = 0; i < snake_len; i++) {
                if (snake[i].x == x && snake[i].y == y) {
                    mvprintw(y, x, "O");
                    printed = 1;
                    break;
                }
            }
            if (food.x == x && food.y == y) {
                mvprintw(y, x, "F");
                printed = 1;
            }
            if (!printed)
                mvprintw(y, x, ".");
        }
    }
    refresh();
}

void spawn_food() {
    int x, y;
    int collision;
    do {
        x = rand() % BOARD_W;
        y = rand() % BOARD_H;
        collision = 0;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == x && snake[i].y == y) {
                collision = 1;
                break;
            }
        }
    } while (collision);
    food.x = x;
    food.y = y;
}

void move_snake() {
    dir = next_dir; // apply queued direction change

    Point new_head = snake[0];
    switch (dir) {
        case 0: new_head.y--; break;
        case 1: new_head.x++; break;
        case 2: new_head.y++; break;
        case 3: new_head.x--; break;
    }

    // wrap around edges
    if (new_head.x < 0) new_head.x = BOARD_W - 1;
    if (new_head.x >= BOARD_W) new_head.x = 0;
    if (new_head.y < 0) new_head.y = BOARD_H - 1;
    if (new_head.y >= BOARD_H) new_head.y = 0;

    // check collision with self
    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == new_head.x && snake[i].y == new_head.y) {
            endwin();
            printf("Game Over! Snake collided with itself.\n");
            exit(0);
        }
    }

    // move body
    for (int i = snake_len; i > 0; i--) {
        snake[i] = snake[i-1];
    }
    snake[0] = new_head;

    // eat food
    if (new_head.x == food.x && new_head.y == food.y) {
        snake_len--;
        spawn_food();
    }

    if (grow_snake > 0) {
        snake_len++;
        grow_snake--;
    } else if (snake_len > MAX_SNAKE) {
        snake_len = MAX_SNAKE;
    }

    draw_board();
}

void snake_grow() {
    grow_snake++;
}

// ------------------- Main ---------------------------
int main() {
    srand(time(NULL));
    initscr();
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    // initial snake
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = 2 - i;
        snake[i].y = 0;
    }

    spawn_food();

    add_task(move_snake, 200);  // move every 200ms
    add_task(snake_grow, 5000); // grow every 5s
    add_task(spawn_food, 10000); // optional: respawn food periodically if desired

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        for (int i = 0; i < task_count; i++) {
            Task *t = &tasks[i];
            if (timespec_cmp(&now, &t->next_run) >= 0) {
                t->callback();
                timespec_add_ms(&t->next_run, t->interval_ms);
            }
        }

        // keyboard input
        int ch = getch();
        switch(ch) {
            case KEY_UP:    if (dir != 2) next_dir = 0; break;
            case KEY_RIGHT: if (dir != 3) next_dir = 1; break;
            case KEY_DOWN:  if (dir != 0) next_dir = 2; break;
            case KEY_LEFT:  if (dir != 1) next_dir = 3; break;
            case 'q': endwin(); return 0;
        }

        struct timespec sleep_time = { 0, 5000000 }; // 1 mil nano secs = 1 ms
        nanosleep(&sleep_time, NULL);

    }

    endwin();
    return 0;
}
