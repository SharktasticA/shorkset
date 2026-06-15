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



#include "general.h"
#include "shorkmenu.h"
#include "shorkres.h"

#include <sys/ioctl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



int CURR_RES = 3840;



/** 
 * Gets the current resolution in SHORK's bootloader configuration file.
 */
void getCurrRes(void)
{
    CURR_RES = -1;

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
            CURR_RES = 3840;
        else
            CURR_RES = (int)strtol(val, NULL, 10);
        break;
    }
    free(line);
    fclose(stream);

    // Catch if unsuccessful
    if (CURR_RES == -1)
    {
        EXIT_MSG = strdup("ERROR: unable to determine the current resolution from bootloader configuration file\n");
        exit(1);
    }
}

/**
 * Saves the selected resolution to SHORK's bootloader configuration file.
 * @param itm Selected resolution's menu item
 */
void saveRes(MenuItem itm)
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
    char mode[4];
    int modeLen = snprintf(mode, 4, "%d", atoi(itm.id));
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
    CURR_RES = atoi(itm.id);

    char postTitle[80];
    snprintf(postTitle, 80, "Saved: %s", itm.name);
    char postBody[256] = "The selected resolution has been saved. A system restart is required before the changes take effect.";
    formatNewLines(postBody, TERM_SIZE.ws_col, NULL, 0);
    printGenericScreen(postTitle, postBody);
}

/**
 * Displays SHORKRES' main menu (selects between VGA and VESA resolutions)
 */
void showMainMenu(void)
{
    setupMenuSys();
    getCurrRes();

    if (TERM_SIZE.ws_col < 40 || TERM_SIZE.ws_row < 10)
    {
        printf("ERROR: terminal size too small (must be 40x10 or larger)\n");
        return;
    }

    MenuItem rawMenu[] = {
        { "vga",    "Select VGA resolution",    showVGAResMenu,     1 },
        { "vesa",   "Select VESA resolution",   showVESAResMenu,    1 }
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
            printHeader("SHORKRES");
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
 * Displays VESA resolution selection menu
 */
void showVESAResMenu(void)
{
    MenuItem menu[] = {
        { "782",    "320x200 (CGA)",        NULL,   1 },
        { "785",    "640x480 (VGA)",        NULL,   1 },
        { "788",    "800x600 (SVGA)",       NULL,   1 },
        { "791",    "1024x768 (XGA)",       NULL,   1 },
        { "794",    "1280x1024 (SXGA)",     NULL,   1 },
        { "837",    "1400x1050 (SXGA+)",    NULL,   1 },
        { "838",    "1600x1200 (UXGA)",     NULL,   1 }
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    // Mark the current resolution
    for (int i = 0; i < menuSize; i++)
    {
        if (atoi(menu[i].id) == CURR_RES)
        {
            strcat(menu[i].name, "*");
            break;
        }
    }

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select VESA resolution (pixels)");
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
                saveRes(menu[cursor - 1]);
                running = 0;
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
 * Displays VGA resolution selection menu
 */
void showVGAResMenu(void)
{
    MenuItem menu[] = {
        { "3840",   "80x25",    NULL,   1 },
        { "3843",   "80x28",    NULL,   1 },
        { "3844",   "80x30",    NULL,   1 },
        { "3846",   "80x34",    NULL,   1 },
        { "3842",   "80x43",    NULL,   1 },
        { "3841",   "80x50",    NULL,   1 },
        { "3847",   "80x60",    NULL,   1 }
    };
    int menuSize = sizeof(menu) / sizeof(menu[0]);

    // Mark the current resolution
    for (int i = 0; i < menuSize; i++)
    {
        if (atoi(menu[i].id) == CURR_RES)
        {
            strcat(menu[i].name, "*");
            break;
        }
    }

    int running = 1;
    int cursor = 1;
    int cursorPrev = 0;
    int fullRedraw = 1;

    while (running)
    {
        if (fullRedraw)
        {
            clearScreen();
            printHeader("Select VGA resolution (characters)");
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
                saveRes(menu[cursor - 1]);
                running = 0;
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
