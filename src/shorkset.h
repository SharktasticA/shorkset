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
    // Keyboard layout (keymap) path (default: "en_us")
    char keymap[PATH_MAX];
} Config;



#define MAX_FONTS       126
#define MAX_KEYMAPS     64

static const char *CFG_PATHS[] = {
    "/boot/grub/grub.cfg",
    "/boot/syslinux/syslinux.cfg"
};
static const int CFG_PATHS_LEN = sizeof(CFG_PATHS) / sizeof(CFG_PATHS[0]);
static Config CONFIG = { 3840, "white", "0;37", "default", "en_us" };
static char CONFONTS[MAX_FONTS][PATH_MAX];
static int CONFONTS_COUNT = 0;
static const char *CONFONTS_DIR = "/usr/share/consolefonts";
static const char *DOT_CONF = "/etc/shorkset.conf";
static char KEYMAPS[MAX_KEYMAPS][PATH_MAX];
static int KEYMAPS_COUNT = 0;
static const char *KEYMAPS_DIR = "/usr/share/keymaps";

void applyFontColFiles(char*);
void applyFontColTtys(char*);
void getCurrRes(void);
void loadConf(void);
int loadConFonts(void);
int loadKeymaps(void);
void saveDispRes(MenuItem, int);
void saveFontCol(MenuItem);
void saveFontPSF(MenuItem);
void saveKeymap(MenuItem);
void showDispResMenu(void);
void showFontColMenu(void);
void showFontPSFMenu(void);
void showHelp(void);
void showKeymapMenu(void);
void showMainMenu(void);
void writeConf(void);

#endif
