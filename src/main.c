/*
    ######################################################
    ##             SHORK UTILITY - SHORKSET             ##
    ######################################################
    ## A settings program for SHORK Operating Systems   ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



static const char *VERSION = "1.0-wip";



#include "shorkset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showHelp();
            return 1;
        }
        else if ((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--version") == 0))
        {
            printf("SHORKSET %s\n", VERSION);
            return 1;
        }
        else if ((strncmp(argv[i], "-vl", 3) == 0 || strncmp(argv[i], "--volume", 8) == 0) && access("/dev/dsp", F_OK) == 0)
        {
            loadConf();
            // Find "=" as our needle
            char *equalsNeedle = strchr(argv[i], '=');
            if (!equalsNeedle)
            {
                printf("%d\n", CONFIG.volume);
                return 1;
            }

            equalsNeedle++;
            for (char *p = equalsNeedle; *p; p++)
            {
                if (*p < '0' || *p > '9')
                {
                    printf("ERROR: volume level must be numeric\n");
                    return 2;
                }
            }

            int level = atoi(equalsNeedle);
            if (level < 0 || level > 100)
            {
                printf("ERROR: volume level must be between 0 and 100\n");
                return 2;
            }

            CONFIG.volume = atoi(equalsNeedle);
            applyVolume(CONFIG.volume);
            writeConf();
            return 1;
        }
        else
        {
            printf("ERROR: unrecognised option \"%s\"\n", argv[i]);
            return 2;
        }
    }

    showMainMenu();
    return 0;
}
