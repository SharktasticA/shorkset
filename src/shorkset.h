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
    // System volume (default: 40)
    int volume;
} Config;

typedef struct {
    char *name;
    char *category;
    char *bus;
    char *description;
} ModuleEntry;



#define CSV_BUFFER          16384
#define CONFONTS_DIR        "/usr/share/consolefonts"
#define DOT_CONF            "/etc/shorkset.conf"
#define KERNEL_VER_LEN      16
#define KEYMAPS_DIR         "/usr/share/keymaps"
#define MAX_FONTS           128
#define MAX_KEYMAPS         64
#define MAX_MODULES_ENTRY   128
#define MODULES_CSV_PATH    "/usr/share/shorkset/modules.csv"
#define MODULES_DIR         "/lib/modules"

static const char *CFG_PATHS[] = {
    "/boot/grub/grub.cfg",
    "/boot/syslinux/syslinux.cfg"
};
static const int CFG_PATHS_LEN = sizeof(CFG_PATHS) / sizeof(CFG_PATHS[0]);
extern Config CONFIG;
extern char CONFONTS[MAX_FONTS][PATH_MAX];
extern int CONFONTS_COUNT;
extern char KERNEL_VER[KERNEL_VER_LEN];
extern char KEYMAPS[MAX_KEYMAPS][PATH_MAX];
extern int KEYMAPS_COUNT;
extern ModuleEntry MODULES[MAX_MODULES_ENTRY];
extern int MODULES_NO;



void applyFontColFiles(char*);
void applyFontColTtys(char*);
void applyVolume(int);
void getCurrRes(void);
void getKernelVer(void);
int getHWVolume(int);
void loadConf(void);
int loadConFonts(void);
int loadKeymaps(void);
int loadModules(void);
void saveDispRes(MenuItem, int);
void saveFontCol(MenuItem);
void saveFontPSF(MenuItem);
void saveKeymap(MenuItem);
void saveVolume(MenuItem);
void showDispResMenu(void);
void showDriverCatsMenu(void);
void showDriversListMenu(const char*);
void showFontColMenu(void);
void showFontPSFMenu(void);
void showHelp(void);
void showKeymapMenu(void);
void showMainMenu(void);
void showNetDriversMenu(void);
void showSndDriversMenu(void);
void showVolumeMenu(void);
void writeConf(void);

#endif
