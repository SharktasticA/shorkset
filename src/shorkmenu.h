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



#ifndef SHORKMENU
#define SHORKMENU

#include <dirent.h>
#include <linux/limits.h>



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
    char id[80];
    char name[80];
    char *payload;
    void (*action)(void);
    int isVisible;
    int isStatic;
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
extern char *COL_FOR_SHORKUTIL;
extern int COMPACT;
extern char CURSOR_CHAR;
extern char *EXIT_MSG;
extern struct termios OLD_TERMIOS;
extern char SEPARATOR;
extern struct winsize TERM_SIZE;



void awaitInput(void);
void clearScreen(void);
void disableRawMode(void);
void enableRawMode(void);
void freeMenu(MenuItem*, int);
int getIntInput(char*, int, int, int);
NavInput getNavInput(void);
void onExit(void);
void onSigInt(int);
void printDir(struct dirent**, int, int, int);
void printFooter(char*);
void printHeader(char*);
void printMenu(MenuItem*, int, char*, int, int, int, int*, int*, int*, int*);
void printTextScreen(char*, char*, int, int);
int printYesNoScreen(char*, char*);
int rowsInCol(int, int, int);
void setupMenuSys(void);
void setupViewport(void);
void showCursor(void);
void showDialog(char*, int);

#endif
