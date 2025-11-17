import curses
import curses.ascii
from editor_state import EditorState, BYTES_PER_ROW, edit_byte
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    # Used for type hinting the curses object without importing it at runtime
    pass

# --- Display Functions ---

def init_tui(stdscr: 'curses._CursesWindow') -> None:
    """
    Initializes the curses environment, including color pairs and 
    window setup. It should prepare the screen for drawing.

    Args:
        stdscr: The main screen object provided by curses.wrapper.
    """
    # Turn off cursor blinking (optional, but nicer)
    curses.curs_set(1)
    
    # Enable color
    if curses.has_colors():
        curses.start_color()
        # Define color pairs (Foreground, Background)
        # Pair 1: Normal text (White on Black)
        curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLACK)
        # Pair 2: Status line / Headers (Black on Cyan)
        curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_CYAN)
        # Pair 3: Cursor highlight (Black on Red)
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_RED)
        # Pair 4: Dirty flag highlight (White on Blue)
        curses.init_pair(4, curses.COLOR_WHITE, curses.COLOR_BLUE)


def draw_screen(stdscr: 'curses._CursesWindow', state: EditorState) -> None:
    """
    The core rendering function. It completely redraws the hex editor 
    display (offset column, hex pane, ASCII pane) based on the current state.
    
    Args:
        stdscr: The main screen object.
        state: The current EditorState object.
    """
    # Clear the screen at the start of every draw cycle
    stdscr.clear()
    
    # Get screen dimensions
    max_y, max_x = stdscr.getmaxyx()
    display_rows = max_y - 2  # Leave space for header and status line

    # --- 1. RENDER HEADER LINE ---
    header = f" {state.filepath} | F-size: {state.file_size} bytes "
    stdscr.addstr(0, 0, header, curses.color_pair(2) | curses.A_BOLD)
    # Fill remaining space with padding using the header color
    stdscr.chgat(0, len(header), max_x - len(header), curses.color_pair(2))

    # Calculate total rows needed for the file content
    total_file_rows = (state.file_size + BYTES_PER_ROW - 1) // BYTES_PER_ROW

    # --- 2. RENDER HEX AND ASCII PANES ---
    
    # Ensure scroll_row is within valid bounds
    if state.scroll_row > total_file_rows - display_rows:
        state.scroll_row = max(0, total_file_rows - display_rows)

    # Autoscroll to keep the cursor visible vertically
    cursor_row_index = state.cursor_index // BYTES_PER_ROW
    # If cursor is above the visible area, scroll up
    if cursor_row_index < state.scroll_row:
        state.scroll_row = cursor_row_index
    # If cursor is below the visible area, scroll down
    elif cursor_row_index >= state.scroll_row + display_rows:
        state.scroll_row = cursor_row_index - display_rows + 1

    
    for screen_row in range(1, max_y - 1): # Start at y=1 (below header)
        data_row_index = state.scroll_row + (screen_row - 1)
        
        # Check if we are past the end of the file data
        if data_row_index * BYTES_PER_ROW >= state.file_size:
            break
        
        # Calculate the starting index for this row's data
        row_start_index = data_row_index * BYTES_PER_ROW
        
        # --- OFFSET COLUMN (0x00000000) ---
        offset_str = f"{row_start_index:08X}:"
        stdscr.addstr(screen_row, 0, offset_str, curses.A_DIM)

        # Start position for hex content after offset
        hex_x = 10 
        # Calculate start of ASCII pane
        ascii_x = hex_x + (BYTES_PER_ROW * 3) + 2 

        # Process the 16 bytes for this row
        for i in range(BYTES_PER_ROW):
            current_byte_index = row_start_index + i
            
            # Draw content or padding
            if current_byte_index < state.file_size:
                byte_val = state.data[current_byte_index]
                
                # A. Hex Representation
                hex_value = f"{byte_val:02X}"
                
                # B. ASCII Representation
                if 32 <= byte_val <= 126:
                    ascii_char = chr(byte_val)
                else:
                    ascii_char = '.'
                
                # C. Check for cursor highlight
                if current_byte_index == state.cursor_index:
                    # Cursor is here: use highlight color
                    byte_color = curses.color_pair(3) | curses.A_BOLD
                else:
                    # No cursor: use normal color
                    byte_color = curses.color_pair(1)
                
                # Draw the hex byte
                hex_byte_x = hex_x + (i * 3) 
                
                # Special rendering for half-edited hex byte
                if state.edit_mode == 'hex' and current_byte_index == state.cursor_index and state.hex_input_buffer is not None:
                    # Draw the first nibble from the buffer, the second from the original data
                    display_hex_value = state.hex_input_buffer.upper() + hex_value[1] 
                    stdscr.addstr(screen_row, hex_byte_x, display_hex_value, byte_color)
                else:
                    stdscr.addstr(screen_row, hex_byte_x, hex_value, byte_color)
                    
                # Draw the ascii character
                ascii_byte_x = ascii_x + i
                stdscr.addstr(screen_row, ascii_byte_x, ascii_char, byte_color)
                
            else:
                # Padding for end-of-file
                hex_byte_x = hex_x + (i * 3)
                stdscr.addstr(screen_row, hex_byte_x, "  ")
                ascii_byte_x = ascii_x + i
                stdscr.addstr(screen_row, ascii_byte_x, " ")
        
    # --- 3. RENDER STATUS LINE ---
    status_y = max_y - 1
    
    # Mode info
    mode_str = f" Mode: {state.edit_mode.upper()} "
    
    # Cursor info
    cursor_info = f" Pos: {state.cursor_index:08X} (dec: {state.cursor_index}) "
    
    # Dirty status
    dirty_str = " MODIFIED " if state.is_dirty else " CLEAN "
    dirty_color = curses.color_pair(4) if state.is_dirty else curses.color_pair(1)

    # Combine status parts
    status_line = mode_str + cursor_info
    
    stdscr.addstr(status_y, 0, status_line)
    stdscr.addstr(status_y, len(status_line) + 1, dirty_str, dirty_color | curses.A_BOLD)
    
    # Pad the rest of the line and ensure color
    stdscr.chgat(status_y, 0, max_x, curses.color_pair(1)) 
    stdscr.addstr(status_y, 0, status_line + dirty_str) 


def position_cursor(stdscr: 'curses._CursesWindow', state: EditorState) -> None:
    """
    Places the physical curses cursor based on the logical cursor_index 
    and edit_mode in the state object.

    Args:
        stdscr: The main screen object.
        state: The current EditorState object.
    """
    if state.file_size == 0:
        return

    # Determine the screen row of the cursor
    row_index = state.cursor_index // BYTES_PER_ROW
    screen_row = row_index - state.scroll_row + 1 # +1 for header line
    
    if screen_row >= 1 and screen_row < stdscr.getmaxyx()[0] - 1:
        byte_in_row = state.cursor_index % BYTES_PER_ROW
        
        hex_x_offset = 10 
        ascii_x_offset = hex_x_offset + (BYTES_PER_ROW * 3) + 2

        if state.edit_mode == 'hex':
            # Hex mode: cursor position is at the first nibble of the byte
            # Column: 10 + (byte_in_row * 3)
            # Use 1 if hex_input_buffer is active (second nibble input)
            target_x = hex_x_offset + (byte_in_row * 3) + (1 if state.hex_input_buffer else 0)
            
        elif state.edit_mode == 'ascii':
            # ASCII mode: cursor position is exactly on the character
            target_x = ascii_x_offset + byte_in_row
            
        else:
            # Fallback
            target_x = 0
            
        # Move the physical terminal cursor
        stdscr.move(screen_row, target_x)
    else:
        # If cursor is off-screen, move the physical cursor to the status line
        stdscr.move(stdscr.getmaxyx()[0] - 1, 0)
        

# --- Input Functions ---

def handle_keypress(key: int, state: EditorState) -> str:
    """
    Processes a single keypress, handles movement, mode switching, 
    save/quit commands, and relays edit characters to edit_byte().

    Returns the command executed, e.g., 'QUIT', 'SAVE', 'EDIT', 'MOVE', 'HALF_EDIT', 'NO_OP'.

    Args:
        key: The integer representation of the key pressed (from stdscr.getch()).
        state: The current EditorState object (modified in-place by movement/edits).
    """
    
    if state.file_size == 0:
        # Only allow quit if the file is empty
        if key in (ord('q'), 17):
            return 'QUIT'
        return 'NO_OP'

    # --- Command Handling ---
    
    # SAVE Command (Ctrl+W)
    if key == 23: # 23 is Ctrl+W
        return 'SAVE' 
    
    # QUIT Command (Ctrl+X) 
    if key == 24: # 24 is Ctrl+X
        return 'QUIT' 

    # Mode Toggle (Tab key)
    if key == ord('\t'):
        state.edit_mode = 'ascii' if state.edit_mode == 'hex' else 'hex'
        state.hex_input_buffer = None # Clear buffer on mode switch
        return 'MODE_CHANGE'

    # --- Editing Logic ---
    # Check if the key is a printable ASCII character
    if curses.ascii.isprint(key):
        char_input = chr(key)
        
        if state.edit_mode == 'hex':
            # Hex mode: only accept valid hex digits
            if char_input.lower() in '0123456789abcdef':
                # Call the new edit_byte function
                if edit_byte(state, char_input):
                    return 'EDIT'      # Full byte edit complete
                else:
                    return 'HALF_EDIT' # First nibble input
        
        elif state.edit_mode == 'ascii':
            # ASCII mode: accept any printable character (checked above)
            # Call the new edit_byte function
            if edit_byte(state, char_input):
                return 'EDIT' # Full byte edit complete
        
    # --- Movement Logic (Arrow Keys) ---
    new_index = state.cursor_index
    command = 'NO_OP'

    if key == curses.KEY_LEFT:
        new_index = state.cursor_index - 1
        command = 'MOVE'
    elif key == curses.KEY_RIGHT:
        new_index = state.cursor_index + 1
        command = 'MOVE'
    elif key == curses.KEY_UP:
        new_index = state.cursor_index - BYTES_PER_ROW
        command = 'MOVE'
    elif key == curses.KEY_DOWN:
        new_index = state.cursor_index + BYTES_PER_ROW
        command = 'MOVE'

    # --- Clamping and Index Update ---
    if command == 'MOVE':
        # Clamp the new index to be within [0, file_size - 1]
        new_index = max(0, min(new_index, state.file_size - 1))
        
        if new_index != state.cursor_index:
            state.cursor_index = new_index
            # When cursor moves, clear any pending hex input
            state.hex_input_buffer = None
            return 'MOVE' # Return MOVE if the cursor actually changed position

    return 'NO_OP'
    