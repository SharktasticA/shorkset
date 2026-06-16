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



static const char *VERSION = "0.1-wip";



#include "shorkset.h"

#include <stdio.h>
#include <string.h>



int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0))
        {
            showHelp();
            return 0;
        }
        else if ((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--version") == 0))
        {
            printf("SHORKSET %s\n", VERSION);
            return 0;
        }
        else
        {
            printf("ERROR: unrecognised option \"%s\"\n", argv[i]);
            return 1;
        }
    }

    showMainMenu();
    return 0;
}
