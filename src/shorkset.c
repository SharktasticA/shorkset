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



#include "general.h"
#include "shorkmenu.h"
#include "shorkset.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <math.h>
#include <linux/soundcard.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



Config CONFIG = { 3840, "white", "0;37", "default", "en_us", 40 };



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
                snprintf(CONFIG.fontColName, sizeof(CONFIG.fontColName), "%s", value);
            else if (strncmp(buffer, "FONT_COL_ANSI=", 14) == 0)
                snprintf(CONFIG.fontColANSI, sizeof(CONFIG.fontColANSI), "%s", value);
            else if (strncmp(buffer, "FONT_PSF=", 9) == 0)
                snprintf(CONFIG.fontPSF, sizeof(CONFIG.fontPSF), "%s", value);
            else if (strncmp(buffer, "KEYMAP=", 7) == 0)
                snprintf(CONFIG.keymap, sizeof(CONFIG.keymap), "%s", value);
            else if (strncmp(buffer, "VOLUME=", 7) == 0)
                CONFIG.volume = atoi(value);
        }
        fclose(stream);
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
        if (KEYMAPS_COUNT >= MAX_FONTS)
            break;
            
        snprintf(KEYMAPS[KEYMAPS_COUNT], PATH_MAX, "%s/%s", KEYMAPS_DIR, entry->d_name);
        KEYMAPS_COUNT++;
    }
    closedir(dir);

    qsort(KEYMAPS, KEYMAPS_COUNT, PATH_MAX, natCmp);
    return KEYMAPS_COUNT > 0;
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
        char msgTitle[80];
        snprintf(msgTitle, 80, "%s", itm.name);
        char msgBody[320] = "The selected display resolution has been saved. If you selected a VGA resolution and had selected a PSF font before, the latter setting will now be discarded as PSF fonts dictate their own VGA resolution. A system restart is required before the changes will take effect.";
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(msgTitle, msgBody, lines, 1);
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

    char msgTitle[80];
    snprintf(msgTitle, 80, "%s", itm.payload);
    char msgBody[320] = "The selected font colour has been saved and will be applied once you exit SHORKSET. If there are any other active virtual terminals (ttyX), you may need to enter \"exit\" when convenient, or restart your computer before this change will take complete effect.";
    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen(msgTitle, msgBody, lines, 1);
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
        
        char msgTitle[80] = "default";
        char msgBody[320] = "The PSF font will be reset to default. If a PSF font other than \"default\" was previously selected, you must restart your computer before this change will take effect.";
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(msgTitle, msgBody, lines, 1);
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

        char msgTitle[80];
        snprintf(msgTitle, 80, "%s", itm.name);
        char msgBody[320] = "The selected PSF font has been saved and will be applied once you exit SHORKSET. If you had selected a VGA display resolution before, that setting will now be discarded as the PSF font will dictate its own VGA resolution. VBE display resolutions are unaffected.";
        int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
        printTextScreen(msgTitle, msgBody, lines, 1);
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

    char msgTitle[80];
    snprintf(msgTitle, 80, "%s", itm.name);
    char msgBody[320] = "The selected keyboard layout has been applied.";
    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen(msgTitle, msgBody, lines, 1);
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

    /*char msgTitle[80];
    snprintf(msgTitle, 80, "%s", itm.name);
    char msgBody[320] = "The selected volume level has been applied.";
    int lines = formatNewLines(msgBody, TERM_SIZE.ws_col, NULL, 0);
    printTextScreen(msgTitle, msgBody, lines, 1);*/
}

/**
 * Displays display resolution selection menu
 */
void showDispResMenu(void)
{
    MenuItem menu[] = {
        { "3840",   "VGA: 80x25", NULL, NULL, 1, 0 },
        { "3843",   "VGA: 80x28", NULL, NULL, 1, 0 },
        { "3846",   "VGA: 80x34", NULL, NULL, 1, 0 },
        { "3842",   "VGA: 80x43", NULL, NULL, 1, 0 },
        { "3841",   "VGA: 80x50", NULL, NULL, 1, 0 },
        { "3847",   "VGA: 80x60", NULL, NULL, 1, 0 },
#ifdef FB
        { "782",    "VBE: 320x200 (CGA)", NULL, NULL, 1, 0 },
        { "785",    "VBE: 640x480 (VGA)", NULL, NULL, 1, 0 },
        { "788",    "VBE: 800x600 (SVGA)", NULL, NULL, 1, 0 },
        { "791",    "VBE: 1024x768 (XGA)", NULL, NULL, 1, 0 },
        { "794",    "VBE: 1280x1024 (SXGA)" ,NULL, NULL, 1, 0 },
        { "837",    "VBE: 1400x1050 (SXGA+)", NULL, NULL, 1, 0 },
        { "838",    "VBE: 1600x1200 (UXGA)", NULL, NULL, 1, 0 }
#endif
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

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
        }
    }

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
        snprintf(menu[menuSize].payload, sizeof(menu[menuSize].payload), "%s", CONFONTS[i]);
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
        snprintf(menu[menuSize].payload, sizeof(menu[menuSize].payload), "%s", KEYMAPS[i]);
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

    if (TERM_SIZE.ws_col < 40 || TERM_SIZE.ws_row < 10)
    {
        EXIT_MSG = "ERROR: terminal size too small (must be 40x10 or larger)\n";
        exit(1);
    }

    loadConf();
    // Whilst loadConf should provide the current resolution, we will still
    // check the bootloader conf in case of a manual edit
    getCurrRes();

    MenuItem rawMenu[] = {
        { "res",    "Display resolution",   "", showDispResMenu,    1                               },
        { "kmp",    "Keyboard layout",      "", showKeymapMenu,     loadKeymaps()                   },
        { "psf",    "Font (PSF)",           "", showFontPSFMenu,    loadConFonts()                  },
        { "col",    "Font colour",          "", showFontColMenu,    1                               },
        { "vol",    "Volume",               "", showVolumeMenu,     access("/dev/dsp", F_OK) == 0   }
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
        }
    }

    clearScreen();
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
        }
    }

    clearScreen();
}

/**
 * Writes CONFIG's current values to the shorkset.conf.
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
    fprintf(stream, "VOLUME=%d\n", CONFIG.volume);
    fclose(stream);
    sync();
}
