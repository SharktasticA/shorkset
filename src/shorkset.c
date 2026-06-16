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
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



Config CONFIG =  { 3840, "white", "0;37", "default" };



/**
 * Apply the selected colour to core system files
 * @param ascii Selected colour's ANSI escape code value
 */
void applyColourFiles(char *ansi)
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
 * Apply the selected colour to all virtual terminals
 * @param ascii Selected colour's ANSI escape code value
 */
void applyColourTtys(char *ansi)
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
        EXIT_MSG = strdup("ERROR: no valid bootloader configuration file was found\n");
        exit(1);
    }

    // Open stream to cfg
    FILE *stream = fopen(cfg, "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to open bootloader configuration file\n");
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
        EXIT_MSG = strdup("ERROR: unable to determine the current resolution from bootloader configuration file\n");
        exit(1);
    }
}

/**
 * Loads shorkset.conf's values into CONFIG.
 */
void loadConf(void)
{
    FILE *stream = fopen(DOT_CONF, "r");
    if (stream)
    {
        // Buffer size has to be PATH_MAX for FONT_PSF, and +15 to take into
        // account the variable and null terminator
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
 * Saves the selected resolution to SHORK's bootloader configuration file.
 * @param itm Selected resolution's menu item
 */
void saveDispRes(MenuItem itm)
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
        EXIT_MSG = strdup("ERROR: no valid bootloader configuration file was found\n");
        exit(1);
    }

    // Open stream to cfg
    FILE *stream = fopen(cfg, "r");
    if (!stream)
    {
        EXIT_MSG = strdup("ERROR: failed to open bootloader configuration file\n");
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
        EXIT_MSG = strdup("ERROR: bootloader configuration missing the \"vga\" kernel parameter\n");
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
        EXIT_MSG = strdup("ERROR: failed to write bootloader configuration file\n");
        free(result);
        exit(1);
    }
    fwrite(result, 1, patchedSize, stream);

    fclose(stream);
    free(result);
    CONFIG.dispRes = atoi(itm.id);
    writeConf();

    char postTitle[80];
    snprintf(postTitle, 80, "%s", itm.name);
    char postBody[320] = "The selected display resolution has been saved. A system restart is required before the changes will take effect.";
    formatNewLines(postBody, TERM_SIZE.ws_col, NULL, 0);
    printGenericScreen(postTitle, postBody);
}

/**
 * Saves the selected font colour to TODO
 * @param itm Selected font colour's menu item
 */
void saveFontCol(MenuItem itm)
{
    applyColourFiles(itm.id);
    applyColourTtys(itm.id);

    snprintf(CONFIG.fontColName, sizeof(CONFIG.fontColName), "%s", itm.payload);
    snprintf(CONFIG.fontColANSI, sizeof(CONFIG.fontColANSI), "%s", itm.id);
    writeConf();

    char postTitle[80];
    snprintf(postTitle, 80, "%s", itm.payload);
    char postBody[320] = "The selected font colour has been saved and will be applied once you exit SHORKSET. If there are any other active virtual terminals (ttyX), you may need to enter \"exit\" when convenient, or restart your computer before this change will take complete effect.";
    formatNewLines(postBody, TERM_SIZE.ws_col, NULL, 0);
    printGenericScreen(postTitle, postBody);
}

/**
 * Displays display resolution selection menu
 */
void showDispResMenu(void)
{
    MenuItem menu[] = {
        { "3840",   "VGA: 80x25",               "", NULL,   1 },
        { "3843",   "VGA: 80x28",               "", NULL,   1 },
        { "3846",   "VGA: 80x34",               "", NULL,   1 },
        { "3842",   "VGA: 80x43",               "", NULL,   1 },
        { "3841",   "VGA: 80x50",               "", NULL,   1 },
        { "3847",   "VGA: 80x60",               "", NULL,   1 },
#ifdef FB
        { "782",    "VBE: 320x200 (CGA)",       "", NULL,   1 },
        { "785",    "VBE: 640x480 (VGA)",       "", NULL,   1 },
        { "788",    "VBE: 800x600 (SVGA)",      "", NULL,   1 },
        { "791",    "VBE: 1024x768 (XGA)",      "", NULL,   1 },
        { "794",    "VBE: 1280x1024 (SXGA)",    "", NULL,   1 },
        { "837",    "VBE: 1400x1050 (SXGA+)",   "", NULL,   1 },
        { "838",    "VBE: 1600x1200 (UXGA)",    "", NULL,   1 },
#endif
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    // Mark the current resolution
    for (int i = 0; i < menuSize; i++)
    {
        if (atoi(menu[i].id) == CONFIG.dispRes)
        {
            strcat(menu[i].name, "*");
            cursor = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select display resolution");
            printMenu(menu, menuSize, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                saveDispRes(menu[cursor - 1]);
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
                            cursor = i + 1;
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
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    // Mark the current colour
    for (int i = 0; i < menuSize; i++)
    {
        if (strcmp(menu[i].payload, CONFIG.fontColName) == 0)
        {
            strcat(menu[i].name, "*");
            cursor = i + 1;
            break;
        }
    }

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select font colour");
            printMenu(menu, menuSize, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Back");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                saveFontCol(menu[cursor - 1]);
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
                            cursor = i + 1;
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

    char desc[150] = "A settings program for changing a SHORK Operating System's display resolution, the terminal's PSF font, and the terminal's font colour.\n";
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
}

/**
 * Displays SHORKSET' main menu
 */
void showMainMenu(void)
{
    setupMenuSys();

    if (TERM_SIZE.ws_col < 40 || TERM_SIZE.ws_row < 10)
    {
        printf("ERROR: terminal size too small (must be 40x10 or larger)\n");
        return;
    }

    loadConf();
    // Whilst loadConf should provide the current resolution, we will still
    // check the bootloader conf in case of a manual edit
    getCurrRes();

    MenuItem rawMenu[] = {
        { "res",    "Display resolution",   "", showDispResMenu,    1 },
        { "col",    "Font colour",          "", showFontColMenu,    1 }
    };
    int rawMenuSize = sizeof(rawMenu) / sizeof(rawMenu[0]);

    // Filter menu to just what should actually be visible
    MenuItem menu[rawMenuSize];
    int menuSize = 0;
    for (int i = 0; i < rawMenuSize; i++)
        if (rawMenu[i].visible)
            menu[menuSize++] = rawMenu[i];

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("SHORKSET");
            printMenu(menu, menuSize, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
            printFooter("[jk] Navigate [Enter] Select [q] Quit");
        }
        else
        {
            if (COL_ENABLED)
                printf("\x1b[2;1H");
            else
                printf("\x1b[3;1H");
            printMenu(menu, menuSize, 1, TERM_SIZE.ws_col - 6, menuSize, 1, cursor, 1, cursorPrev);
        }

        NavInput input = getNavInput();

        fullRedraw = 1;
        cursorPrev = 0;
        switch (input)
        {
            case CURSOR_UP:
                cursorPrev = cursor;
                cursor--;
                if (cursor < 1) cursor = menuSize;
                fullRedraw = 0;
                break;

            case CURSOR_DOWN:
                cursorPrev = cursor;
                cursor++;
                if (cursor > menuSize) cursor = 1;
                fullRedraw = 0;
                break;

            case ENTER:
                clearScreen();
                menu[cursor - 1].action();
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
    fclose(stream);
}
