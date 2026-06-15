/*
    ######################################################
    ##                  SHORK UTILITY                   ##
    ######################################################
    ## An interactive menu system for SHORK UTILITIES   ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (sharktastica.co.uk)                        ##
    ######################################################
*/



#ifndef SHORKMENU
#define SHORKMENU

#include <dirent.h>



typedef enum 
{
    CURSOR_DOWN,
    CURSOR_UP,
    CURSOR_LEFT,
    CURSOR_RIGHT,
    QUIT,
    ENTER,
    INVALID
} NavInput;

typedef struct 
{
    char *id;
    char name[80];
    void (*action)(void);
    int visible;
} MenuItem;



#define DT_EXE  100

extern int AVAIL_HEIGHT;
extern int BASE_ROW;
extern int COL_ENABLED;
extern char *COL_FOR_ARROW;
extern char *COL_FOR_CODE;
extern char *COL_FOR_CURSOR;
extern char *COL_FOR_HEADING;
extern char *COL_FOR_OL;
extern int COMPACT;
extern char CURSOR_CHAR;
extern char *EXIT_MSG;
extern struct termios OLD_TERMIOS;
extern struct winsize TERM_SIZE;



void awaitInput(void);
void clearScreen(void);
void disableRawMode(void);
void enableRawMode(void);
NavInput getNavInput(void);
void onExit(void);
void onSigInt(int);
void printDir(struct dirent**, int, int, int);
void printFooter(char*);
void printGenericScreen(char*, char*);
void printHeader(char*);
void printMenu(MenuItem*, int, int, int, int, int, int, int, int);
void printScrollingText(char*, int, int);
int rowsInCol(int, int, int);
void setupMenuSys(void);
void showCursor(void);
void showDialog(char*, int);

#endif
