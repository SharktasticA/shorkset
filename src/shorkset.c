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



#include "colours.h"
#include "general.h"
#include "shorkmenu.h"
#include "shorkset.h"
#include "vbe.h"

#include <fcntl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <math.h>
#include <linux/soundcard.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/utsname.h>



Config CONFIG = {
    3840,
    "white",
    "0;37",
    "default",
    "qwerty_en_us",
    "",
    0,
    "",
    40
};

char CONFONTS[MAX_FONTS][PATH_MAX] = {0};
int CONFONTS_COUNT = 0;
char KERNEL_VER[KERNEL_VER_LEN] = {0};
char KEYMAPS[MAX_KEYMAPS][PATH_MAX] = {0};
int KEYMAPS_COUNT = 0;
int IS_NET_MODULES = 0;
int IS_PBR_MODULES = 0;
int IS_SND_MODULES = 0;
ModuleEntry MODULES[MAX_MODULES_ENTRIES] = {0};
int MODULES_NO = 0;
NetIfEntry NET_IFS[MAX_NET_IFS_ENTRIES] = {0};
int NET_IFS_NO = 0;



/**
 * Applies the selected colour to core system files
 * @param ascii Selected colour's ANSI escape code value
 */
void applyFontColFiles(char *ansi)
{
    char cmd[256];

    // Write to /etc/profile
    snprintf(cmd, 256, "sed -i 's|\\\\033\\[[0-9;]*m|\\\\033[%sm|g' /etc/profile", ansi);
    system(cmd);

    // Write to etc/init.d/rc
    snprintf(cmd, 256, "sed -i 's|\\\\033\\[[0-9;]*m|\\\\033[%sm|g' /etc/init.d/rc", ansi);
    system(cmd);

    // Write to terminfo.src
    //snprintf(cmd, 256, "sed -i 's|\\\\E\\[[0-9;]*m|\\\\E[%sm|g' /usr/share/terminfo/src/terminfo.src", ascii);
    //system(cmd);

    // Rebuild terminfo
    //system("tic -x -1 -o /usr/share/terminfo /usr/share/terminfo/src/terminfo.src");
}

/**
 * Applies the selected colour to all virtual terminals
 * @param ascii Selected colour's ANSI escape code value
 */
void applyFontColTtys(char *ansi)
{
    // TODO: dynamically get number of TTYs
    // SHORK 486 always has 3 at least...
    for (int t = 1; t <= 3; t++)
    {
        char ttyPath[32];
        snprintf(ttyPath, sizeof(ttyPath), "/dev/tty%d", t);

        int tty = open(ttyPath, O_WRONLY | O_NOCTTY);
        if (tty < 0)
            continue;

        dprintf(tty, "\033[%sm", ansi);
        close(tty);
    }
}

/**
 * Applies the select volume level to /dev/mixer
 * @param level 
 */
void applyVolume(int level)
{
    level = getHWVolume(level);
    int mixerFD = open("/dev/mixer", O_WRONLY);
    if (mixerFD >= 0)
    {
        int vol = (level << 8) | level;
        if (ioctl(mixerFD, SOUND_MIXER_WRITE_VOLUME, &vol) < 0)
        {
            EXIT_MSG = strdup("ERROR: I/O error with SOUND_MIXER_WRITE_VOLUME when trying to apply new volume level");
            exit(1);
        }
        if (ioctl(mixerFD, SOUND_MIXER_WRITE_PCM, &vol) < 0)
        {
            EXIT_MSG = strdup("ERROR: I/O error with SOUND_MIXER_WRITE_PCM when trying to apply new volume level");
            exit(1);
        }
        close(mixerFD);
    }
    else
    {
        EXIT_MSG = strdup("ERROR: could not open /dev/mixer when trying to apply new volume level");
        exit(1);
    }
}

/** 
 * Gets the current resolution in SHORK's bootloader configuration file.
 */
void getCurrRes(void)
{
    CONFIG.dispRes = -1;

    // Find and select a bootloader cfg file
    const char *cfg = NULL;
    for (int i = 0; i < CFG_PATHS_LEN; i++)
    {
        if (access(CFG_PATHS[i], F_OK) == 0)
        {
            cfg = CFG_PATHS[i];
            break;
        }
    }

    // If no cfg found, time to exit...
    if (!cfg)
    {
        EXIT_MSG = strdup("ERROR: no valid bootloader configuration file was found");
        exit(1);
    }

    // Open stream to cfg
    FILE *stream = fopen(cfg, "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to open bootloader configuration file");
        exit(1);
    }

    // Find and extract the "vga=" parameter's values
    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, stream) != -1)
    {
        char *needle = strstr(line, "vga=");
        if (!needle)
            continue;

        char *val = needle + 4;
        if (strncmp(val, "ask", 3) == 0 || strncmp(val, "normal", 6) == 0)
            CONFIG.dispRes = 3840;
        else
            CONFIG.dispRes = (int)strtol(val, NULL, 10);
        break;
    }
    free(line);
    fclose(stream);

    // Catch if unsuccessful
    if (CONFIG.dispRes == -1)
    {
        EXIT_MSG = strdup("ERROR: unable to determine the current resolution from bootloader configuration file");
        exit(1);
    }
}

/**
 * Loads the Linux kernel version into KERNEL_VER.
 */
void getKernelVer(void)
{
    struct utsname un;
    if (uname(&un) != 0)
    {
        EXIT_MSG = strdup("ERROR: could not get Linux kernel version");
        exit(1);
    }

    strncpy(KERNEL_VER, un.release, KERNEL_VER_LEN - 1);
    KERNEL_VER[KERNEL_VER_LEN - 1] = '\0';
}

/**
 * Returns a list of loaded Linux modules.
 * @return Initialised LoadedModules struct containing an array of module
 *         names and how many were found.
 */
LoadedModules getLoadedModules(void)
{
    LoadedModules result;
    result.count = 0;

    FILE *stream = fopen("/proc/modules", "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: could not access /proc/modules");
        exit(1);
    }

    char buffer[PATH_MAX];
    while (fgets(buffer, PATH_MAX, stream))
    {
        if (result.count >= MAX_MODULES_ENTRIES)
            break;

        char name[128];
        if (sscanf(buffer, "%127s", name) != 1)
            continue;

        size_t len = strlen(name);
        if (len >= MODULE_NAME_LEN)
            continue;

        strcpy(result.modules[result.count], name);
        result.count++;
    }

    return result;
}

/**
 * Converts a human-readible volume percent (0-100%) to a value the OSS API
 * expects. 
 * @param pct Human-readible volume percentage (0-100)
 * @return Logarithmic volume level for OSS API
 */
int getHWVolume(int pct)
{
    if (pct <= 0)
        return 0;
    if (pct > 100)
        return 100;

    double hw = 100.0 + 12.5 * log2(pct / 100.0);
    if (hw < 50)
        hw = 50;
    return (int)(hw + 0.5);
}

/**
 * Loads shorkset.conf's values into CONFIG.
 */
void loadConf(void)
{
    FILE *stream = fopen(DOT_CONF, "r");
    if (stream)
    {
        // Buffer size has to be PATH_MAX for FONT_PSF and KEYMAP, and +15 to
        // take into account the variable and null terminator
        char buffer[PATH_MAX + 15];
        while (fgets(buffer, sizeof(buffer), stream))
        {
            buffer[strcspn(buffer, "\r\n")] = '\0';

            char *value = strchr(buffer, '=');
            if (!value)
                continue;

            value++;
            if (*value == '"')
            {
                value++;
                char *end = strrchr(value, '"');
                if (end) *end = '\0';
            }
   
            if (strncmp(buffer, "DISP_RES=", 9) == 0)
                CONFIG.dispRes = atoi(value);
            else if (strncmp(buffer, "FONT_COL_NAME=", 14) == 0)
                snprintf(CONFIG.fontColName, CONFIG_FONT_COL_NAME_LEN, "%s", value);
            else if (strncmp(buffer, "FONT_COL_ANSI=", 14) == 0)
                snprintf(CONFIG.fontColANSI, CONFIG_FONT_COL_ANSI_LEN, "%s", value);
            else if (strncmp(buffer, "FONT_PSF=", 9) == 0)
                snprintf(CONFIG.fontPSF, PATH_MAX, "%s", value);
            else if (strncmp(buffer, "KEYMAP=", 7) == 0)
                snprintf(CONFIG.keymap, PATH_MAX, "%s", value);
            else if (strncmp(buffer, "MODULES=", 7) == 0)
                snprintf(CONFIG.modules, CONFIG_MODULES_LEN, "%s", value);
            else if (strncmp(buffer, "NET_ENABLED=", 12) == 0)
                CONFIG.netEnabled = atoi(value);
            else if (strncmp(buffer, "NET_IFS=", 8) == 0)
                snprintf(CONFIG.netIfs, CONFIG_NET_IFS_LEN, "%s", value);
            else if (strncmp(buffer, "VOLUME=", 7) == 0)
                CONFIG.volume = atoi(value);
        }
        fclose(stream);

        // Validate integer values and reset to default if needed
        if (CONFIG.dispRes < 0)
            CONFIG.dispRes = 3840;
        if (CONFIG.netEnabled != 0 && CONFIG.netEnabled != 1)
            CONFIG.netEnabled = 0;
        if (CONFIG.volume < 0 || CONFIG.volume > 100)
            CONFIG.volume = 40;
    }
    else
    {
        size_t extMsgLen = 48 + strlen(DOT_CONF);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to load %s", DOT_CONF);
        exit(1);
    }
}

/**
 * Loads the contents of the CONFONTS_DIR directory.
 * @return 1 if CONFONTS_DIR exists and not empty; 0 if doesn't exist or is
 *         empty
 */
int loadConFonts(void)
{
    struct stat st;
    if (stat(CONFONTS_DIR, &st) != 0 || !S_ISDIR(st.st_mode))
        return 0;

    DIR *dir = opendir(CONFONTS_DIR);
    if (!dir)
    {
        size_t extMsgLen = 48 + strlen(CONFONTS_DIR);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: could not access %s", CONFONTS_DIR);
        exit(1);
    }

    CONFONTS_COUNT = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (CONFONTS_COUNT >= MAX_FONTS)
            break;
            
        snprintf(CONFONTS[CONFONTS_COUNT], PATH_MAX, "%s/%s", CONFONTS_DIR, entry->d_name);
        CONFONTS_COUNT++;
    }
    closedir(dir);

    qsort(CONFONTS, CONFONTS_COUNT, PATH_MAX, natCmp);
    return CONFONTS_COUNT > 0;
}

/**
 * Loads the contents of the KEYMAPS_DIR directory.
 * @return 1 if KEYMAPS_DIR exists and not empty; 0 if doesn't exist or is
 *         empty
 */
int loadKeymaps(void)
{
    struct stat st;
    if (stat(KEYMAPS_DIR, &st) != 0 || !S_ISDIR(st.st_mode))
        return 0;

    DIR *dir = opendir(KEYMAPS_DIR);
    if (!dir)
    {
        size_t extMsgLen = 48 + strlen(KEYMAPS_DIR);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: could not access %s", KEYMAPS_DIR);
        exit(1);
    }

    KEYMAPS_COUNT = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (KEYMAPS_COUNT >= MAX_KEYMAPS)
            break;
            
        snprintf(KEYMAPS[KEYMAPS_COUNT], PATH_MAX, "%s/%s", KEYMAPS_DIR, entry->d_name);
        KEYMAPS_COUNT++;
    }
    closedir(dir);

    qsort(KEYMAPS, KEYMAPS_COUNT, PATH_MAX, natCmp);
    return KEYMAPS_COUNT > 0;
}

/**
 * Loads the contents of modules.csv into MODULES.
 * @returns 1 if successful; 0 if not
 */
int loadModules(void)
{
    // Load csv file
    FILE *stream;
    if (fileExists(MODULES_CSV_PATH))
        stream = fopen(MODULES_CSV_PATH, "r");
    else
        return 0;

    // Load csv into buffer
    static char buffer[CSV_BUFFER];
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, stream);
    fclose(stream);
    buffer[n] = '\0';

    char *p = buffer;

    // Skip header line
    while (*p && *p != '\n')
        p++;
    if (*p == '\n')
        p++;

    MODULES_NO = 0;
    while (*p && MODULES_NO < MAX_MODULES_ENTRIES)
    {
        char *line = p;

        // Find end of line
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
        {
            *p = '\0';
            p++;
        }

        if (*line == '\0')
            continue;

        // Load line
        char *fields[5];
        int fieldCount = loadCSVLine(line, fields, 5);

        // Check if malformed line/parsing
        if (fieldCount < 5)
            continue;

        char modulePath[PATH_MAX];
        snprintf(modulePath, PATH_MAX, "%s/%s/%s", MODULES_DIR, KERNEL_VER,
            fields[1]);

        if (fileExists(modulePath))
        {
            // Input line into entries
            MODULES[MODULES_NO].name = fields[0];
            MODULES[MODULES_NO].category = fields[2];
            MODULES[MODULES_NO].bus = fields[3];
            MODULES[MODULES_NO].description = fields[4];

            // Flag which driver categories were sound
            if (!IS_NET_MODULES && strcmp(fields[2], "net") == 0)
                IS_NET_MODULES = 1;
            else if (!IS_PBR_MODULES && strcmp(fields[2], "pbr") == 0)
                IS_PBR_MODULES = 1;
            else if (!IS_SND_MODULES && strcmp(fields[2], "snd") == 0)
                IS_SND_MODULES = 1;
            MODULES_NO++;
        }

    }

    return 1;
}

/**
 * Loads a list of network interfaces and their up/down status in NET_IFS.
 * @returns 1 if successful; 0 if not
 */
int loadNetIfs(void)
{
    struct ifaddrs *ifs;
    struct ifaddrs *currIF;

    // Make sure NET_IFS is empty if refreshing
    if (NET_IFS_NO > 0)
    {
        memset(NET_IFS, 0, sizeof(NET_IFS));
        NET_IFS_NO = 0;
    }

    if (getifaddrs(&ifs) == -1)
        return 0;

    for (currIF = ifs; currIF && NET_IFS_NO < MAX_NET_IFS_ENTRIES;
        currIF = currIF->ifa_next)
    {
        if (!currIF->ifa_name)
            continue;

        // Skip loopback
        if (strcmp(currIF->ifa_name, "lo") == 0)
            continue;

        // Duplicate check
        int isDup = 0;
        for (int i = 0; i < NET_IFS_NO; i++)
        {
            if (strncmp(NET_IFS[i].name, currIF->ifa_name,
                NET_IF_NAME_LEN) == 0)
            {
                isDup = 1;
                break;
            }
        }
        if (isDup)
            continue;

        // Copy if name
        strncpy(NET_IFS[NET_IFS_NO].name, currIF->ifa_name,
            NET_IF_NAME_LEN - 1);
        NET_IFS[NET_IFS_NO].name[NET_IF_NAME_LEN - 1] = '\0';

        // Set up/down status
        NET_IFS[NET_IFS_NO].up = (currIF->ifa_flags & IFF_UP) ? 1 : 0;

        NET_IFS_NO++;
    }
    freeifaddrs(ifs);

    return 1;
}

/**
 * Saves the selected resolution to SHORK's bootloader configuration file.
 * @param itm Selected resolution's menu item
 * @param skipMsg Flags if the end message screen should be skipped
 */
void saveDispRes(MenuItem itm, int skipMsg)
{
    // Find and select a bootloader cfg file
    const char *cfg = NULL;
    for (int i = 0; i < CFG_PATHS_LEN; i++)
    {
        if (access(CFG_PATHS[i], F_OK) == 0)
        {
            cfg = CFG_PATHS[i];
            break;
        }
    }

    // If no cfg found, time to exit...
    if (!cfg)
    {
        EXIT_MSG = strdup("ERROR: no valid bootloader configuration file was found");
        exit(1);
    }

    // Open stream to cfg
    FILE *stream = fopen(cfg, "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to open bootloader configuration file");
        exit(1);
    }

    // Get cfg's contents length
    fseek(stream, 0, SEEK_END);
    size_t size = ftell(stream);
    rewind(stream);

    // Read cfg into buffer
    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, stream);
    buffer[size] = '\0';
    fclose(stream);

    // Ensure vga param is in the buffer
    char *vgaNeedle = strstr(buffer, "vga=");
    if (!vgaNeedle)
    {
        EXIT_MSG = strdup("ERROR: bootloader configuration missing the \"vga\" kernel parameter");
        free(buffer);
        exit(1);
    }

    // Get end of where we need to patch
    char *end = vgaNeedle + 4;
    while (*end && *end != ' ' && *end != '\n')
        end++;

    // Assemble the patch and result string
    char mode[5];
    int modeLen = snprintf(mode, 5, "%s", itm.id);
    size_t patchedSize = size - (end - (vgaNeedle + 4)) + modeLen;
    char *result = malloc(patchedSize + 1);
    size_t prefix = vgaNeedle + 4 - buffer;

    // Make the patch
    memcpy(result, buffer, prefix);
    memcpy(result + prefix, mode, modeLen);
    strcpy(result + prefix + modeLen, end);
    free(buffer);

    // Save the new cfg
    stream = fopen(cfg, "w");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to write bootloader configuration file");
        free(result);
        exit(1);
    }
    fwrite(result, 1, patchedSize, stream);

    fclose(stream);
    free(result);
    CONFIG.dispRes = atoi(itm.id);
    // If a VGA resolution was selected, we also have to reset the PSF font to
    // "default" since they normally dictate their own VGA resolution
    if (!skipMsg && CONFIG.dispRes >= 3840)
        snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "default");
    writeConf();

    if (!skipMsg)
    {
        char msgTitle[MENU_ITEM_NAME_LEN];
        snprintf(msgTitle, MENU_ITEM_NAME_LEN, "%s", itm.name);
        char msgBody[320] = "The selected display resolution has been saved. If you selected a VGA resolution and had selected a PSF font before, the latter setting will now be discarded as PSF fonts dictate their own VGA resolution. A system restart is required before the changes will take effect.";

        WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col, NULL, 0,
            0);
        printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
        free(wrapped->str);
        free(wrapped);
    }
}

/**
 * Saves the selected font colour to shorkset.conf and applies it.
 * @param itm Selected font colour's menu item
 */
void saveFontCol(MenuItem itm)
{
    applyFontColFiles(itm.id);
    applyFontColTtys(itm.id);

    snprintf(CONFIG.fontColName, sizeof(CONFIG.fontColName), "%s", itm.payload);
    snprintf(CONFIG.fontColANSI, sizeof(CONFIG.fontColANSI), "%s", itm.id);
    writeConf();

    char msgTitle[MENU_ITEM_NAME_LEN];
    snprintf(msgTitle, MENU_ITEM_NAME_LEN, "%s", itm.payload);
    char msgBody[320] = "The selected font colour has been saved and will be applied once you exit SHORKSET. If there are any other active virtual terminals (ttyX), you may need to enter \"exit\" when convenient, or restart your computer before this change will take complete effect.";

    WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col, NULL, 0, 0);
    printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
    free(wrapped->str);
    free(wrapped);
}

/**
 * Saves the selected PSF font to shorkset.conf and applies it.
 * @param itm Selected PSF font's menu item
 */
void saveFontPSF(MenuItem itm)
{   
    if (strcmp(itm.id, "default") == 0)
    {
        snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "default");
        writeConf();
        
        char msgTitle[MENU_ITEM_NAME_LEN] = "default";
        char msgBody[320] = "The PSF font will be reset to default. If a PSF font other than \"default\" was previously selected, you must restart your computer before this change will take effect.";

        WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col, NULL, 0,
            0);
        printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
        free(wrapped->str);
        free(wrapped);
    }
    else
    {
        char cmd[PATH_MAX + 9];
        snprintf(cmd, sizeof(cmd), "setfont %s", itm.payload);
        system(cmd);
        setupViewport();

        snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "%s", itm.payload);
        // If VGA font, we need to reset to default since PSF fonts override VGA
        // resolutions
        if (CONFIG.dispRes >= 3840)
            saveDispRes((MenuItem){ "3840", "", "", NULL, 1 }, 1);
        // saveDispRes will call writeConf anyway, only needed if not resetting
        // display resolution
        else
            writeConf();

        char msgTitle[MENU_ITEM_NAME_LEN];
        snprintf(msgTitle, MENU_ITEM_NAME_LEN, "%s", itm.name);
        char msgBody[320] = "The selected PSF font has been saved and will be applied once you exit SHORKSET. If you had selected a VGA display resolution before, that setting will now be discarded as the PSF font will dictate its own VGA resolution. VBE display resolutions are unaffected.";

        WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col, NULL, 0,
            0);
        printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
        free(wrapped->str);
        free(wrapped);
    }
}

/**
 * Saves the keyboard layout to shorkset.conf and applies it.
 * @param itm Selected keyboard layout's menu item
 */
void saveKeymap(MenuItem itm)
{   
    char cmd[PATH_MAX + 12];
    snprintf(cmd, sizeof(cmd), "loadkmap < %s", itm.payload);
    system(cmd);

    snprintf(CONFIG.keymap, sizeof(CONFIG.keymap), "%s", itm.id);
    writeConf();

    char msgTitle[MENU_ITEM_NAME_LEN];
    snprintf(msgTitle, MENU_ITEM_NAME_LEN, "%s", itm.name);
    char msgBody[320] = "The selected keyboard layout has been applied.";

    WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col, NULL, 0, 0);
    printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
    free(wrapped->str);
    free(wrapped);
}

/**
 * Saves the selected volume level to shorkset.conf and applies it.
 * @param itm Selected volume level's menu item
 */
void saveVolume(MenuItem itm)
{
    CONFIG.volume = atoi(itm.name);
    applyVolume(CONFIG.volume);
    writeConf();

    /*char msgTitle[MENU_ITEM_NAME_LEN];
    snprintf(msgTitle, MENU_ITEM_NAME_LEN, "%s", itm.name);
    char msgBody[320] = "The selected volume level has been applied.";

    WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col, NULL, 0, 0);
    printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
    free(wrapped->str);
    free(wrapped);*/
}

/**
 * Displays display resolution selection menu
 */
void showDispResMenu(void)
{
    MenuItem rawMenu[] = {
        { "",       "VGA modes (col x row)", NULL, NULL, 1, 1 },
        { "3840",   "80x25", NULL, NULL, 1, 0 },
        { "3843",   "80x28", NULL, NULL, 1, 0 },
        { "3846",   "80x34", NULL, NULL, 1, 0 },
        { "3842",   "80x43", NULL, NULL, 1, 0 },
        { "3841",   "80x50", NULL, NULL, 1, 0 },
        { "3847",   "80x60", NULL, NULL, 1, 0 },
#ifdef FB
        { "",       "VBE resolutions (width x height x bits)", NULL, NULL, 1, 1 },
        { "782",    "320x200x16", NULL, NULL, 1, 0 },
        { "783",    "320x200x24", NULL, NULL, 1, 0 },

        { "768",    "640x400x8", NULL, NULL, 1, 0 },
        { "802",    "640x400x16", NULL, NULL, 1, 0 },
        { "803",    "640x400x24", NULL, NULL, 1, 0 },

        { "769",    "640x480x8", NULL, NULL, 1, 0 },
        { "785",    "640x480x16", NULL, NULL, 1, 0 },
        { "786",    "640x480x24", NULL, NULL, 1, 0 },

        { "770",    "800x600x4", NULL, NULL, 1, 0 },
        { "771",    "800x600x8", NULL, NULL, 1, 0 },
        { "788",    "800x600x16", NULL, NULL, 1, 0 },
        { "789",    "800x600x24", NULL, NULL, 1, 0 },

        { "772",    "1024x768x4", NULL, NULL, 1, 0 },
        { "773",    "1024x768x8", NULL, NULL, 1, 0 },
        { "791",    "1024x768x16", NULL, NULL, 1, 0 },
        { "792",    "1024x768x24", NULL, NULL, 1, 0 },

        { "774",    "1280x1024x4", NULL, NULL, 1, 0 },
        { "775",    "1280x1024x8", NULL, NULL, 1, 0 },
        { "794",    "1280x1024x16", NULL, NULL, 1, 0 },
        { "795",    "1280x1024x24", NULL, NULL, 1, 0 }
#endif
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int menuSize = 0;
#ifdef FB
    vbeInit();
#endif
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].isStatic || countSubstrs(rawMenu[i].name, "x") == 1 || vgaModeAvailable(atoi(rawMenu[i].id)))
            menu[menuSize++] = rawMenu[i];
#ifdef FB
    vbeShutdown();
#endif
    if (menuSize == 8)
        menuSize--;

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    // Mark the current resolution
    for (int i = 0; i < menuSize; i++)
    {
        if (atoi(menu[i].id) == CONFIG.dispRes)
        {
            strcat(menu[i].name, "*");
            cursorY = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select display resolution");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                saveDispRes(menu[cursorY - 1], 0);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (atoi(menu[i].id) == CONFIG.dispRes)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursorY = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

/**
 * Displays driver category selection menu
 */
void showDriverCatsMenu(void)
{
    MenuItem rawMenu[] = {
        {
            "net", "Network interface", NULL, showNetDriversMenu,
            IS_NET_MODULES, 0
        },
        {
            "pbr", "PCMCIA bridge", NULL, showPBrDriversMenu,
            IS_PBR_MODULES, 0
        },
        {
            "snd", "Sound card", NULL, showSndDriversMenu,
            IS_SND_MODULES, 0
        }
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int menuSize = 0;
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].isVisible)
            menu[menuSize++] = rawMenu[i];
    freeMenu(rawMenu, rawMenuSize);

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select driver category");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6,
                menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6,
                menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                menu[cursorY - 1].action();
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

/**
 * Displays drivers list selection menu
 * @param cat Driver category to filter the given list to
 */
void showDriversListMenu(const char *cat)
{
    // Create a menu containing modules for the given cat
    MenuItem menu[MODULES_NO];
    int menuSize = 0;
    for (int i = 0; i < MODULES_NO; i++)
    {
        if (strcmp(MODULES[i].category, cat) == 0)
        {
            snprintf(menu[menuSize].id, sizeof(menu[i].id), "%s",
                MODULES[i].name);
            snprintf(menu[menuSize].name, sizeof(menu[i].name), "%s (%s)",
                MODULES[i].description, MODULES[i].bus);
            menu[menuSize].payload = NULL;
            menu[menuSize].action = NULL;
            menu[menuSize].isVisible = 1;
            menu[menuSize].isStatic = 0;
            menuSize++;
        }
    }

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    char title[32] = {0};
    char msg[200] = {0};
    if (strcmp(cat, "net") == 0)
    {
        snprintf(title, 32, "Toggle network interface driver");
        snprintf(msg, 200, "Select one or more drivers to load or unload "
            "their Linux modules. Any driver marked in green with a "
            "leading \"*\" is currently loaded.");
    }
    else if (strcmp(cat, "pbr") == 0)
    {
        snprintf(title, 32, "Toggle PCMCIA bridge driver");
        snprintf(msg, 200, "Select a driver to load or unload its Linux "
            "module. The driver marked in green with a leading \"*\" is "
            "currently loaded.");
    }
    else if (strcmp(cat, "snd") == 0)
    {
        snprintf(title, 32, "Toggle sound card driver");
        snprintf(msg, 200, "Select a driver to load or unload its Linux "
            "module. The driver marked in green with a leading \"*\" is "
            "currently loaded.");
    }

    while (running)
    {
        if (fullRedraw)
        {
            // Mark/unmark the loaded modules
            LoadedModules loaded = getLoadedModules();
            for (int i = 0; i < menuSize; i++)
            {
                int marked = 0;
                for (int j = 0; j < loaded.count; j++)
                {
                    if (strcmp(menu[i].id, loaded.modules[j]) == 0)
                    {
                        markEntry(menu[i].name, COL_FOR_GREEN, 0);
                        marked = 1;
                        break;
                    }
                }

                if (!marked)
                    unmarkEntry(menu[i].name);
            }

            clearScreen();
            printHeader(title);
            printMenu(menu, menuSize, msg, 1, TERM_SIZE.ws_col - 6,
                menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Toggle [q] Quit");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, msg, 1, TERM_SIZE.ws_col - 6,
                menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                toggleDriver(menu[cursorY - 1].id);
                // If toggling a networking driver, update NET_IFS
                if (strcmp(cat, "net") == 0)
                    loadNetIfs();
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    freeMenu(menu, menuSize);
    clearScreen();
}

/**
 * Displays font colour selection menu
 */
void showFontColMenu(void)
{
    MenuItem menu[] = {
        { "0;34",   "\033[0;34mBlue\033[0m",                "blue",             NULL,   1 },
        { "0;1;34", "\033[0;1;34mBright blue\033[0m",       "blue_bright",      NULL,   1 },
        { "0;36",   "\033[0;36mCyan\033[0m",                "cyan",             NULL,   1 },
        { "0;1;36", "\033[0;1;36mBright cyan\033[0m",       "cyan_bright",      NULL,   1 },
        { "0;32",   "\033[0;32mGreen\033[0m",               "green",            NULL,   1 },
        { "0;1;32", "\033[0;1;32mBright green\033[0m",      "green_bright",     NULL,   1 },
        { "0;1;30", "\033[0;1;30mGrey\033[0m",              "grey",             NULL,   1 },
        { "0;35",   "\033[0;35mMagenta\033[0m",             "magenta",          NULL,   1 },
        { "0;1;35", "\033[0;1;35mBright magenta\033[0m",    "magneta_bright",   NULL,   1 },
        { "0;31",   "\033[0;31mRed\033[0m",                 "red",              NULL,   1 },
        { "0;1;31", "\033[0;1;31mBright red\033[0m",        "red_bright",       NULL,   1 },
        { "0;37",   "\033[0;37mWhite\033[0m",               "white",            NULL,   1 },
        { "0;1;37", "\033[0;1;37mBright white\033[0m",      "white_bright",     NULL,   1 },
        { "0;33",   "\033[0;33mYellow\033[0m",              "yellow",           NULL,   1 },
        { "0;1;33", "\033[0;1;33mBright yellow\033[0m",     "yellow_bright",    NULL,   1 },
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].payload, CONFIG.fontColName) == 0)
        {
            strcat(menu[i].name, "*");
            cursorY = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select font colour");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                saveFontCol(menu[cursorY - 1]);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].payload, CONFIG.fontColName) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursorY = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

/**
 * Displays PSF font selection menu
 */
void showFontPSFMenu(void)
{
    MenuItem menu[MAX_FONTS + 1];
    menu[0] = (MenuItem){ "default", "Default", "default", NULL, 1 };
    int menuSize = 1;
    
    for(int i = 0; i < CONFONTS_COUNT; i++)
    {
        char *base = basename(CONFONTS[i]);

        // Strip extension
        char nameStr[129];
        strncpy(nameStr, base, 128);
        nameStr[128] = '\0';
        char *dot = strrchr(nameStr, '.');
        if (dot)
            *dot = '\0';

        // Trim to 100 chars and add "..." if needed
        if (strlen(nameStr) > 100)
        {
            nameStr[100] = '\0';
            strcat(nameStr, "...");
        }

        // Add font
        snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%s", nameStr);
        snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "%s", nameStr);
        menu[menuSize].payload = strdup(CONFONTS[i]);
        menu[menuSize].action = NULL;
        menu[menuSize].isVisible = 1;
        menu[menuSize].isStatic = 0;
        menuSize++;
    }

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].payload, CONFIG.fontPSF) == 0)
        {
            strcat(menu[i].name, "*");
            cursorY = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select font (PSF)");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                saveFontPSF(menu[cursorY - 1]);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].payload, CONFIG.fontPSF) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursorY = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

void showHelp(void)
{
    TERM_SIZE = getTerminalSize();

    char desc[160] = "A settings program for changing the display resolution, keyboard layout (keymap), system sound volume, terminal PSF font, and terminal font colour.\n";
    formatNewLines(desc, TERM_SIZE.ws_col, NULL, 0);
    printf("%s\n", desc);

    char usage[50] = "Usage: shorkset [OPTIONS]\n\n";
    formatNewLines(usage, TERM_SIZE.ws_col, NULL, 0);
    printf("%s", usage);

    char help[70] = "-h, --help     Displays help information and exits\n";
    formatNewLines(help, TERM_SIZE.ws_col, "               ", 0);
    printf("%s", help);

    char version[100] = "-v, --version  Displays version number and exits\n";
    formatNewLines(version, TERM_SIZE.ws_col, "               ", 0);
    printf("%s", version);

    if (access("/dev/dsp", F_OK) == 0)
    {
        char volume[110] = "-vl, --volume  Set a volume level between 0 and 100; no assignment returns the current level\n";
        formatNewLines(volume, TERM_SIZE.ws_col, "               ", 0);
        printf("%s", volume);
    }
}

/**
 * Displays keyboard layout selection menu
 */
void showKeymapMenu(void)
{
    MenuItem menu[MAX_KEYMAPS + 1];
    int menuSize = 0;
    for(int i = 0; i < KEYMAPS_COUNT; i++)
    {
        char *base = basename(KEYMAPS[i]);

        // Strip extension
        char nameStr[129];
        strncpy(nameStr, base, 128);
        nameStr[128] = '\0';
        char *dot = strrchr(nameStr, '.');
        if (dot)
        {
            *dot = '\0';
            dot = strrchr(nameStr, '.');
            if (dot && (strcmp(dot, ".kmap") == 0 || strcmp(dot, ".bin") == 0))
                *dot = '\0';
        }

        // Trim to 100 chars and add "..." if needed
        if (strlen(nameStr) > 100)
        {
            nameStr[100] = '\0';
            strcat(nameStr, "...");
        }

        // Add font
        snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%s", nameStr);
        snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "%s", nameStr);
        menu[menuSize].payload = strdup(KEYMAPS[i]);
        menu[menuSize].action = NULL;
        menu[menuSize].isVisible = 1;
        menu[menuSize].isStatic = 0;
        menuSize++;
    }

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].id, CONFIG.keymap) == 0)
        {
            strcat(menu[i].name, "*");
            cursorY = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select keyboard layout");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                saveKeymap(menu[cursorY - 1]);
                // Update marked item
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].id, CONFIG.keymap) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursorY = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;
        
            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

/**
 * Displays SHORKSET' main menu
 */
void showMainMenu(void)
{
    setupMenuSys();

    loadConf();
    // Whilst loadConf should provide the current resolution, we will still
    // check the bootloader conf in case of a manual edit
    getCurrRes();
    getKernelVer();
    loadModules();
    loadNetIfs();

    MenuItem rawMenu[] = {
        { 
            "res",
            "Display resolution",
            "",
            showDispResMenu,
            1
        },
        {
            "drv",
            "Drivers",
            "",
            showDriverCatsMenu,
            IS_NET_MODULES || IS_SND_MODULES
        },
        {
            "lay", 
            "Keyboard layout",
            "",
            showKeymapMenu,
            loadKeymaps()
        },
        {
            "psf",
            "Font (PSF)",
            "",
            showFontPSFMenu,
            loadConFonts()
        },
        {
            "col",
            "Font colour",
            "",
            showFontColMenu,
            1
        },
        {
            "net",
            "Network",
            "",
            showNetManMenu,
            isProgramInstalled("ifconfig", 1)
        },
        {
            "vol",
            "Volume",
            "",
            showVolumeMenu,
            access("/dev/dsp", F_OK) == 0
        }
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int menuSize = 0;
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].isVisible)
            menu[menuSize++] = rawMenu[i];

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("SHORKSET");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Quit");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                menu[cursorY - 1].action();
                fullRedraw = 1;
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

void showNetDriversMenu(void)
{
    showDriversListMenu("net");
}

/**
 * Shows the network (manager) menu.
 */
void showNetManMenu(void)
{
    MenuItem rawMenu[] = {
        {
            "enb", "Enable networking", NULL, NULL, 1
        },
        {
            "aif", "Select interfaces", NULL, showNetSelectIfs,
            NET_IFS_NO > 0
        }
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int realMenuSize = 0;
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].isVisible)
            menu[realMenuSize++] = rawMenu[i];
    freeMenu(rawMenu, rawMenuSize);

    // The menu size used in the menu loop that can be =1 if networking is
    // disabled
    int currMenuSize = realMenuSize;
    if (!CONFIG.netEnabled)
        currMenuSize = 1;

    // Set enable/disable networking item's default value
    if (CONFIG.netEnabled)
    {
        snprintf(menu[0].name, MENU_ITEM_NAME_LEN, "Disable networking");
        markEntry(menu[0].name, COL_FOR_GREEN, 1);
    }
    else
    {
        snprintf(menu[0].name, MENU_ITEM_NAME_LEN, "Enable networking");
        markEntry(menu[0].name, COL_FOR_RED, 1);
    }

    char msg[200] = {0};
    if (NET_IFS_NO == 0)
    {
        snprintf(msg, 200, "No network interfaces were found. Please "
            "ensure your network controllers are properly installed and "
            "that you have selected the correct drivers in the \"Drivers\" "
            "menu.");
    }

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            // Show/hide full menu depending on CONFIG.netEnabled
            if (CONFIG.netEnabled)
                currMenuSize = realMenuSize;
            else
                currMenuSize = 1;

            clearScreen();
            printHeader("Network");
            printMenu(menu, currMenuSize, msg, 1, TERM_SIZE.ws_col - 6,
                currMenuSize, &cursorX, &cursorY, &cursorXPrev,
                &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, currMenuSize, msg, 1, TERM_SIZE.ws_col - 6,
                currMenuSize, &cursorX, &cursorY, &cursorXPrev,
                &cursorYPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorYPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorYPrev = cursorY;
                cursorY--;
                if (cursorY < 1) cursorY = currMenuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorYPrev = cursorY;
                cursorY++;
                if (cursorY > currMenuSize) cursorY = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                if (cursorY == 1)
                    toggleNetEnabled(menu[cursorY - 1].name);
                else
                    menu[cursorY - 1].action();
                fullRedraw = 1;
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

/**
 * Shows the select network interfaces menu.
 */
void showNetSelectIfs(void)
{
    // Create a menu containing modules for the given cat
    MenuItem menu[NET_IFS_NO];
    int menuSize = 0;
    for (int i = 0; i < NET_IFS_NO; i++)
    {
        snprintf(menu[menuSize].id, sizeof(menu[menuSize].id), "%s",
            NET_IFS[i].name);

        if (strncmp(NET_IFS[i].name, "eth", 3) == 0 ||
            strncmp(NET_IFS[i].name, "enp", 3) == 0 ||
            strncmp(NET_IFS[i].name, "ens", 3) == 0 ||
            strncmp(NET_IFS[i].name, "eno", 3) == 0)
            snprintf(menu[menuSize].name, sizeof(menu[menuSize].name),
                "Ethernet (%s)", NET_IFS[i].name);
        else if (strncmp(NET_IFS[i].name, "wlan", 4) == 0 ||
            strncmp(NET_IFS[i].name, "wlx", 3) == 0)
            snprintf(menu[menuSize].name, sizeof(menu[menuSize].name),
                "Wireless (%s)", NET_IFS[i].name);
        else
            snprintf(menu[menuSize].name, sizeof(menu[menuSize].name), "%s",
                NET_IFS[i].name);

        menu[menuSize].payload = NULL;
        menu[menuSize].action = NULL;
        menu[menuSize].isVisible = 1;
        menu[menuSize].isStatic = 0;
        menuSize++;
    }

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    char msg[200] = "Select one or more network interfaces to activate or "
        "deactivate them. Any interface marked in green with a leading "
        "\"*\" currently activated.";

    while (running)
    {
        if (fullRedraw)
        {
            // Mark/unmark the active interfaces
            for (int i = 0; i < menuSize; i++)
            {
                if (NET_IFS[i].up)
                    markEntry(menu[i].name, COL_FOR_GREEN, 0);
                else
                    unmarkEntry(menu[i].name);
            }

            clearScreen();
            printHeader("Available interfaces");
            printMenu(menu, menuSize, msg, 1, TERM_SIZE.ws_col - 6,
                menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, msg, 1, TERM_SIZE.ws_col - 6,
                menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                toggleNetIf(menu[cursorY - 1].id);
                fullRedraw = 1;
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

void showPBrDriversMenu(void)
{
    showDriversListMenu("pbr");
}

void showSndDriversMenu(void)
{
    showDriversListMenu("snd");
}

/**
 * Displays System volume selection menu
 */
void showVolumeMenu(void)
{
    MenuItem menu[] = {
        { "0",      "0",    "", NULL,   1   },
        { "10",     "10",   "", NULL,   1   },
        { "20",     "20",   "", NULL,   1   },
        { "30",     "30",   "", NULL,   1   },
        { "40",     "40",   "", NULL,   1   },
        { "50",     "50",   "", NULL,   1   },
        { "60",     "60",   "", NULL,   1   },
        { "70",     "70",   "", NULL,   1   },
        { "80",     "80",   "", NULL,   1   },
        { "90",     "90",   "", NULL,   1   },
        { "100",    "100",  "", NULL,   1   }
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    int running = 1;
    int cursorX = 1;
    int cursorY = 1;
    int cursorXPrev = 1;
    int cursorYPrev = 0;
    int fullRedraw = 1;

    // Mark the current volume
    char vol[12];
    snprintf(vol, 12, "%d", CONFIG.volume);
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].id, vol) == 0)
        {
            strcat(menu[i].name, "*");
            cursorY = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select volume");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, NULL, 1, TERM_SIZE.ws_col - 6, menuSize, &cursorX, &cursorY, &cursorXPrev, &cursorYPrev);
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
                saveVolume(menu[cursorY - 1]);
                // Update marked item
                snprintf(vol, 12, "%d", CONFIG.volume);
                for (int i = 0; i < menuSize; i++)
                {
                    size_t len = strlen(menu[i].name);
                    if (strcmp(menu[i].id, vol) == 0)
                    {
                        // Add "*" only if not already present
                        if (len == 0 || menu[i].name[len - 1] != '*')
                        {
                            strcat(menu[i].name, "*");
                            cursorY = i + 1;
                        }
                    }
                    else
                    {
                        // Remove trailing "*" if present
                        if (len > 0 && menu[i].name[len - 1] == '*')
                            menu[i].name[len - 1] = '\0';
                    }
                }
                fullRedraw = 1;
                break;

            case QUIT:
                running = 0;
                break;

            case INVALID:
                fullRedraw = 0;
                break;

            case CURSOR_LEFT:
            case CURSOR_RIGHT:
                break;
        }
    }

    clearScreen();
}

/**
 * Loads/unloads the given driver's Linux module id depending on if its
 * unloaded/loaded.
 * @param id ID for Linux module to toggle
 */
void toggleDriver(const char *id)
{
    // Check if module is loaded
    LoadedModules modules = getLoadedModules();
    int loaded = 0;
    for (int i = 0; i < modules.count; i++)
    {
        if (strcmp(id, modules.modules[i]) == 0)
        {
            loaded = 1;
            break;
        }
    }

    tcflush(STDIN_FILENO, TCIFLUSH);

    if (!loaded)
    {
        char dlgMsg[256];
        snprintf(dlgMsg, 256, "The Linux module for the %s driver is being "
            "loaded. Please wait.", id);
        showDialog(dlgMsg, 50);
        sleep(1);

        // Load driver's module via modprobe
        int result = runCmd("modprobe", id, NULL);
        if (result != 0)
        {
            tcflush(STDIN_FILENO, TCIFLUSH);

            char msgTitle[MENU_ITEM_NAME_LEN];
            snprintf(msgTitle, MENU_ITEM_NAME_LEN, "Could not load driver");
            char msgBody[480] = "The selected driver's Linux module could "
                "not be loaded. This likely means the hardware it targets "
                "is not present or addressable, and the module could not "
                "load without it. Please ensure the target hardware is "
                "properly connected when it is safe to do so, or that you "
                "are trying the correct driver.";

            WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col,
                NULL, 0, 0);
            printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
            free(wrapped->str);
            free(wrapped);

            return;
        }
        csvAppend(CONFIG.modules, CONFIG_MODULES_LEN, id);
    }
    else
    {
        char dlgMsg[256];
        snprintf(dlgMsg, 256, "The Linux module for the %s driver is being "
            "unloaded. Please wait.", id);
        showDialog(dlgMsg, 50);
        sleep(1);

        // Always remove module from CONFIG.modules so if it fails to unload
        // now, we know it won't be loaded next boot
        csvRemove(CONFIG.modules, id);

        // Unload driver's module via rmmod
        int result = runCmd("rmmod", id, NULL);
        if (result != 0)
        {
            tcflush(STDIN_FILENO, TCIFLUSH);
    
            char msgTitle[MENU_ITEM_NAME_LEN];
            snprintf(msgTitle, MENU_ITEM_NAME_LEN,
                "Could not unload driver");
            char msgBody[480] = "The selected driver's Linux module could "
                "not be unloaded immediately. This could be due to the "
                "driver being in use. It has been removed from SHORKSET's "
                "saved configuration so that it will not be loaded upon "
                "reboot.";

            WORD_WRAPPED *wrapped = wordWrap(msgBody, TERM_SIZE.ws_col,
                NULL, 0, 0);
            printTextScreen(msgTitle, wrapped->str, wrapped->lines, 1);
            free(wrapped->str);
            free(wrapped);
        }
    }

    writeConf();
    tcflush(STDIN_FILENO, TCIFLUSH);
}

/**
 * Toggles the value of CONFIG.netEnabled and writes it, then modifies the
 * toggling menu item to convey the new status.
 * @param name Name of the menu item that toggled this
 */
void toggleNetEnabled(char *name)
{
    CONFIG.netEnabled = !CONFIG.netEnabled;
    writeConf();
    // TODO: confirmation screen?

    if (CONFIG.netEnabled)
    {
        snprintf(name, MENU_ITEM_NAME_LEN, "Disable networking");
        markEntry(name, COL_FOR_GREEN, 1);
    }
    else
    {
        snprintf(name, MENU_ITEM_NAME_LEN, "Enable networking");
        markEntry(name, COL_FOR_RED, 1);
    }
}

/**
 * Toggles the up (activated)/down (deactivated) status of the given network
 * interface to its opposite value.
 * @param id Network interface's menu item ID
 */
void toggleNetIf(const char *id)
{
    // Get pointer to the interface to operate on
    NetIfEntry *netIf = NULL;
    for(int i = 0; i < NET_IFS_NO; i++)
    {
        if(strcmp(NET_IFS[i].name, id) == 0)
        {
            netIf = &NET_IFS[i];
            break;
        }
    }
    if (!netIf)
    {
        EXIT_MSG = strdup("ERROR: could not find network interface");
        exit(1);
    }

    // Show notice dialog
    char dlgMsg[256];
    if (!netIf->up)
        snprintf(dlgMsg, 256, "Activating %s. Please wait.", id);
    else
        snprintf(dlgMsg, 256, "Deactivating %s. Please wait.", id);
    showDialog(dlgMsg, 50);
    sleep(1);

    // Get socket for operations
    int fdSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (fdSocket < 0)
    {
        EXIT_MSG = strdup("ERROR: could not create socket for network "
            "interface operations");
        exit(1);
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, netIf->name, IFNAMSIZ);

    // Read current flags
    if (ioctl(fdSocket, SIOCGIFFLAGS, &ifr) < 0)
    {
        close(fdSocket);
        EXIT_MSG = strdup("ERROR: could not read network interface flags "
            "from the system");
        exit(1);
    }

    // If netIf is currently down, set it up
    if (!netIf->up)
        ifr.ifr_flags |= IFF_UP;
    // ...and vice versa
    else
        ifr.ifr_flags &= ~IFF_UP;
    netIf->up = !netIf->up;

    // Write flags back
    if (ioctl(fdSocket, SIOCSIFFLAGS, &ifr) < 0)
    {
        close(fdSocket);
        EXIT_MSG = strdup("ERROR: could not write network interface flags "
            "back to the system");
        exit(1);
    }
    close(fdSocket);

    // If the interface is now "up", run DHCP so the connection can be
    // immediately available
    if (netIf->up)
    {
        char carrierPath[PATH_MAX];
        snprintf(carrierPath, PATH_MAX, "/sys/class/net/%s/carrier",
            netIf->name);

        int carrierReady = 0;
        // Make sure carrier is ready (and wait for up to 10 secs if not)
        for (int i = 0; i < 100; i++)
        {
            FILE *carrierFile = fopen(carrierPath, "r");
            if (carrierFile)
            {
                char carrierStatus = fgetc(carrierFile);
                fclose(carrierFile);
                if (carrierStatus == '1')
                {
                    carrierReady = 1;
                    break;
                }
            }
            usleep(100000);
        }
        if (!carrierReady)
        {
            EXIT_MSG = strdup("ERROR: network interface carrier was not "
                "ready before timeout");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            EXIT_MSG = strdup("ERROR: could not fork udhcpc process");
            exit(1);
        }

        if (pid == 0)
        {
            int ifNull = open("/dev/null", O_RDWR);
            if (ifNull < 0)
            {
                EXIT_MSG = strdup("ERROR: could not redirect udhcpc output "
                    "to /dev/null");
                exit(1);
            }

            dup2(ifNull, STDOUT_FILENO);
            dup2(ifNull, STDERR_FILENO);
            close(ifNull);

            execlp("udhcpc", "udhcpc", "-i", netIf->name, "-n", "-t", "3",
                "-T", "2", "-s", UDHCPC_DEFAULT_SCRIPT, (char *)NULL);

            _exit(1);
            EXIT_MSG = strdup("ERROR: could not run udhcpc");
            exit(1);
        }
    }

    // Now that we know the system changes worked, we can write the changes
    if (netIf->up)
        csvAppend(CONFIG.netIfs, CONFIG_NET_IFS_LEN, id);
    else
        csvRemove(CONFIG.netIfs, id);
    writeConf();
}

/**
 * Writes CONFIG's current values into shorkset.conf.
 */
void writeConf(void)
{
    FILE *stream = fopen(DOT_CONF, "w");
    if (!stream)
    {
        size_t extMsgLen = 48 + strlen(DOT_CONF);
        EXIT_MSG = malloc(extMsgLen);
        snprintf(EXIT_MSG, extMsgLen, "ERROR: failed to write %s", DOT_CONF);
        exit(1);
    }

    fprintf(stream, "DISP_RES=%d\n", CONFIG.dispRes);
    fprintf(stream, "FONT_COL_NAME=\"%s\"\n", CONFIG.fontColName);
    fprintf(stream, "FONT_COL_ANSI=\"%s\"\n", CONFIG.fontColANSI);
    fprintf(stream, "FONT_PSF=\"%s\"\n", CONFIG.fontPSF);
    fprintf(stream, "KEYMAP=\"%s\"\n", CONFIG.keymap);
    fprintf(stream, "MODULES=\"%s\"\n", CONFIG.modules);
    fprintf(stream, "NET_ENABLED=%d\n", CONFIG.netEnabled);
    fprintf(stream, "NET_IFS=\"%s\"\n", CONFIG.netIfs);
    fprintf(stream, "VOLUME=%d\n", CONFIG.volume);
    fclose(stream);
    sync();
}
