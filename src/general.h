/*
    ######################################################
    ##             SHORK UTILITY - SHORKRES             ##
    ######################################################
    ## General, utility functions to be used throughout ##
    ## SHORKRES                                         ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



#ifndef GENERAL
#define GENERAL

#include <dirent.h>
#include <stdio.h>



typedef struct {
    int pid;
    char name[256];
} Process;



#define TASK_COMM_LEN       24



char *bytesToReadable(const char *, const long long);
char *extractFromPoint(char *, size_t, char, int);
char *findErase(const char *, const size_t, const char *);
char *findReplace(const char *, const size_t, const char *, const char *);
int formatNewLines(char *, int, char *, int);
float fSqrt(float);
int getIntInput(char*, int, int, int);
Process getParentProcess(int);
struct winsize getTerminalSize(void);
int isFileExecutable(char*, struct dirent*);
int isProgramInstalled(char*, int);
int iSqrt(int);
int procExists(const char *, const int);
int readHexFile(const char *);
void splitText(char*, char*[], int);

#endif
