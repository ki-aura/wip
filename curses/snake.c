#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#define BOARD_W 20
#define BOARD_H 10
// Tied maximum capacity directly to the grid area to ensure the win condition is reachable
#define MAX_SNAKE (BOARD_W * BOARD_H)
#define MAX_TASKS 10

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

// Initialize food safely off-board to prevent uninitialized matrix writes
Point food = {-1, -1};

// Direction: 0=up,1=right,2=down,3=left
int dir = 1;
int next_dir = 1;

// Grid Matrix State for O(1) Lookups
typedef enum { EMPTY, SNAKE_SEGMENT, FOOD_CELL } CellType;
CellType board[BOARD_H][BOARD_W];

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
void draw_board() {
    clear();
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            // Optimized character rendering using mvaddch instead of mvprintw
            switch (board[y][x]) {
                case SNAKE_SEGMENT:
                    mvaddch(y, x, 'O');
                    break;
                case FOOD_CELL:
                    mvaddch(y, x, 'F');
                    break;
                default:
                    mvaddch(y, x, '.');
                    break;
            }
        }
    }
    refresh();
}

void spawn_food() {
    // Only wipe the old food cell if it resides within legal coordinates
    if (food.x >= 0 && food.x < BOARD_W && food.y >= 0 && food.y < BOARD_H) {
        board[food.y][food.x] = EMPTY;
    }

    // Win condition check now functions correctly with the adjusted MAX_SNAKE macro
    if (snake_len >= BOARD_W * BOARD_H) {
        endwin();
        printf("Victory! You successfully filled the entire board!\n");
        exit(0);
    }

    int x, y;
    do {
        x = rand() % BOARD_W;
        y = rand() % BOARD_H;
    } while (board[y][x] != EMPTY); // O(1) collision check

    food.x = x;
    food.y = y;
    board[y][x] = FOOD_CELL;
}

void move_snake() {
    dir = next_dir; 

    Point new_head = snake[0];
    switch (dir) {
        case 0: new_head.y--; break;
        case 1: new_head.x++; break;
        case 2: new_head.y++; break;
        case 3: new_head.x--; break;
    }

    // Wrap around edges
    if (new_head.x < 0) new_head.x = BOARD_W - 1;
    if (new_head.x >= BOARD_W) new_head.x = 0;
    if (new_head.y < 0) new_head.y = BOARD_H - 1;
    if (new_head.y >= BOARD_H) new_head.y = 0;

    // Check if food was consumed
    int eating = (new_head.x == food.x && new_head.y == food.y);
    
    // Explicit growth boundary evaluation (Fixes ghost segment matrix leak)
    int will_grow = (eating || grow_snake > 0) && (snake_len < MAX_SNAKE);

    // Self-collision tracking via the grid matrix
    if (board[new_head.y][new_head.x] == SNAKE_SEGMENT) {
        // Safe tail tip exemption logic only valid when not expanding
        int is_tail_tip = (new_head.x == snake[snake_len - 1].x && new_head.y == snake[snake_len - 1].y);
        
        if (will_grow || !is_tail_tip) {
            endwin();
            printf("Game Over! Snake collided with itself.\n");
            exit(0);
        }
    }

    // Clean old tail out if not physically expanding size this frame
    if (!will_grow) {
        board[snake[snake_len - 1].y][snake[snake_len - 1].x] = EMPTY;
    }

    // Shift body segments securely
    int shift_limit = (snake_len < MAX_SNAKE) ? snake_len : (MAX_SNAKE - 1);
    for (int i = shift_limit; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = new_head;
    board[new_head.y][new_head.x] = SNAKE_SEGMENT;

    if (eating) {
        grow_snake++; 
        spawn_food();
    }

    if (grow_snake > 0) {
        if (snake_len < MAX_SNAKE) {
            snake_len++;
        }
        grow_snake--; // Always consume counter down to match matrix state updates
    }

    draw_board();
}

void snake_grow() {
    // Only accumulate counter if beneath absolute max capacity limits
    if (snake_len + grow_snake < MAX_SNAKE) {
        grow_snake++;
    }
}

// ------------------- Main ---------------------------
int main() {
    // High-resolution seeding using nanoseconds to ensure unique runs
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand((unsigned int)(ts.tv_nsec ^ ts.tv_sec));

    initscr();
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    // Initialize blank board array state
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            board[y][x] = EMPTY;
        }
    }

    // Construct starting snake configuration
    for (int i = 0; i < snake_len; i++) {
        snake[i].x = 2 - i;
        snake[i].y = 0;
        board[snake[i].y][snake[i].x] = SNAKE_SEGMENT;
    }

    spawn_food();

    add_task(move_snake, 200);   // Move updates every 200ms
    add_task(snake_grow, 5000);  // Environmental passive growth every 5s

    // Guard against empty-scheduler execution crashes
    if (task_count == 0) {
        endwin();
        return 1;
    }

    while (1) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // Frame skipping approach to due tasks (prevents massive burst teleports)
        for (int i = 0; i < task_count; i++) {
            Task *t = &tasks[i];
            if (timespec_cmp(&now, &t->next_run) >= 0) {
                t->callback();
                timespec_add_ms(&t->next_run, t->interval_ms);
            }
        }

        // Process keyboard input
        int ch = getch();
        // Validate inputs against next_dir to cleanly resolve double-tap crashes
        switch(ch) {
            case KEY_UP:    if (next_dir != 2) next_dir = 0; break;
            case KEY_RIGHT: if (next_dir != 3) next_dir = 1; break;
            case KEY_DOWN:  if (next_dir != 0) next_dir = 2; break;
            case KEY_LEFT:  if (next_dir != 1) next_dir = 3; break;
            case 'q': endwin(); return 0;
        }

        // Dynamic Sleep Throttling Calculation
        clock_gettime(CLOCK_MONOTONIC, &now);

        struct timespec earliest = tasks[0].next_run;
        for (int i = 1; i < task_count; i++) {
            if (timespec_cmp(&tasks[i].next_run, &earliest) < 0) {
                earliest = tasks[i].next_run;
            }
        }

        struct timespec sleep_time;
        sleep_time.tv_sec = earliest.tv_sec - now.tv_sec;
        sleep_time.tv_nsec = earliest.tv_nsec - now.tv_nsec;

        // Normalize underflows in nanoseconds
        if (sleep_time.tv_nsec < 0) {
            sleep_time.tv_sec -= 1;
            sleep_time.tv_nsec += 1000000000;
        }

        // Only sleep if the next deadline resides in the future
        if (sleep_time.tv_sec > 0 || (sleep_time.tv_sec == 0 && sleep_time.tv_nsec > 0)) {
            nanosleep(&sleep_time, NULL);
        }
    }

    endwin();
    return 0;
}