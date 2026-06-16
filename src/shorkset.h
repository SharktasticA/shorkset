/*
    ######################################################
    ##             SHORK UTILITY - SHORKSET             ##
    ######################################################
    ## Main program logic                               ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



#ifndef SHORKSET
#define SHORKSET

#include "shorkmenu.h"

#include <linux/limits.h>



typedef struct {
    // Display resolution code (default: 3840)
    int dispRes;
    // Font colour name (default: "white")
    char fontColName[32];
    // Font colour ANSI escape code (default: "0;37")
    char fontColANSI[32];
    // Font PSF path (default: "default")
    char fontPSF[PATH_MAX];
} Config;



static const char *CFG_PATHS[] = {
    "/boot/grub/grub.cfg",
    "/boot/syslinux/syslinux.cfg"
};
static const int CFG_PATHS_LEN = sizeof(CFG_PATHS) / sizeof(CFG_PATHS[0]);
extern Config CONFIG;
static const char *DOT_CONF = "/etc/shorkset.conf";

void applyColourFiles(char*);
void applyColourTtys(char*);
void getCurrRes(void);
void loadConf(void);
void saveDispRes(MenuItem);
void saveFontCol(MenuItem);
void showDispResMenu(void);
void showFontColMenu(void);
void showHelp(void);
void showMainMenu(void);
void writeConf(void);

#endif
