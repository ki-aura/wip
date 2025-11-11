import struct
import sys
from dataclasses import dataclass, field
from typing import Optional, Literal

# Standard hex editor row size
BYTES_PER_ROW = 16

@dataclass
class EditorState:
    """Stores the entire state of the hex editor application."""
    filepath: str
    data: bytearray = field(default_factory=bytearray)
    file_size: int = 0
    
    # Cursor and View State
    cursor_index: int = 0  # Absolute byte index in the file
    scroll_row: int = 0    # First visible row index
    
    # Editing State
    is_dirty: bool = False
    edit_mode: Literal['hex', 'ascii'] = 'hex'
    
    # Hex input buffer: Stores the first nibble (char) while waiting for the second
    hex_input_buffer: Optional[str] = None


def load_file(filepath: str) -> EditorState:
    """
    Loads the contents of the file into the EditorState data buffer.
    Returns an empty state if the file cannot be loaded.
    """
    try:
        with open(filepath, 'rb') as f:
            data_bytes = f.read()
            data_array = bytearray(data_bytes)
            return EditorState(
                filepath=filepath,
                data=data_array,
                file_size=len(data_array)
            )
    except FileNotFoundError:
        print(f"Error: File not found at '{filepath}'", file=sys.stderr)
        # Return an initialized state with zero size to prevent AttributeErrors
        return EditorState(filepath=filepath)
    except Exception as e:
        print(f"Error loading file: {e}", file=sys.stderr)
        return EditorState(filepath=filepath)


def save_file(state: EditorState) -> None:
    """
    Writes the current data buffer (bytearray) back to the file specified
    in state.filepath. Resets the dirty flag upon successful write.
    """
    try:
        if not state.filepath:
            print("Error: Cannot save, no filepath defined.", file=sys.stderr)
            return

        # Open file in binary write mode ('wb') and overwrite the contents
        with open(state.filepath, 'wb') as f:
            f.write(state.data)
        
        # If writing succeeds, the buffer is no longer dirty
        state.is_dirty = False
        # Note: In a real app, you would add a status message here ("File saved!")

    except Exception as e:
        # If an error occurs (e.g., permissions), print it but keep the dirty flag True
        print(f"Error saving file '{state.filepath}': {e}", file=sys.stderr)


def edit_byte(state: EditorState, char_input: str) -> bool:
    """
    Modifies the byte at the current cursor_index based on the input character
    and the current edit mode (hex or ascii). Handles hex nibble-level input.

    Returns:
        True if the state was modified and the screen needs to be redrawn, False otherwise.
    """
    # Prevent editing if the file is empty or cursor is out of bounds
    if state.file_size == 0 or state.cursor_index >= state.file_size:
        return False
    if state.edit_mode == 'hex':
        char_input = char_input.lower()
    
    if state.edit_mode == 'hex':
        if char_input in '0123456789abcdef':
            if state.hex_input_buffer is None:
                # 1. First nibble received (half-edit)
                state.hex_input_buffer = char_input
                # Return False as the byte isn't fully changed yet (only visual)
                return False 
            else:
                # 2. Second nibble received: Complete the byte
                
                # Combine nibbles (e.g., 'a' and 'f' -> 0xAF)
                byte_str = state.hex_input_buffer + char_input
                new_byte_value = int(byte_str, 16)
                
                # Apply the change
                state.data[state.cursor_index] = new_byte_value
                state.is_dirty = True
                state.hex_input_buffer = None
                
                # Move cursor to the next byte
                state.cursor_index = min(state.cursor_index + 1, state.file_size - 1)
                
                return True # State modified, screen needs redraw
    
    elif state.edit_mode == 'ascii':
        # Check for printable ASCII (32 to 126)
        if 32 <= ord(char_input) <= 126:
            # Apply the change
            state.data[state.cursor_index] = ord(char_input)
            state.is_dirty = True
            
            # Move cursor to the next byte
            state.cursor_index = min(state.cursor_index + 1, state.file_size - 1)
            
            # Clear hex buffer just in case the user switched modes mid-nibble
            state.hex_input_buffer = None
            return True # State modified, screen needs redraw

    return False