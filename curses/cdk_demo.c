#include <cdk.h>
#include <string.h>
#include <unistd.h> // For sleep()
#include <signal.h> // for window resize

/////////////////////////////////////////////////////////////////////////////////
// compile using
// gcc -o cdk_demo cdk_demo.c -I/opt/homebrew/opt/cdk/include -L/opt/homebrew/opt/cdk/lib -lcdk -lncurses
//
// note - homebrew version of CDK does not support mouse. 
/////////////////////////////////////////////////////////////////////////////////

// SIGWINCH handler
volatile sig_atomic_t resize_flag = 0; // global flag

void handle_resize(int sig) {
    if (sig == SIGWINCH) {
        resize_flag = 1;
    }
}

// Utility function to display a temporary message at the top of the screen
void show_message(CDKSCREEN *cdkscreen, const char *message) {
    CDKLABEL *label = NULL;
    char *lines[1];
    lines[0] = (char *)message;

    // Create a label (pop-up style message box)
    label = newCDKLabel(cdkscreen, CENTER, TOP, lines, 1, 
                        TRUE, FALSE); 

    if (label != NULL) {
        drawCDKLabel(label, FALSE);
        refreshCDKScreen(cdkscreen);
        sleep(2); // Keep the message on screen for 2 seconds
        destroyCDKLabel(label);
        refreshCDKScreen(cdkscreen);
    }
}

// Function to handle the Dialog widget (Pop-up Message Box)
void do_dialog(CDKSCREEN *cdkscreen) {
    CDKDIALOG *dialog = NULL;
    char *message[] = {"<C></B>CDK Dialog Example", 
                       "", 
                       "This is a simple pop-up message box.", 
                       "Press <C>OK to continue."};
    char *buttons[] = {"<C>OK", "<C>Cancel"};
    int selection;

    // The arguments for newCDKDialog were correct (10 arguments)
    dialog = newCDKDialog (cdkscreen, CENTER, CENTER, message, 4, buttons, 2, 
                           A_REVERSE, TRUE, TRUE, FALSE); 

    if (dialog == NULL) {
        return;
    }

    selection = activateCDKDialog(dialog, NULL);
    destroyCDKDialog(dialog);

    if (selection == 0) {
        show_message(cdkscreen, "<C></B/1>You pressed OK.");
    } else {
        show_message(cdkscreen, "<C></B/1>You pressed Cancel or Escape.");
    }
}

// Function to handle the Entry widget (Single Text Line Entry)
void do_entry(CDKSCREEN *cdkscreen) {
    CDKENTRY *entry = NULL;
    char *title = "<C></B>CDK Entry Widget (Username)";
    char *label = "<R>Username: ";
    char *result = NULL;
    char message[256];

    // Arguments for newCDKEntry were correct (12 arguments)
    entry = newCDKEntry(cdkscreen, CENTER, CENTER, title, label, 
                        A_BOLD, '.', vMIXED, 40, 0, 100, TRUE, FALSE); 

    if (entry == NULL) {
        return;
    }
	
	curs_set(1);
	echo();
    result = activateCDKEntry(entry, NULL);
	curs_set(0);
    noecho();
	
    if (result != NULL) {
        snprintf(message, sizeof(message), "<C></B/1>Entered Username: %s", result);
        show_message(cdkscreen, message);
    } else {
        show_message(cdkscreen, "<C></B/1>Entry was cancelled (Escape key).");
    }

    destroyCDKEntry(entry);
}

// Function to handle the Radio widget (Radio Button List)
void do_radio(CDKSCREEN *cdkscreen) {
    CDKRADIO *radio = NULL;
    char *title = "<C></B>CDK Radio List (Select an OS)";
    
    // Increased the number of options (5 total) and added one more.
    char *items[] = {"<C>Linux", "<C>macOS", "<C>Windows", "<C>FreeBSD", "<C>Solaris"};
    int item_count = 5;
    int menu_height_without_items = 3; //box = 2 + title
    int selection;
    char message[256];

    // Increased height to 5 to accommodate all 5 items.
    // FIX: Re-added the missing 'spos' (Scroll Position) argument with a value of 0.
    radio = newCDKRadio(cdkscreen, CENTER, CENTER, 
                        0, item_count + menu_height_without_items, 30, title, items, item_count, 
                        (chtype)'*', 1, A_REVERSE, TRUE, FALSE); 
                        // xpos, ypos, spos(0), height, width(30), title, mesg, items(5), choiceChar, defItem, highlight, Box, Shadow
    
    if (radio == NULL) {
        return;
    }

    // Set initial selection to "macOS" (index 1)
    setCDKRadioCurrentItem(radio, 1); 

    selection = activateCDKRadio(radio, NULL);

    if (selection >= 0) {
        snprintf(message, sizeof(message), "<C></B/1>You selected: %s", items[selection]);
        show_message(cdkscreen, message);
    } else {
        show_message(cdkscreen, "<C></B/1>Radio selection was cancelled (Escape key).");
    }

    destroyCDKRadio(radio);
}

// Function to handle the multi-select Selection List widget
void do_selection(CDKSCREEN *cdkscreen) {
    CDKSELECTION *selection_list = NULL;
    // Changed title to reflect multi-select functionality
    char *title = "<C></B>CDK Selection List (Select One or More OSs)";
    
    // Items for the list (5 total)
    char *items[] = {"<C>Linux", "<C>macOS", "<C>Windows", "<C>FreeBSD", "<C>Solaris"};
    int item_count = 5;
    
    // The 'choices' array defines the *display* of the selection marker for each item.
    // We want a simple on/off marker for each item.
    // The number of characters in this array determines the maximum value of the result array.
    char *choices[] = {"<C> ( )", "<C> (X)"}; // The available selections: Unselected (0) and Selected (1)
    int choice_count = 2; // Selection values will be 0 or 1
    
    int menu_height_without_items = 3; // Box = 2 + title
    int *selections = NULL;
    int default_selections[item_count]; // Array to hold initial selection state (0 or 1)
    char message[512];
    char result_string[512] = ""; // To build the output message

    // --- 1. Set up Initial/Default Selections ---
    // Multi-select lists require an initial state array. 
    // We'll set the first item (Linux) and the last item (Solaris) as selected (1).
    // All others are unselected (0).
    default_selections[0] = 1; // Linux (Selected)
    default_selections[1] = 0; // macOS (Unselected)
    default_selections[2] = 0; // Windows (Unselected)
    default_selections[3] = 0; // FreeBSD (Unselected)
    default_selections[4] = 1; // Solaris (Selected)

    // --- 2. Create the CDKSELECTION Widget ---
    selection_list = newCDKSelection(cdkscreen, 
                                     CENTER, CENTER,    // xpos, ypos (Centered)
                                     0,                  // spos (Scroll position, 0)
                                     item_count + menu_height_without_items, 
                                     35,                 // width (Increased slightly for choices display)
                                     title,              // Title
                                     items,              // The list of selectable items
                                     item_count,         // Number of items
                                     choices,            // The selection display choices (" ( )" or " (X)")
                                     choice_count,       // Number of choices (2: selected/unselected)
                                     A_REVERSE,          // Highlight attribute
                                     TRUE,               // Draw Box
                                     FALSE);             // Draw Shadow
    
    if (selection_list == NULL) {
        return;
    }

    // --- 3. Set Initial Selections ---
    // The highlight (A_REVERSE) is already set in the newCDKSelection call.
    // setCDKSelection is used to set the initial state array.
    setCDKSelection(selection_list, A_REVERSE, default_selections, TRUE);
    
    // --- 4. Activate the Widget ---
    // The activation returns the exit key, but the results are stored internally.
    activateCDKSelection(selection_list, NULL);

    // Get the final selections after the user exits.
    // This returns a pointer to an internal array of integers (0 or 1)
    selections = getCDKSelectionChoices(selection_list);

    // --- 5. Process and Display Results ---
    if (selection_list->exitType == vNORMAL) {
        
        // Build the result string showing which items were selected (where array value is 1)
        for (int i = 0; i < item_count; i++) {
            if (selections[i] == 1) {
                // items[i] already contains the formatted string (e.g., "<C>Linux")
                strcat(result_string, items[i]); 
                strcat(result_string, " ");
            }
        }
        
        if (strlen(result_string) > 0) {
            // Remove the trailing space and show the message
            result_string[strlen(result_string) - 1] = '\0'; 
            snprintf(message, sizeof(message), "<C></B/1>You selected: %s", result_string);
        } else {
            snprintf(message, sizeof(message), "<C></B/1>You selected no operating systems.");
        }
        
        // Assuming you have a function called show_message, as used in your template
        show_message(cdkscreen, message);
        
    } else {
        // Handle Escape or other non-normal exits
        show_message(cdkscreen, "<C></B/1>Selection was cancelled (Escape key).");
    }

    // --- 6. Clean Up ---
    destroyCDKSelection(selection_list);
}

void reset_screen_and_widgets(CDKSCREEN **cdkscreen_ptr, CDKBUTTONBOX **buttonbox_ptr, char **buttons) {
    // 1. Destroy the old screen and widgets
    destroyCDKButtonbox(*buttonbox_ptr);
    destroyCDKScreen(*cdkscreen_ptr);
    
    // 2. Tear down curses and re-initialize it
    endCDK(); 
    
    // 3. Re-initialize the Curses Development Kit
    *cdkscreen_ptr = initCDKScreen(initscr());
    
    // Re-check for null, though usually unnecessary after successful start
    if (*cdkscreen_ptr == NULL) {
        // Handle error: Fatal exit or return to shell
    }
    
    curs_set(0); 
    keypad(stdscr, TRUE);
    
    // 4. Re-create the buttonbox using the new COLS value
    // NOTE: We assume 'COLS' is now updated by the curses re-initialization
    *buttonbox_ptr = newCDKButtonbox (*cdkscreen_ptr, CENTER, BOTTOM,
                                     1, COLS-2, 
                                     NULL, 1, 5, 
                                     buttons, 5, A_REVERSE, TRUE, FALSE);
    
    drawCDKButtonbox(*buttonbox_ptr, FALSE);
    refreshCDKScreen(*cdkscreen_ptr);
}

// Main function
int main (void) {
    CDKSCREEN *cdkscreen = NULL;
    char *buttons[] = {"<C>Dialog", "<C>Entry", "<C>Radio", "<C>Selection", "<C>Exit"};
    CDKBUTTONBOX *buttonbox = NULL;
    int selection = 0;

    // 1. Initialize the Curses Development Kit
    cdkscreen = initCDKScreen (initscr());
    if (cdkscreen == NULL) {
        endCDK();
        return 1;
    }
   
    cbreak();
    noecho();
    curs_set(0); 
    keypad(stdscr, TRUE);
    
	//1b. create sigwinch handler
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_resize;
	sigaction(SIGWINCH, &sa, NULL);


    // 2. Create the main selection menu (Button Box) at the bottom
    // FIX: Re-added the missing 'rows' (1) and 'cols' (5) arguments.
    buttonbox = newCDKButtonbox (cdkscreen, CENTER, BOTTOM, 
                                 1, COLS-2, // height, width
                                 NULL, 1, 5, // title(NULL), rows(1), cols(5)
                                 buttons, 5, A_REVERSE, TRUE, FALSE); 
                                 // buttons, buttonCount, highlight, box, shadow
    
    if (buttonbox == NULL) {
        endCDK();
        return 1;
    }
    
    drawCDKButtonbox(buttonbox, FALSE);

    // 3. Main event loop
    while (selection != 4) { // 4 is the index for "Exit"

		// first Check for the resize flag at the start of the loop
		if (resize_flag) {
			resize_flag = 0; // Reset the flag
			reset_screen_and_widgets(&cdkscreen, &buttonbox, buttons);
			
			// After resize, the buttonbox needs to be activated again
			drawCDKButtonbox(buttonbox, FALSE); 
			refreshCDKScreen(cdkscreen);
			continue; // Restart the loop iteration
		}

        selection = activateCDKButtonbox(buttonbox, NULL);

        if (selection == 0) {
            do_dialog(cdkscreen);
        } else if (selection == 1) {
            do_entry(cdkscreen);
        } else if (selection == 2) {
            do_radio(cdkscreen);
        } else if (selection == 3) {
            do_selection(cdkscreen);
        }

        refreshCDKScreen(cdkscreen);
    }

    // 4. Clean up and exit
    destroyCDKButtonbox(buttonbox);
    destroyCDKScreen(cdkscreen);
    endCDK(); 

    return 0;
}