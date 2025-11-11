import curses
import sys
import os
from editor_state import load_file, save_file, EditorState
from tui_handler import init_tui, draw_screen, position_cursor, handle_keypress

def quit_prompt(stdscr: 'curses._CursesWindow', state: EditorState) -> bool:
    """
    Displays a prompt asking for confirmation to quit if the file is dirty.
    Returns True if the user confirms quitting, False otherwise.
    """
    if not state.is_dirty:
        return True # File is clean, safe to quit

    max_y, max_x = stdscr.getmaxyx()
    status_y = max_y - 1
    
    # 1. Draw the prompt message
    prompt_msg = " WARNING: Unsaved changes! Quit anyway? (y/N) "
    
    # Use the dirty color pair (4) for emphasis
    prompt_color = curses.color_pair(4) | curses.A_BOLD
    
    # Ensure the status line is updated with the message
    stdscr.addstr(status_y, 0, prompt_msg, prompt_color)
    stdscr.chgat(status_y, len(prompt_msg), max_x - len(prompt_msg), prompt_color)
    stdscr.refresh()

    # CRITICAL FIX: Temporarily set timeout to -1 (blocking) so getch waits indefinitely.
    stdscr.timeout(-1) 

    # 2. Wait for confirmation key
    key = stdscr.getch()
    
    # 3. Redraw the screen to clear the prompt, regardless of outcome
    # The main loop will automatically restore the stdscr.timeout(100) on the next iteration.
    draw_screen(stdscr, state) 
    position_cursor(stdscr, state)
    stdscr.refresh()
    
    # 4. Check for confirmation (y or Y)
    if key in (ord('y'), ord('Y')):
        return True
    
    return False


def main(stdscr: 'curses._CursesWindow', filepath: str) -> None:
    """
    The main application loop for the hex editor.

    Args:
        stdscr: The curses window object.
        filepath: The path to the file to be edited.
    """
    # 1. Initialization
    init_tui(stdscr)
    state = load_file(filepath)
    
    # Initial draw
    draw_screen(stdscr, state)
    position_cursor(stdscr, state)
    stdscr.refresh()
    
    # 2. Main Loop
    while True:
        try:
            # 2.1 Handle Input
            # Set a timeout for getch() so we can redraw occasionally (if needed)
            stdscr.timeout(100) 
            key = stdscr.getch()
            
            # If nothing was pressed, continue the loop
            if key == -1:
                continue

            command = handle_keypress(key, state)
            
            # 2.2 Process Command
            
            if command == 'QUIT':
                # Check for dirty flag and prompt user before quitting
                if quit_prompt(stdscr, state):
                    break
                else:
                    # If quit is cancelled, continue the loop without redrawing here
                    continue
            
            # Action: Call save_file when Ctrl+X (command 'SAVE') is detected
            if command == 'SAVE':
                save_file(state)
            
            # Only redraw the screen if a command that changes state/view was executed
            if command in ('EDIT', 'MOVE', 'MODE_CHANGE', 'HALF_EDIT', 'SAVE'):
                draw_screen(stdscr, state)
                position_cursor(stdscr, state)
                stdscr.refresh()
                
        except curses.error as e:
            # Handle resizing or other curses errors gracefully
            if "resize" in str(e):
                curses.update_panels()
                curses.doupdate()
            else:
                # Other non-fatal errors can be printed to stderr
                print(f"Curses error: {e}", file=sys.stderr)
        except Exception as e:
            # Catch all other unexpected errors
            stdscr.endwin()
            print(f"Fatal error: {e}", file=sys.stderr)
            break

# --- Entry Point ---

def run_editor() -> None:
    """
    Sets up the curses environment and calls the main function.
    """
    if len(sys.argv) != 2:
        print("Usage: python hex_editor.py <filepath>")
        sys.exit(1)
        
    filepath = sys.argv[1]
    
    # Check if the file exists before attempting to start curses
    if not os.path.exists(filepath):
        print(f"Error: File not found at '{filepath}'")
        sys.exit(1)
        
    # Check if the file is readable and writable (best effort)
    if not os.access(filepath, os.R_OK | os.W_OK):
        print(f"Error: File '{filepath}' is not readable or writable. Read-only mode implied.")
    
    # The curses.wrapper handles initialization and cleanup automatically
    try:
        curses.wrapper(main, filepath)
    except Exception as e:
        # Final catch for errors outside of the main loop wrapper
        print(f"An unexpected error occurred during curses initialization: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    run_editor()