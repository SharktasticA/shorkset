/*
    ######################################################
    ##                  SHORK UTILITY                   ##
    ######################################################
    ## An interactive menu system for SHORK UTILITIES & ##
    ## SHORK ENTERTAINMENT                              ##
    ######################################################
    ## Revision C                                       ##
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



#define DT_EXE              100
#define MENU_ITEM_ID_LEN    80
#define MENU_ITEM_NAME_LEN  80



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
    char id[MENU_ITEM_ID_LEN];
    char name[MENU_ITEM_NAME_LEN];
    char *payload;
    void (*action)(void);
    int isVisible;
    int isStatic;
} MenuItem;




extern int AVAIL_HEIGHT;
extern int BASE_ROW;
extern char *COL_BAK_BAR;
extern char *COL_BAK_MENU_MSG;
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
void markEntry(char*, const char*, const int);
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
void unmarkEntry(char*);

#endif
