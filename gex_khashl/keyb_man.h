#ifndef GEX_KEYB_MAN_H
#define GEX_KEYB_MAN_H

void e_handle_keys(int k);
void e_handle_full_grid_keys(int k);
void e_handle_partial_grid_keys(int k);

void handle_click(clickwin win, int row, int col);
void handle_in_screen_movement(int k);
void handle_scrolling_movement(int k);
void handle_edit_keys(int k);

void k_left(void);
void k_right(void);


#endif

/*
#define KEY_CODE_YES   256    A wchar_t contains a key code 
#define KEY_MIN        257    Minimum curses key 
#define KEY_BREAK      257    Break key (unreliable) 
#define KEY_SRESET     344    Soft (partial) reset (unreliable) 
#define KEY_RESET      345    Reset or hard reset (unreliable) 
#define KEY_DOWN       258    down-arrow key 
#define KEY_UP         259    up-arrow key 
#define KEY_LEFT       260    left-arrow key 
#define KEY_RIGHT      261    right-arrow key 
#define KEY_HOME       262    home key 
#define KEY_BACKSPACE  263    backspace key 
#define KEY_F0         264    Function keys.  Space for 64 
#define KEY_F(n)      (264+(n))   Value of function key n 
#define KEY_DL         328    delete-line key 
#define KEY_IL         329    insert-line key 
#define KEY_DC         330    delete-character key 
#define KEY_IC         331    insert-character key 
#define KEY_EIC        332    sent by rmir or smir in insert mode 
#define KEY_CLEAR      333    clear-screen or erase key 
#define KEY_EOS        334    clear-to-end-of-screen key 
#define KEY_EOL        335    clear-to-end-of-line key 
#define KEY_SF         336    scroll-forward key 
#define KEY_SR         337    scroll-backward key 
#define KEY_NPAGE      338    next-page key 
#define KEY_PPAGE      339    previous-page key 
#define KEY_STAB       340    set-tab key 
#define KEY_CTAB       341    clear-tab key 
#define KEY_CATAB      342    clear-all-tabs key 
#define KEY_ENTER      343    enter/send key 
#define KEY_PRINT      346    print key 
#define KEY_LL         347    lower-left key (home down) 
#define KEY_A1         348    upper left of keypad 
#define KEY_A3         349    upper right of keypad 
#define KEY_B2         350    center of keypad 
#define KEY_C1         351    lower left of keypad 
#define KEY_C3         352    lower right of keypad 
#define KEY_BTAB       353    back-tab key 
#define KEY_BEG        354    begin key 
#define KEY_CANCEL     355    cancel key 
#define KEY_CLOSE      356    close key 
#define KEY_COMMAND    357    command key 
#define KEY_COPY       358    copy key 
#define KEY_CREATE     359    create key 
#define KEY_END        360    end key 
#define KEY_EXIT       361    exit key 
#define KEY_FIND       362    find key 
#define KEY_HELP       363    help key 
#define KEY_MARK       364    mark key 
#define KEY_MESSAGE    365    message key 
#define KEY_MOVE       366    move key 
#define KEY_NEXT       367    next key 
#define KEY_OPEN       368    open key 
#define KEY_OPTIONS    369    options key 
#define KEY_PREVIOUS   370    previous key 
#define KEY_REDO       371    redo key 
#define KEY_REFERENCE  372    reference key 
#define KEY_REFRESH    373    refresh key 
#define KEY_REPLACE    374    replace key 
#define KEY_RESTART    375    restart key 
#define KEY_RESUME     376    resume key 
#define KEY_SAVE       377    save key 
#define KEY_SBEG       378    shifted begin key 
#define KEY_SCANCEL    379    shifted cancel key 
#define KEY_SCOMMAND   380    shifted command key 
#define KEY_SCOPY      381    shifted copy key 
#define KEY_SCREATE    382    shifted create key 
#define KEY_SDC        383    shifted delete-character key 
#define KEY_SDL        384    shifted delete-line key 
#define KEY_SELECT     385    select key 
#define KEY_SEND       386    shifted end key 
#define KEY_SEOL       387    shifted clear-to-end-of-line key 
#define KEY_SEXIT      388    shifted exit key 
#define KEY_SFIND      389    shifted find key 
#define KEY_SHELP      390    shifted help key 
#define KEY_SHOME      391    shifted home key 
#define KEY_SIC        392    shifted insert-character key 
#define KEY_SLEFT      393    shifted left-arrow key 
#define KEY_SMESSAGE   394    shifted message key 
#define KEY_SMOVE      395    shifted move key 
#define KEY_SNEXT      396    shifted next key 
#define KEY_SOPTIONS   397    shifted options key 
#define KEY_SPREVIOUS  398    shifted previous key 
#define KEY_SPRINT     399    shifted print key 
#define KEY_SREDO      400    shifted redo key 
#define KEY_SREPLACE   401    shifted replace key 
#define KEY_SRIGHT     402    shifted right-arrow key 
#define KEY_SRSUME     403    shifted resume key 
#define KEY_SSAVE      404    shifted save key 
#define KEY_SSUSPEND   405    shifted suspend key 
#define KEY_SUNDO      406    shifted undo key 
#define KEY_SUSPEND    407    suspend key 
#define KEY_UNDO       408    undo key 
#define KEY_MOUSE      409    Mouse event has occurred 
#define KEY_RESIZE     410    Terminal resize event 
#define KEY_MAX        511    Maximum key value is 0632(octal)=410(dec) 

*/
