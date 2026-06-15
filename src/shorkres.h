/*
    ######################################################
    ##             SHORK UTILITY - SHORKRES             ##
    ######################################################
    ## Main program logic                               ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



#ifndef SHORKRES
#define SHORKRES

#include "shorkmenu.h"



static const char *CFG_PATHS[] = {
    "/boot/grub/grub.cfg",
    "/boot/syslinux/syslinux.cfg"
};
static const int CFG_PATHS_LEN = sizeof(CFG_PATHS) / sizeof(CFG_PATHS[0]);
extern int CURR_RES;


void getCurrRes(void);
void saveRes(MenuItem);
void showMainMenu(void);
void showVESAResMenu(void);
void showVGAResMenu(void);

#endif
