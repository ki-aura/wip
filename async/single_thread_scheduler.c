// Include standard C libraries
#include <stdio.h>       // Standard I/O functions (like printf, fprintf)
#include <stdlib.h>      // Standard utility functions (like malloc, free)
#include <time.h>        // Time-related functions and structures (like struct timespec, clock_gettime, nanosleep)
#include <stdbool.h>     // Boolean type support (_Bool, bool)
#include <unistd.h>      // POSIX API (general utilities, not all functions used here)
#include <string.h>      // String manipulation functions (like strdup)
#include <errno.h>      // String manipulation functions (like strdup)
#include "gheap.h"       // External min-heap implementation header (assumed to provide heap_t, heap_push, etc.)
#include "termios_handler.h"


// External library compilation instruction (not C code, kept as a note)
///////////////////
/////// cc -o scheduler single_thread_scheduler.c gheap.c termios_handler.c 
///////////////////

// --- APPLICATION-SPECIFIC TYPEDEFS ---

// Defines the signature for all task functions.
// All tasks must accept:
// 1. scheduled time (what time it was supposed to run)
// 2. actual time (what time it actually ran)
// 3. context pointer (task-specific data)
typedef void (*task_func)(struct timespec *scheduled, struct timespec *actual, void *ctx);

// Structure for task-specific context data for generic wizard tasks.
// Holds a message to print and the time the task last ran, and whether it's a one-off task
typedef struct {
    char *message;
    struct timespec last_run_time;
} WizContext;

// Main structure representing a scheduled task.
typedef struct {
    long task_id;             // Unique identifier for the task (used for deletion)
    struct timespec next_run; // The time at which the task should next execute (priority key in heap)
    task_func func;           // Pointer to the function to execute
    void *ctx;                // Pointer to task-specific context data (e.g., WizContext)
    long interval_ms;         // Repeat interval in milliseconds
    bool one_off_task;		  // is this a one_off task
} Task;

// --- HEAP UTILITY FUNCTIONS ---

// Comparison function required by the min-heap implementation.
// It compares two Task pointers based on their 'next_run' time.
// Used to determine priority: earlier time means higher priority (smaller value).
static int task_compare(const void *a, const void *b) {
    const Task *tA = a;
    const Task *tB = b;
    // Primary sort key: seconds
    if (tA->next_run.tv_sec != tB->next_run.tv_sec)
        return tA->next_run.tv_sec - tB->next_run.tv_sec;
    // Secondary sort key: nanoseconds
    return tA->next_run.tv_nsec - tB->next_run.tv_nsec;
}

// Matching function required by the min-heap for finding and deleting items.
// Checks if a Task item's ID matches the given key (a task ID).
static bool task_match_id(const void *item, const void *key) {
    const Task *t = item;
    long id = *(const long*)key; // The key is a pointer to the task ID
    return t->task_id == id;
}

// --- TIME UTILITY FUNCTIONS ---

char now_str[64]; // Buffer to hold the formatted time string
char* realtime_now(void) {
    struct timespec now;
    struct tm local_time;
    time_t seconds;

    // 1. Get the real-time clock value
    if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
        fprintf(stderr, "Error calling clock_gettime: %s\n", strerror(errno));
        return NULL;
    }

    // 2. Extract seconds and convert to local calendar time
    seconds = now.tv_sec;
    
    // POSIX function: Thread-safe conversion to local time
    if (localtime_r(&seconds, &local_time) == NULL) {
        fprintf(stderr, "Error converting time to local time.\n");
        return NULL;
    }
    
    // 3. Format the time into a human-readable string
    // Format string: %H (Hour 24-hr), %M (Minute), %S (Second)
    if (strftime(now_str, sizeof(now_str), "%S", &local_time) == 0) {
        fprintf(stderr, "Error formatting time string.\n");
        return NULL;
    }

    return now_str;
    // If you wanted to show the date as well:
    // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &local_time);
    // printf("Current Local Date/Time: %s\n", time_str);
}

// Adds a given number of milliseconds to a timespec structure.
// Handles overflow from nanoseconds to seconds.
static void timespec_add_ms(struct timespec *t, long ms) {
    // Add seconds part
    t->tv_sec += ms / 1000;
    // Add nanoseconds part (ms * 1,000,000)
    t->tv_nsec += (ms % 1000) * 1000000;
    // Handle nanosecond overflow (if >= 1 second)
    if (t->tv_nsec >= 1000000000) {
        t->tv_sec++;
        t->tv_nsec -= 1000000000;
    }
}

// Calculates the time difference between two timespec structures (b - a).
// Returns the difference in seconds as a double, positive if b is later than a.
static double timespec_diff(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) + (b->tv_nsec - a->tv_nsec) / 1e9;
}

// Pauses the execution until the specified target time using POSIX's nanosleep.
static void sleep_until(const struct timespec *target) {
    struct timespec now, delta;
    // Get the current monotonic time (time since an arbitrary point, not subject to system clock changes)
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) return;
    
    // Calculate the time difference (target - now)
    delta.tv_sec = target->tv_sec - now.tv_sec;
    delta.tv_nsec = target->tv_nsec - now.tv_nsec;
    
    // Normalize delta: if nsec is negative, borrow from sec
    if (delta.tv_nsec < 0) { 
        delta.tv_sec--; 
        delta.tv_nsec += 1000000000; 
    }
    
    // If target time is in the past, return immediately
    if (delta.tv_sec < 0) return;
    
    // Sleep for the calculated delta time
    nanosleep(&delta, NULL);
}

// --- TASK CREATION & DESTRUCTION ---

// Allocates memory for a new Task and its context, initializes it, and pushes it onto the heap.
// Returns a pointer to the created Task on success, NULL on failure.
Task *add_task(long add_task_id, heap_t *heap, task_func func, bool is_one_off, long initial_delay_ms,
               long interval_ms, const char *msg) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) return NULL; // Get current time
    
    // Allocate memory for the Task structure
    Task *t = malloc(sizeof(Task));
    if (!t) return NULL;

    // Allocate and initialize WizContext if a message is provided
    WizContext *w_ctx = NULL;
    if (msg) {
        w_ctx = malloc(sizeof(WizContext));
        if (!w_ctx) { free(t); return NULL; } // Cleanup Task if context allocation fails
        w_ctx->message = strdup(msg); // Duplicate the message string
        w_ctx->last_run_time = (struct timespec){0, 0}; // Initialize last run time
    }

    // Initialize the Task fields
    t->task_id = add_task_id;
    t->func = func;
    t->ctx = w_ctx;
    t->interval_ms = interval_ms;
    t->next_run = now;
	t->one_off_task = is_one_off;
    
    // Calculate the initial run time by adding the delay
    timespec_add_ms(&t->next_run, initial_delay_ms);

    // Push the new task pointer onto the min-heap
    if (heap_push(heap, t) != 0) {
        // If heap_push fails, clean up the allocated memory
        if (t->ctx) { free(((WizContext*)t->ctx)->message); free(t->ctx); }
        free(t);
        return NULL;
    }
    return t; // Return the successfully created task
}

// Frees the memory associated with a Task structure and its context.
void destroy_task(Task *t) {
    // If context exists, free its message string and the context structure itself
    if (t->ctx) { free(((WizContext*)t->ctx)->message); free(t->ctx); }
    free(t); // Free the Task structure
}

// Iteratively pops all remaining tasks from the heap and destroys them, then destroys the heap structure.
void cleanup_tasks(heap_t *heap) {
    Task *t;
    // Pop and destroy tasks until the heap is empty
    while ((t = heap_pop(heap)) != NULL) destroy_task(t);
    heap_destroy(heap); // Destroy the heap structure itself
}

// Finds a task by ID, removes it from the heap, and destroys it.
// Returns true if the task was found and deleted, false otherwise.
bool delete_task_by_id(heap_t *heap, long task_id) {
    // Use the heap function to find, remove, and return the item matching the ID.
    // The match is determined by 'task_match_id'.
    Task *t = heap_find_and_pop(heap, &task_id);
    if (!t) return false; // Task not found
    destroy_task(t); // Task found, so destroy it
    return true;
}


// --- TASK FUNCTIONS ---

// A simple timer task that prints the scheduling delay (difference between scheduled and actual run time).
void task_timer(struct timespec *scheduled, struct timespec *actual, void *ctx){
    double diff = timespec_diff(scheduled, actual);
    printf("Timer running: Scheduled=%ld.%09ld, Actual=%ld.%09ld, Diff=%+.6f s\n",
           scheduled->tv_sec, scheduled->tv_nsec,
           actual->tv_sec, actual->tv_nsec,
           diff);
}

// A task that runs cyclically and changes its output message based on a static counter.
// It also calculates and prints the time since its last execution.
void task_snape(struct timespec *scheduled, struct timespec *actual, void *ctx){
    static int counter = 0;              // Static counter persists across calls
    static struct timespec last_run = {0, 0}; // Static time of last execution
    struct timespec now;
    
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) return;
    
    // Calculate time elapsed since the last run. Checks if last_run is initialized.
    double seconds_since_last_run = (last_run.tv_sec || last_run.tv_nsec) ? timespec_diff(&last_run, &now) : 0;
    
    // Print a different message based on the counter
    switch(counter){
        case 0: printf("                Snape(%.1f)\n", seconds_since_last_run); break;
        case 1: printf("                Snape(%.1f)\n", seconds_since_last_run); break;
        case 2: printf("                Severus Snape(%.1f)\n", seconds_since_last_run); break;
        default: break; // On case 3, nothing is printed before the reset
    }
    // Increment and wrap the counter (0, 1, 2, 3 -> 0, 1, 2, 3...)
    counter = (counter + 1) % 4;
    last_run = now; // Update the last run time
}

// A generic task that uses its context (WizContext) to print a message
// and track the time elapsed since its previous run.
void task_generic_wizard(struct timespec *scheduled, struct timespec *actual, void *ctx){
    WizContext *self_ctx = ctx; // Cast the void context pointer to the expected WizContext type
    struct timespec now = *actual; // Use the actual run time passed by the scheduler
    
    // Calculate time elapsed since the last run, stored in the context.
    double seconds_since_last_run = (self_ctx->last_run_time.tv_sec || self_ctx->last_run_time.tv_nsec)
                                     ? timespec_diff(&self_ctx->last_run_time, &now) : 0;
    
    // Print the context message and the time difference
    printf("(%.2f)(%s)%s\n", seconds_since_last_run, realtime_now(), self_ctx->message);
    
    self_ctx->last_run_time = now; // Update the context's last run time
}


// --- SCHEDULER (MAIN) ---
int main() {
    // 1. Initialize the min-heap, passing the comparison and matching functions.
    heap_t *heap = heap_create(task_compare, task_match_id);
    if (!heap) { fprintf(stderr, "Failed to init heap\n"); return 1; }
	
	init_termios();
	
    printf("Starting Scheduler...\n\n");
    // 2. Schedule initial tasks. task id's a random
    // add_task(ID, heap, func, initial_delay_ms, interval_ms, context_msg)
    // Note: Multiple tasks can have the same ID (e.g., ID 45), which is fine for the scheduler's
    // execution, but 'delete_task_by_id' will only delete *one* of them (whichever is found first).
    add_task(51, heap, task_generic_wizard, true, 0, 500, "There's something going on in the Library.....");
    add_task(22, heap, task_generic_wizard, false, 100, 500, "tick");
    add_task(32, heap, task_snape, false, 1000, 1000, "Snape Context");
    add_task(45, heap, task_generic_wizard, false, 3200, 4000, "			Dumbledore!");
    // ADD DUPLICATE ID!!! this won't fail in the heap code, but would be a serious error on 
    // this calling side. included so show random nature of delete below
    add_task(45, heap, task_generic_wizard, false, 3300, 4000, "			(DuplicateDore)"); 
    add_task(1 , heap, task_generic_wizard, false, 16000, 4000, "						Ron");
    add_task(2 , heap, task_generic_wizard, false, 17000, 4000, "						Ron...");
    add_task(6 , heap, task_generic_wizard, false, 18400, 4000, "						Ron WEEEEEEEEEASLEY");
    add_task(77, heap, task_generic_wizard, false, 20500, 2000, "											Hermione");
    add_task(9 , heap, task_generic_wizard, false, 28600, 250, "													Harry Potter...");

    int loop_count = 0;
    
    // 3. Main scheduler loop: runs as long as there are tasks in the heap.
    while (heap_size(heap) > 0) {

// DEMO LOGIC START    
        // DEMO: Dynamically delete a task at a specific loop iteration.
        // Since two tasks share ID 45, the first call to delete_task_by_id will remove one.
        // The one removed is implementation-dependent, based on which duplicate is found first in the heap.
        if (loop_count == 30) {
            printf("\n--- Deleting Random Dumbledore Task ---\n");
            delete_task_by_id(heap, 45);
        }
        
        // DEMO: Delete the remaining task with ID 45.
        if (loop_count == 50) {
            printf("\n--- Deleting Remaining Dumbledore Task ---\n");
            delete_task_by_id(heap, 45);
        }
        
        // DEMO: Dynamically add new tasks at a specific loop iteration.
        if (loop_count == 80) {
            printf("\n--- Adding Dumbledore Task back ---\n");
            add_task(45, heap, task_generic_wizard, false, 0, 4000, "			Dumbledore's BACK!");
            add_task(666, heap, task_generic_wizard, false, 6000, 8000, "<VOLDERMORT WITH THE PIPE BOMB!>");
        }
        // DEMO: Dynamically add new tasks at a specific loop iteration.
        if (loop_count == 10) {
        	// add a one off task
            printf("\n--- Adding one off Boo task ---\n");
            add_task(999, heap, task_generic_wizard, true, 0, 0, "    					  Boo!!!!!!!!!");
        }
// DEMO LOGIC END    

// STANDARD SCHEDULER FUNCTIONALITY
		
        // Get the highest-priority task (earliest run time) and remove it from the heap.
        Task *next = heap_pop(heap);
        if (!next) break; // Should not happen if heap_size > 0, but acts as a safeguard.

        // Store the time the task was *supposed* to run.
        struct timespec scheduled_for_this_run = next->next_run;
        
        // Wait until the scheduled time. This is the core blocking point of the single-threaded scheduler.
        sleep_until(&scheduled_for_this_run);

        // Get the actual time the task is starting execution.
        struct timespec actual;
        clock_gettime(CLOCK_MONOTONIC, &actual);

        // Execute the task function, passing scheduled time, actual time, and context.
        next->func(&scheduled_for_this_run, &actual, next->ctx);
        
        // Calculate the next scheduled run time by adding the task's interval.
		timespec_add_ms(&scheduled_for_this_run, next->interval_ms);
		next->next_run = scheduled_for_this_run; // Overwrite task's next_run with the new, non-drifting time  
    
        // Re-insert the task into the heap to be scheduled again.
        // This process maintains the "async" or non-blocking loop while handling recurring tasks.
        if(!next->one_off_task) heap_push(heap, next);

        // Limit the total number of loop iterations for the demo.
        if (++loop_count >= 300 || check_for_q()) break;
    }

    // 4. Cleanup and exit.
    cleanup_tasks(heap); // Destroy all remaining tasks and the heap structure.
    printf("\nScheduler finished.\n");
    return 0;
}