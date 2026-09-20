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



#define CSV_BUFFER                  16384
#define CONFIG_FONT_COL_NAME_LEN    32
#define CONFIG_FONT_COL_ANSI_LEN    32
#define CONFIG_GPM_TYPE_LEN         16
#define CONFIG_MODULES_LEN          1024
#define CONFIG_NET_IFS_LEN          1024
#define CONFONTS_DIR                "/usr/share/consolefonts"
#define DOT_CONF                    "/etc/shorkset.conf"
#define KERNEL_VER_LEN              16
#define KEYMAPS_DIR                 "/usr/share/keymaps"
#define MAX_FONTS                   128
#define MAX_KEYMAPS                 64
#define MODULE_NAME_LEN             24
#define MAX_MODULES_ENTRIES         128
#define MAX_NET_IFS_ENTRIES         8
#define MODULE_NAME_LEN             24
#define MODULES_CSV_PATH            "/usr/share/shorkset/modules.csv"
#define MODULES_DIR                 "/lib/modules"
#define NET_IF_NAME_LEN             32
#define UDHCPC_DEFAULT_SCRIPT       "/usr/share/udhcpc/default.script"



typedef struct {
    // Display resolution code (default: 3840)
    int dispRes;
    // Font colour name (default: "white")
    char fontColName[CONFIG_FONT_COL_NAME_LEN];
    // Font colour ANSI escape code (default: "0;37")
    char fontColANSI[CONFIG_FONT_COL_ANSI_LEN];
    // Font PSF path (default: "default")
    char fontPSF[PATH_MAX];
    // gpm mouse device path (default: "/dev/input/mice")
    char gpmDev[PATH_MAX];
    // gpm enabled (default: 0)
    int gpmEnabled;
    // gpm responsiveness (default: 10)
    int gpmResp;
    // gpm mouse type (default: "imps2")
    char gpmType[CONFIG_GPM_TYPE_LEN];
    // Keyboard layout (keymap) path (default: "qwerty_en_us")
    char keymap[PATH_MAX];
    // Linux modules to load at system startup
    char modules[CONFIG_MODULES_LEN];
    // Networking enabled (default: 0)
    int netEnabled;
    // Network interfaces to activate at system startup
    char netIfs[CONFIG_NET_IFS_LEN];
    // System volume (default: 40)
    int volume;
} Config;

typedef struct {
    char modules[MAX_MODULES_ENTRIES][MODULE_NAME_LEN];
    int count;
} LoadedModules;

typedef struct {
    char *name;
    char *category;
    char *bus;
    char *description;
} ModuleEntry;

typedef struct {
    char name[NET_IF_NAME_LEN];
    int up;
} NetIfEntry;



extern int ANY_PBR_LOADED;
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
extern int IS_NET_MODULES;
extern int IS_PBR_MODULES;
extern int IS_SND_MODULES;
extern ModuleEntry MODULES[MAX_MODULES_ENTRIES];
extern int MODULES_NO;
extern NetIfEntry NET_IFS[MAX_NET_IFS_ENTRIES];
extern int NET_IFS_NO;
static const char *PCMCIA_BRIDGES[] = {
    "i82092",
    "i82365",
    "pd6729",
    "tcic",
    "yenta_socket"
};
static const int PCMCIA_BRIDGES_LEN = sizeof(PCMCIA_BRIDGES) /
    sizeof(PCMCIA_BRIDGES[0]);



void applyFontColFiles(char*);
void applyFontColTtys(char*);
void applyVolume(int);
void getCurrRes(void);
int getHWVolume(int);
void getKernelVer(void);
LoadedModules getLoadedModules(void);
ModuleEntry *getModuleFromName(const char*);
void loadConf(void);
int loadConFonts(void);
int loadKeymaps(void);
int loadModules(void);
int loadNetIfs(void);
void saveDispRes(MenuItem, int);
void saveFontCol(MenuItem);
void saveFontPSF(MenuItem);
void saveKeymap(MenuItem);
void saveVolume(MenuItem);
void seedPCMCIASockets(void);
void showDispResMenu(void);
void showDriverCatsMenu(void);
void showDriversListMenu(const char*);
void showFontColMenu(void);
void showFontMenu(void);
void showFontPSFMenu(void);
void showGpmMenu(void);
void showGpmRespMenu(void);
void showGpmTypeMenu(void);
void showHelp(void);
void showKeymapMenu(void);
void showMainMenu(void);
void showMouseMenu(void);
void showNetDriversMenu(void);
void showNetManMenu(void);
void showNetSelectIfs(void);
void showPBrDriversMenu(void);
void showSndDriversMenu(void);
void showVolumeMenu(void);
void toggleDriver(const ModuleEntry*);
void toggleNetEnabled(char*);
void toggleNetIf(const char*);
void writeConf(void);

#endif
