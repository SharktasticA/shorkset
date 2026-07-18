/*
    ######################################################
    ##                  SHORK UTILITY                   ##
    ######################################################
    ## An interactive menu system for SHORK UTILITIES & ##
    ## SHORK ENTERTAINMENT                              ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (sharktastica.co.uk)                        ##
    ######################################################
*/



#include "colours.h"
#include "general.h"
#include "shorkmenu.h"

#include <ctype.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>



int AVAIL_HEIGHT;
int BASE_ROW;
int COL_ENABLED = 1;
char *COL_FOR_ARROW = COL_FOR_BOLD_RED;
char *COL_FOR_CODE = COL_FOR_BOLD_RED;
char *COL_FOR_CURSOR = COL_FOR_BOLD_CYAN;
char *COL_FOR_HEADING = COL_FOR_BOLD_CYAN;
char *COL_FOR_OL = COL_FOR_GREEN;
char *COL_FOR_SHORKUTIL = COL_FOR_BOLD_MAGENTA;
int COMPACT = 0;
char CURSOR_CHAR = '*';
char *EXIT_MSG = NULL;
struct termios OLD_TERMIOS;
char SEPARATOR = '-';
struct winsize TERM_SIZE;



/**
 * Awaits for any user input.
 */
void awaitInput(void)
{
    int len = printf("Press any key to continue... ");
    if (COL_ENABLED)
        for (size_t i = len; i < TERM_SIZE.ws_col; i++)
            printf(" ");
    getchar();
}

/**
 * Moves the cursor to topleft-most position and clears below cursor.
 */
void clearScreen(void)
{
    printf("\033[H\033[J");
}

/**
 * Enables the terminal's canonical input. Used only when the program exits.
 */
void disableRawMode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &OLD_TERMIOS);
}

/**
 * Disables the terminal's canonical input so that things like getchar do not
 * wait until enter is pressed.
 */
void enableRawMode(void)
{
    struct termios newTERMIO;
    tcgetattr(STDIN_FILENO, &OLD_TERMIOS);
    newTERMIO = OLD_TERMIOS;
    newTERMIO.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTERMIO);
}

/**
 * Frees all the heap-allocated components of a menu.
 * @param menu Pointer to menu
 * @param size Menu size
 */
void freeMenu(MenuItem* menu, int size)
{
    if (!menu) return;
    for (int i = 0; i < size; i++)
        free(menu[i].payload);
}

/**
 * Gets an integer input from the user.
 * @param prompt Prompt to give the user 
 * @param min Minimum allowed input (set to same as max to disable validation)
 * @param max Maximum allowed input (set to same as min to disable validation)
 * @param negativeIfInvalid Flags if function should return -1 if invalid input instead of looping
 * @return User's integer input
 */
int getIntInput(char *prompt, int min, int max, int negativeIfInvalid)
{
    if (min > max) min = max;
    if (max < min) max = min;

    int isValid;
    char buffer[32];
    int val;

    do
    {
        disableRawMode();

        if (COL_ENABLED)
        {
            printf("\033[%s;%sm", COL_FOR_BOLD_WHITE, COL_BAK_BLUE);
            for (int i = 0; i < TERM_SIZE.ws_col; i++) printf(" ");
            printf("\033[1G");
        }

        printf("%s", prompt);
        if (min != max) printf(" (%d-%d)", min, max);
        printf(": ");

        if (fgets(buffer, sizeof(buffer), stdin) != NULL)
        {
            if (sscanf(buffer, "%d", &val) == 1)
            {
                if ((min != max) && (val >= min && val <= max))
                    isValid = 1;
                else if (min == max)
                    isValid = 1;
                else
                    isValid = 0;
            }
            else isValid = 0;
        }
        else
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            isValid = 0;
        }

        enableRawMode();

        if (!isValid && negativeIfInvalid)
            return -1;

    } while (!isValid);

    if (COL_ENABLED) printf("\033[%sm", COL_RESET);

    return val;
}

/**
 * @return The nav input action detected
 */
NavInput getNavInput(void)
{
    int c = getchar();

    if (c == 27)
    {
        getchar();
        switch (getchar())
        {
            case 'A': return CURSOR_UP;
            case 'B': return CURSOR_DOWN;
            case 'C': return CURSOR_RIGHT;
            case 'D': return CURSOR_LEFT;
        }
    }
    else
    {
        c = tolower(c);
        switch (c)
        {
            case '\n': return ENTER;
            case '\r': return ENTER;
            case 'q': return QUIT;
            case 'w': return CURSOR_UP;
            case 'a': return CURSOR_LEFT;
            case 's': return CURSOR_DOWN;
            case 'd': return CURSOR_RIGHT;
            case 'h': return CURSOR_LEFT;
            case 'j': return CURSOR_DOWN;
            case 'k': return CURSOR_UP;
            case 'l': return CURSOR_RIGHT;
        }
    }

    return INVALID;
}

/**
 * Makes the terminal cursor visible again, resets the terminal's colours and clears the screen
 * upon exiting.
 */
void onExit(void)
{
    disableRawMode();
    showCursor();
    clearScreen();

    if (EXIT_MSG)
    {
        printf("%s\n", EXIT_MSG);
        free(EXIT_MSG);
    }
}

/**
 * Used to handle exiting controllably if SIGINT is received (Ctrl+C).
 */
void onSigInt(int sig)
{
    exit(0);
}

/**
 * Prints a directory listing.
 * @param dirContents Pointer to one or more dirent structs for each file in the current directory
 * @param entryCount Number of entries in current directory
 * @param cursor Current line cursor position
 * @param cursorPrev Previous line cursor position
 */
void printDir(struct dirent **dirContents, int entryCount, int cursor, int cursorPrev)
{
    // If directory is empty
    if (!dirContents || entryCount == 0)
    {
        printf("(empty)\n");
        for (int i = 1; i < AVAIL_HEIGHT; i++)
            printf("\n");
        return;
    }

    // Viewport offset and clamping for current line cursor
    int offset = (cursor - 1) - (AVAIL_HEIGHT / 2);
    if (offset < 0) offset = 0;
    if (offset > entryCount - AVAIL_HEIGHT) offset = entryCount - AVAIL_HEIGHT;
    if (offset < 0) offset = 0;

    // Viewport offset and clamping for previous line cursor
    int prevOffset = (cursorPrev - 1) - (AVAIL_HEIGHT / 2);
    if (prevOffset < 0) prevOffset = 0;
    if (prevOffset > entryCount - AVAIL_HEIGHT) prevOffset = entryCount - AVAIL_HEIGHT;
    if (prevOffset < 0) prevOffset = 0;

    int inScrolling = (prevOffset != offset);

    // cursorPrev is initialised as 0 to indicate first frame should be drawn
    if (cursorPrev == 0) inScrolling = 1;

    int prevIndex = cursorPrev - 1;
    int currIndex = cursor - 1;

    // If we don't need to scroll, just update the cursor
    if (!inScrolling)
    {
        int rowPrev = BASE_ROW + (prevIndex - offset);
        int rowCurr = BASE_ROW + (currIndex - offset);

        // Remove old line cursor
        printf("\x1b[%d;1H   ", rowPrev);

        // Print new line cursor
        if (COL_ENABLED)
            printf("\x1b[%d;1H \033[%sm%c\033[%sm ", rowCurr, COL_FOR_CURSOR, CURSOR_CHAR, COL_RESET);
        else
            printf("\x1b[%d;1H %c ", rowCurr, CURSOR_CHAR);

        return;
    }

    int canGoUp = offset > 0;
    int canGoDown = (offset + AVAIL_HEIGHT) < entryCount;
    int linesPrinted = 0;

    for (int i = offset; i < entryCount && i < offset + AVAIL_HEIGHT; i++)
    {
        printf("\x1b[%d;1H\x1b[K", BASE_ROW + linesPrinted);

        char prefix = '?';
        switch (dirContents[i]->d_type)
        {
            case DT_DIR: prefix = 'd'; break;
            case DT_REG: prefix = 'f'; break;
            case DT_EXE: prefix = 'x'; break;
            case DT_LNK: prefix = 'l'; break;
            case DT_FIFO: prefix = '|'; break;
            case DT_CHR: prefix = 'c'; break;
            case DT_BLK: prefix = 'b'; break;
            case DT_SOCK: prefix = 's'; break;
            default: prefix = '?'; break;
        }

        // Can scroll up indicator
        if (canGoUp && i == offset)
        {
            if (COL_ENABLED)
                printf("\033[%sm^\033[%sm\x1b[K\n", COL_FOR_ARROW, COL_RESET);
            else
                printf("^\x1b[K\n");
        }
        // Can scroll down indicator
        else if (canGoDown && i == offset + AVAIL_HEIGHT - 1)
        {
            if (COL_ENABLED)
                printf("\033[%smv\033[%sm\n", COL_FOR_ARROW, COL_RESET);
            else
                printf("v\x1b[K\n");
        }
        // Selected line
        else if (i == currIndex)
        {
            if (COL_ENABLED)
                printf(" \033[%sm%c\033[%sm %c %s\n", COL_FOR_CURSOR, CURSOR_CHAR, COL_RESET, prefix, dirContents[i]->d_name);
            else
                printf(" %c %c %s\n", CURSOR_CHAR, prefix, dirContents[i]->d_name);
        }
        // Other lines
        else
            printf("   %c %s\n", prefix, dirContents[i]->d_name);

        linesPrinted++;
    }

    // "Fill in" lines if listing is shorter than viewport
    if (!canGoUp && !canGoDown)
        for (int i = linesPrinted; i < AVAIL_HEIGHT; i++)
            printf("\n");
}

/**
 * @param footnote Text to be printed in the footer
 */
void printFooter(char *footnote)
{
    if (COL_ENABLED)
    {
        printf("\033[%d;1H", TERM_SIZE.ws_row);
        printf("\033[%s;%sm", COL_FOR_BOLD_WHITE, COL_BAK_BLUE);
        int len = 0;
        if (footnote)
            len = printf("%s", footnote);
        for (int i = len; i < TERM_SIZE.ws_col; i++) printf(" ");
        printf("\033[%sm", COL_RESET);
    }
    else
    {
        printf("\033[%d;1H", TERM_SIZE.ws_row - 1);
        for (int i = 0; i < TERM_SIZE.ws_col; i++)
            putchar(SEPARATOR);
        printf("\033[%d;1H", TERM_SIZE.ws_row);
        int len = 0;
        if (footnote)
            len = printf("%s", footnote);
    }
}

/**
 * @param title Text to be printed in the header
 */
void printHeader(char *title)
{
    if (COL_ENABLED)
        printf("\033[%s;%sm", COL_FOR_BOLD_WHITE, COL_BAK_BLUE);

    size_t titleLen = strlen(title);
    if (titleLen <= TERM_SIZE.ws_col) 
    {
        printf("%s", title);
        if (COL_ENABLED)
        {
            for (size_t i = titleLen; i < TERM_SIZE.ws_col; i++)
                printf(" ");
        }
        else
            printf("\n");
    }
    else
    {
        size_t visibleLen = TERM_SIZE.ws_col - 3;
        char *start = title + (titleLen - visibleLen);
        printf("...%s\n", start);
    }

    if (COL_ENABLED)
        printf("\033[%sm", COL_RESET);
    else
        for (int i = 0; i < TERM_SIZE.ws_col; i++)
            putchar(SEPARATOR);
}

/**
 * @param menu The menu to print
 * @param menuSize How many items are in the menu
 * @param msg Optional essage to display at the top of the message
 * @param cols Number of columns to draw
 * @param colWidth How many characters are in a column (excluding the cursor indicator)
 * @param rows Number of rows to draw
 * @param cursorX Current column cursor position
 * @param cursorY Current row cursor position
 * @param cursorXPrev Previous column cursor position
 * @param cursorYPrev Previous row cursor position
 */
void printMenu(MenuItem *menu, int menuSize, char *msg, int cols, int colWidth, int rows, int *cursorX, int *cursorY, int *cursorXPrev, int *cursorYPrev)
{
    // If a menu item is static, we need to skip the cursor over it
    if (menu[(*cursorX + *cursorY) - 2].isStatic)
    {
        // Going up
        if (*cursorYPrev > 0 && *cursorY < *cursorYPrev && !(*cursorYPrev == rows && *cursorXPrev == cols && *cursorY == 1))
        {
            // Decrement the cursor until we reach something non-static
            while (*cursorY > 0 && menu[(*cursorX - 1) * rows + *cursorY - 1].isStatic)
                (*cursorY)--;

            // If the cursor is now out of bounds, wrap around and try again
            if (*cursorY < 1)
            {
                *cursorY = rows;
                while (*cursorY > 0 && menu[(*cursorX - 1) * rows + *cursorY - 1].isStatic)
                    (*cursorY)--;
            }
        }
        // Going down
        else
        {
            // Increment the cursor until we reach something non-static
            while (*cursorY <= rows && menu[(*cursorX - 1) * rows + *cursorY - 1].isStatic)
                (*cursorY)++;

            // If the cursor is now out of bounds, wrap around and try again
            if (*cursorY > rows)
            {
                *cursorY = 1;
                while (*cursorY <= rows && menu[(*cursorX - 1) * rows + *cursorY - 1].isStatic)
                    (*cursorY)++;
            }
        }
    }



    // Format and clamp potential message to display
    int msgLines = 0;
    char *msgBuffer = NULL;
    if (msg && msg[0] != '\0')
    {
        size_t msgLen = strlen(msg) + 1;
        msgBuffer = malloc(msgLen);
        if (msgBuffer)
        {
            strncpy(msgBuffer, msg, msgLen);
            msgLines = formatNewLines(msgBuffer, TERM_SIZE.ws_col, NULL, 1);
            if (msgLines > AVAIL_HEIGHT - 2)
                msgLines = AVAIL_HEIGHT - 2;
        }
    }

    int adjustedAvailHeight = AVAIL_HEIGHT;
    int adjustedBaseRow = BASE_ROW;

    if (msgLines > 0)
    {
        // Adjust parameters for the menu's viewport size
        int reserved = msgLines;
        if (!COL_ENABLED) reserved++;
        adjustedAvailHeight -= reserved;
        adjustedBaseRow += reserved;
        if (adjustedBaseRow < 1) adjustedBaseRow = 1;

        // Print message
        if (cursorYPrev == 0)
        {
            char *currPos = msgBuffer;
            for (int i = 0; i < msgLines; i++)
            {
                printf("\x1b[%d;1H\x1b[K", BASE_ROW + i);

                char *nextNL = strchr(currPos, '\n');
                if (nextNL)
                {
                    printf("%.*s\n", (int)(nextNL - currPos), currPos);
                    currPos = nextNL + 1;
                }
                else
                {
                    printf("%s\n", currPos);
                    currPos += strlen(currPos);
                }
            }

            int sepRow = BASE_ROW + msgLines;
            printf("\x1b[%d;1H\x1b[K", sepRow);
            if (!COL_ENABLED)
            {
                for (int i = 0; i < TERM_SIZE.ws_col; i++)
                    putchar(SEPARATOR);
                printf("\x1b[%d;1H\x1b[K", sepRow + 1);
            }
            else
                printf("\n");
        }

        free(msgBuffer);
    }



    // Viewport offset and clamping for current row cursor
    int offset = (*cursorY - 1) - (adjustedAvailHeight / 2);
    if (offset < 0) offset = 0;
    if (offset > rows - adjustedAvailHeight) offset = rows - adjustedAvailHeight;
    if (offset < 0) offset = 0;

    // Viewport offset and clamping for previous row cursor
    int prevOffset = (*cursorYPrev - 1) - (adjustedAvailHeight / 2);
    if (prevOffset < 0) prevOffset = 0;
    if (prevOffset > rows - adjustedAvailHeight) prevOffset = rows - adjustedAvailHeight;
    if (prevOffset < 0) prevOffset = 0;



    int inScrolling = (prevOffset != offset);

    // cursorPrev is initialised as 0 to indicate first frame should be drawn
    if (*cursorYPrev == 0) inScrolling = 1;

    int prevIndex = *cursorYPrev - 1;
    int currIndex = *cursorY - 1;

    // If we don't need to scroll, just update the cursor
    if (!inScrolling)
    {
        // Remove old line cursor
        int prevCol = 1 + (*cursorXPrev - 1) * (colWidth + 3);
        int prevRow = adjustedBaseRow + (*cursorYPrev - 1 - offset);
        printf("\x1b[%d;%dH   ", prevRow, prevCol);

        // Print new line cursor
        int currCol = 1 + (*cursorX - 1) * (colWidth + 3);
        int currRow = adjustedBaseRow + (*cursorY - 1 - offset);
        if (COL_ENABLED)
            printf("\x1b[%d;%dH \033[%sm%c\033[%sm ", currRow, currCol, COL_FOR_CURSOR, CURSOR_CHAR, COL_RESET);
        else
            printf("\x1b[%d;%dH %c ", currRow, currCol, CURSOR_CHAR);
        
        return;
    }



    int canGoUp = offset > 0;
    int canGoDown = (offset + adjustedAvailHeight) < rows;
    int linesPrinted = 0;

    for (int i = offset; i < rows && linesPrinted < adjustedAvailHeight; i++)
    {
        printf("\x1b[%d;1H\x1b[K", adjustedBaseRow + linesPrinted);

        // Can scroll up indicator
        if (canGoUp && i == offset)
        {
            if (COL_ENABLED)
                printf("\033[%sm^\033[%sm\x1b[K\n", COL_FOR_ARROW, COL_RESET);
            else
                printf("^\n");
        }
        // Can scroll down indicator
        else if (canGoDown && i == offset + adjustedAvailHeight - 1)
        {
            if (COL_ENABLED)
                printf("\033[%smv\033[%sm\n", COL_FOR_ARROW, COL_RESET);
            else
                printf("v\n");
        }
        // Selected row
        else
        {
            for (int j = 0; j < cols; j++)
            {
                int cursor = i + j * rows;
                if (cursor < menuSize)
                {
                    if (menu[cursor].isStatic)
                    {
                        if (COL_ENABLED)
                            printf("\033[%sm%-*s\033[%sm", COL_FOR_HEADING, colWidth + 3, menu[cursor].name, COL_RESET);
                        else
                            printf("%-*s", colWidth + 3, menu[cursor].name);
                    }
                    else if (j + 1 == *cursorX && i + 1 == *cursorY)
                    {
                        if (COL_ENABLED)
                            printf(" \033[%sm%c\033[%sm %-*s", COL_FOR_CURSOR, CURSOR_CHAR, COL_RESET, colWidth, menu[cursor].name);
                        else
                            printf(" %c %-*s", CURSOR_CHAR, colWidth, menu[cursor].name);
                    }
                    else
                        printf("   %-*s", colWidth, menu[cursor].name);
                }
            }
            printf("\n");
        }

        linesPrinted++;
    }



    // "Fill in" remaining viewport lines
    for (int i = linesPrinted; i < adjustedAvailHeight; i++)
        printf("\n");
}

/**
 * @param title Header's text string
 * @param text Text for the main body
 * @param totalLines How many newlines are present in main body text
 * @param pageScroll Flags if scrolling should be page based instead of line-by-line
 */
void printTextScreen(char *title, char *text, int totalLines, int pageScroll)
{
    printHeader(title);
    printFooter("[jk] Scroll [q] Back");

    char *textLines[totalLines > 0 ? totalLines : 1];
    if (totalLines > 1) splitText(text, textLines, totalLines);
    else textLines[0] = text;

    int offscreen = totalLines - AVAIL_HEIGHT;
    if (offscreen < 0) offscreen = 0;

    int running = 1;
    int cursor = 0;
    int canGoUp = 0;
    int canGoDown = 0;

    while (running)
    {
        canGoUp = cursor > 0;
        canGoDown = (cursor + AVAIL_HEIGHT) < totalLines;

        for (int i = 0; i < AVAIL_HEIGHT; i++)
        {
            printf("\x1b[%d;1H\x1b[K", BASE_ROW + i);

            if (canGoUp && i == 0)
            {
                if (COL_ENABLED)
                    printf("\033[%sm^\033[%sm\x1b[K\n", COL_FOR_ARROW, COL_RESET);
                else
                    printf("^\x1b[K\n");
            }
            else if (canGoDown && i == AVAIL_HEIGHT - 1)
            {
                if (COL_ENABLED)
                    printf("\033[%smv\033[%sm\x1b[K\n", COL_FOR_ARROW, COL_RESET);
                else
                    printf("v\x1b[K\n");
            }
            else if (i + cursor < totalLines)
                printf("%s", textLines[i + cursor]);
            else
                printf("\n");
        }

        NavInput input = getNavInput();
        switch (input)
        {
            case CURSOR_UP:
                if (pageScroll) cursor -= (AVAIL_HEIGHT - 3);
                else cursor--;
                if (cursor < 0) cursor = 0;
                break;

            case CURSOR_DOWN:
                if (pageScroll) cursor += (AVAIL_HEIGHT - 3);
                else cursor++;
                if (cursor > offscreen) cursor = offscreen;
                break;

            case QUIT:
                running = 0;
                break;
        }
    }
}

/**
 * @param title Header's text string
 * @param text Text for the main body
 * @return 1 = yes; 0 = no; -1 = error
 */
int printYesNoScreen(char *title, char *prompt)
{
    const int menuSize = 2;
    MenuItem menu[2] = {
        { "yes",    "Yes",  "", NULL,   1 },
        { "no",     "No",   "", NULL,   1 }
    };

    int running = 1;
    int cursorX = 1;
    int cursorY = 2;
    int cursorXPrev = 0;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader(title);
            printMenu(menu, menuSize, prompt, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Cancel");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, prompt, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorYPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorYPrev = cursorY;
                cursorY--;
                if (cursorY < 1) cursorY = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorYPrev = cursorY;
                cursorY++;
                if (cursorY > menuSize) cursorY = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                if (strcmp(menu[cursorY - 1].id, "yes") == 0)
                    return 1;
                else
                    return 0;
                break;

            case QUIT:
                running = 0;
                return -1;
                break;

            case INVALID:
                fullRedraw = 0;
                break;
        }
    }

    clearScreen();
    return -1;
}

/**
 * Gets how many valid rows there are in the current menu column. Used to help clamp menu cursors.
 */
int rowsInCol(int menuSize, int rows, int col)
{
    int start = (col - 1) * rows;
    int end   = start + rows;
    if (start >= menuSize) return 0;
    if (end > menuSize) return menuSize - start;
    return rows;
}

void setupMenuSys(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    atexit(onExit);
    signal(SIGINT, onSigInt);
    
    if (!COL_ENABLED)
    {
        COL_FOR_ARROW = COL_FOR_RESET;
        COL_FOR_CODE = COL_FOR_RESET;
        COL_FOR_CURSOR = COL_FOR_RESET;
        COL_FOR_HEADING = COL_FOR_RESET;
        COL_FOR_OL = COL_FOR_RESET;
        COL_FOR_SHORKUTIL = COL_FOR_RESET;
    }

    enableRawMode();
    clearScreen();
    printf("\033[?25l");
    setupViewport();
}

void setupViewport(void)
{
    TERM_SIZE = getTerminalSize();
    if (TERM_SIZE.ws_col < 40 || TERM_SIZE.ws_row < 10)
    {
        EXIT_MSG = "ERROR: terminal size too small (must be 40x10 or larger)\n";
        exit(1);
    }

    if (COL_ENABLED)
    {
        BASE_ROW = 2;
        AVAIL_HEIGHT = TERM_SIZE.ws_row - BASE_ROW;
    }
    else
    {
        BASE_ROW = 3;
        AVAIL_HEIGHT = TERM_SIZE.ws_row - BASE_ROW - 1;
    }
}

void showCursor(void)
{
    printf("\033[?25h");
    if (COL_ENABLED)
        printf("\033[%sm", COL_RESET);
}

/**
 * General center-justified dialog message.
 * @param message Message the dialog should display
 * @param width How wide the dialog should be
 */
void showDialog(char *message, int width)
{
    if (width > TERM_SIZE.ws_col - 6) width = TERM_SIZE.ws_col - 6;
    
    // Modify message to fit the given width
    size_t msgLen = strlen(message) + 1;
    char buf[msgLen + 1];
    strncpy(buf, message, msgLen - 1);
    buf[msgLen - 1] = '\0';
    int lines = formatNewLines(buf, width, NULL, 0);

    // Calculate where to begin printing the dialog
    int startRow = (TERM_SIZE.ws_row - lines) / 2;
    int startCol = (TERM_SIZE.ws_col - (width + 4)) / 2 + 1;  

    int pad = '#';
    if (COL_ENABLED) 
    {
        // Set dialog console colours
        printf("\033[%s;%sm", COL_FOR_WHITE, COL_BAK_BLUE);
        pad = ' ';
    }

    // Print top border
    printf("\x1b[%d;%dH", startRow, startCol);
    for (int j = 0; j < width + 4; j++) putchar(pad);

    // Print message
    char *currPos = buf;
    for (int i = 0; i < lines; i++)
    {
        char *lineStart = currPos;
        char *nextNL = strchr(currPos, '\n');

        int len;
        // If future newline found, prepare to print until reaching it
        if (nextNL)
        {
            len = nextNL - currPos;
            currPos = nextNL + 1;
        }
        // If no future newline, prepare to print the rest of message
        else
        {
            len = strlen(currPos);
            currPos += len;
        }

        printf("\x1b[%d;%dH", startRow + 1 + i, startCol);

        printf("%c ", pad);
        printf("%.*s", len, lineStart);
        for (int j = len; j < width; j++) putchar(' ');
        printf(" %c", pad);
    }

    // Print bottom border
    printf("\x1b[%d;%dH", startRow + 1 + lines, startCol);
    for (int j = 0; j < width + 4; j++) putchar(pad);

    // Reset console colour
    if (COL_ENABLED) printf("\033[%sm", COL_RESET);
}
