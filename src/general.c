/*
    ######################################################
    ##             SHORK UTILITY - SHORKRES             ##
    ######################################################
    ## General, utility functions to be used throughout ##
    ## SHORKRES                                         ##
    ######################################################
    ## Licence: GNU GENERAL PUBLIC LICENSE Version 3    ##
    ######################################################
    ## Kali (links.sharktastica.co.uk)                  ##
    ######################################################
*/



#include "colours.h"
#include "general.h"
#include "shorkmenu.h"

#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>



/**
 * Converts a data value into a string formatted into a unit that makes sense for
 * its magnitude with its new unit added to the end.
 * @param from Unit the input value is in (e.g., "B", "KiB")
 * @param val Input value to convert
 * @return String containing the converted value and its new unit (e.g., "1.5MiB")
 */
char *bytesToReadable(const char *from, const long long val)
{
    long long bytes = val;
    if (strcmp(from, "KiB") == 0)
        bytes *= 1024;
    else if (strcmp(from, "MiB") == 0)
        bytes *= 1024LL * 1024;
    else if (strcmp(from, "GiB") == 0)
        bytes *= 1024LL * 1024 * 1024;
    else if (strcmp(from, "TiB") == 0)
        bytes *= 1024LL * 1024 * 1024 * 1024;

    const long long TiB = 1024LL * 1024 * 1024 * 1024;
    const long long GiB = 1024LL * 1024 * 1024;
    const long long MiB = 1024LL * 1024;
    const long long KiB = 1024LL;

    const int resultSize = 32;
    char *result = malloc(resultSize);
    if (!result) return strdup("");
    long long whole, remainder;
    int decimal;

    if (bytes >= TiB)
    {
        whole = bytes / TiB;
        remainder = bytes % TiB;

        if (COMPACT)
        {
            if (remainder > 0) whole++;
            snprintf(result, resultSize, "%lldT", whole);
            return result;
        }

        decimal = (int)((remainder * 10 + TiB / 2) / TiB);
        if (decimal == 10) { whole++; decimal = 0; }
        if (decimal == 0) snprintf(result, resultSize, "%lldTiB", whole);
        else snprintf(result, resultSize, "%lld.%dTiB", whole, decimal);
    }
    else if (bytes >= GiB)
    {
        whole = bytes / GiB;
        remainder = bytes % GiB;
        
        if (COMPACT)
        {
            if (remainder > 0) whole++;
            snprintf(result, resultSize, "%lldG", whole);
            return result;
        }

        decimal = (int)((remainder * 10 + GiB / 2) / GiB);
        if (decimal == 10) { whole++; decimal = 0; }
        if (decimal == 0) snprintf(result, resultSize, "%lldGiB", whole);
        else snprintf(result, resultSize, "%lld.%dGiB", whole, decimal);
    }
    else if (bytes >= MiB)
    {
        whole = bytes / MiB;
        remainder = bytes % MiB;
        
        if (COMPACT)
        {
            if (remainder > 0) whole++;
            snprintf(result, resultSize, "%lldM", whole);
            return result;
        }

        decimal = (int)((remainder * 10 + MiB / 2) / MiB);
        if (decimal == 10) { whole++; decimal = 0; }
        if (decimal == 0) snprintf(result, resultSize, "%lldMiB", whole);
        else snprintf(result, resultSize, "%lld.%dMiB", whole, decimal);
    }
    else if (bytes >= KiB)
    {
        whole = bytes / KiB;
        remainder = bytes % KiB;
        
        if (COMPACT)
        {
            if (remainder > 0) whole++;
            snprintf(result, resultSize, "%lldK", whole);
            return result;
        }

        decimal = (int)((remainder * 10 + KiB / 2) / KiB);
        if (decimal == 10) { whole++; decimal = 0; }
        if (decimal == 0) snprintf(result, resultSize, "%lldKiB", whole);
        else snprintf(result, resultSize, "%lld.%dKiB", whole, decimal);
    }
    else
        snprintf(result, resultSize, "%lldB", bytes);

    return result;
}

/**
 * Extracts a substring from an input string after a given separation character
 * and offset. Also removes any surrounding quotes or trailing newline characters
 * present. 
 * @param input Input string
 * @param point Character to find to separate from (e.g., '=' or ':')
 * @param offset How many characters after the point to separate at
 * @param inputSize Size to use when allocating the result string
 * @return String containing what's left after separation and cleaning
 */
char *extractFromPoint(char *input, size_t inputSize, char point, int offset)
{
    if (!input || inputSize < 2) return strdup("");

    // Prepare result string
    char *result = malloc(inputSize);
    if (!result) return strdup("");
    result[0] = '\0';

    // Find our separation point in the input string
    char *sep = strchr(input, point);
    if (!sep) return result;

    // Our start position taking into account possible offset
    char *start = sep + offset;

    // Trim potential leading double quote
    if (*start == '"') start++;

    // Copy everything after the start position into our result
    strncpy(result, start, inputSize - 1);
    result[inputSize - 1] = '\0';
    size_t len = strlen(result);

    // Trim potential trailing newline 
    if (len > 0 && result[len - 1] == '\n')
        result[--len] = '\0';

    // Trim potential trailing double quote
    if (len > 0 && result[len - 1] == '"')
        result[len - 1] = '\0';

    return result;
}

/**
 * Finds and erases a desired substring from an input string.
 * @param input Input string
 * @param inputSize Size to use when allocating the result string
 * @param needle Substring to find and erase
 * @return String containing what's left after erasing
 */
char *findErase(const char *input, const size_t inputSize, const char *needle)
{
    if (!input || !needle || inputSize < 2) return strdup("");

    size_t needleLen = strlen(needle);
    if (needleLen == 0) return strdup("");

    // Prepare result string
    char *result = malloc(inputSize);
    if (!result) return strdup("");

    // Copy input string to result
    strncpy(result, input, inputSize);
    result[inputSize - 1] = '\0';

    // Go through the string looking for our needle(s)... When found, we move the rest
    // of the string over and on top of said needles
    char *pos = result;
    while ((pos = strstr(pos, needle)) != NULL)
    {
        size_t tailLen = strlen(pos + needleLen);
        memmove(pos, pos + needleLen, tailLen + 1);
    }

    return result;
}

/**
 * Finds and replaces a given search term with a desired replacement term from an
 * input string.
 * @param input Input string
 * @param inputSize Size to use when allocating the result string
 * @param needle Substring to find and replace
 * @param replacement New string to insert
 * @return String after term replacement
 */
char *findReplace(const char *input, const size_t inputSize, const char *needle, const char *replacement)
{
    if (!input || !needle || !replacement || inputSize < 2) return strdup("");

    size_t needleLen = strlen(needle);
    size_t replacementLen = strlen(replacement);
    if (needleLen == 0) return strdup("");

    // Prepare result string
    char *result = malloc(inputSize);
    if (!result) return strdup("");

    // Copy input string to result
    strncpy(result, input, inputSize);
    result[inputSize - 1] = '\0';

    char *pos = result;
    while ((pos = strstr(pos, needle)) != NULL)
    {
        size_t tailLen = strlen(pos + needleLen);

        // If replacement is larger than our needle, realloc memory to avoid overflowing
        if (replacementLen > needleLen)
        {
            size_t currentLen = strlen(result);
            size_t newLen = currentLen + (replacementLen - needleLen) + 1;
            char *tmp = realloc(result, newLen);
            if (!tmp) break;
            pos = tmp + (pos - result);
            result = tmp;
        }

        // Move the trailing text to accomodate the new size and paste our replacement into
        // the 'gap'
        memmove(pos + replacementLen, pos + needleLen, tailLen + 1);
        memcpy(pos, replacement, replacementLen);
        pos += replacementLen;
    }

    return result;
}

/**
 * Adds new lines to a given string based on the requested line width.
 * @param input Input string
 * @param width Characters per line
 * @param indent Indent to include after newly inserted new line
 * @param trim Flags that any trailing newlines should be removed
 * @return Number of lines in the string
 */
int formatNewLines(char *input, int width, char *indent, int trim)
{
    if (!input || width < 1) return 0;

    // Initialse variables that help us track progress
    size_t inputStrLen = strlen(input);
    size_t indentLen = indent ? strlen(indent) : 0;
    int lines = 1;
    int lastSpace = -1;
    int widthCount = 1;

    // Iterate through the input string to find line breaks or places to add new ones
    for (int i = 0; i < inputStrLen; i++)
    {
        if (input[i] == '\033')
        {
            while (i < inputStrLen && input[i] != 'm') i++;
            if (i >= inputStrLen) break;
            continue; 
        }
        
        // Track where the last space was in case so we can go back for a future word wrap
        if (input[i] == ' ') lastSpace = i;
        // Reset tracking and take into account if we find an existing new line
        else if (input[i] == '\n')
        {
            lines++;
            widthCount = 0;
            continue;
        }

        // Begin word wrapping once the line width is saturated
        if (widthCount == width)
        {
            if (lastSpace != -1)
            {
                input[lastSpace] = '\n';
                lines++;

                if (indent && indentLen > 0)
                {
                    memmove(input + lastSpace + 1 + indentLen, input + lastSpace + 1, inputStrLen - lastSpace);
                    memcpy(input + lastSpace + 1, indent, indentLen);
                    inputStrLen += indentLen;
                    if (lastSpace <= i) i += indentLen;
                }
            }
            widthCount = i - lastSpace;
        }

        widthCount++;
    }

    // If desired, strip possible trailing new line
    if (trim)
    {
        int end = strlen(input) - 1;
        while (end >= 0 && input[end] == '\n')
        {
            input[end] = '\0';
            end--;
            lines--;
        }
    }

    return lines;
}

/**
 * Calculates the square root of a given number (and helps us avoid including
 * math.h) - float variant.
 * @param x Input value
 * @returns Square root of the input value; -1 if imaginary/invalid
 */
float fSqrt(float x)
{
    // Return -1 to flag imaginary result
    if (x < 0.0) return -1;
    // sqrt(0 or 1) = number itself anyway
    if (x == 0.0 || x == 1.0) return x;

    float result = x;
    float last = 0.0;

    // Newton–Raphson...
    for (int i = 0; i < 20; i++)
    {
        last = result;
        result = 0.5 * (result + x / result);

        // Get out once we're stable
        if (result == last) break;
    }

    return result;
}

/**
 * Gets an integer input from the user.
 * @param prompt Prompt to give the user 
 * @param min Minimum allowed input (set to same as max to disable validation)
 * @param max Maximum allowed input (set to same as min to disable validation)
 * @param negativeIfInvalid Flags if function should return -1 if invalid input instead of looping
 * @return User's integer input
 */
int getIntInput(char *prompt, int min, int max, int negativeIfInvalid)
{
    if (min > max) min = max;
    if (max < min) max = min;

    int isValid;
    char buffer[32];
    int val;

    do
    {
        disableRawMode();

        if (COL_ENABLED)
        {
            printf("\033[%s;%sm", COL_FOR_BOLD_WHITE, COL_BAK_BLUE);
            for (int i = 0; i < TERM_SIZE.ws_col; i++) printf(" ");
            printf("\033[1G");
        }

        printf("%s", prompt);
        if (min != max) printf(" (%d-%d)", min, max);
        printf(": ");

        if (fgets(buffer, sizeof(buffer), stdin) != NULL)
        {
            if (sscanf(buffer, "%d", &val) == 1)
            {
                if ((min != max) && (val >= min && val <= max))
                    isValid = 1;
                else if (min == max)
                    isValid = 1;
                else
                    isValid = 0;
            }
            else isValid = 0;
        }
        else
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            isValid = 0;
        }

        enableRawMode();

        if (!isValid && negativeIfInvalid)
            return -1;

    } while (!isValid);

    if (COL_ENABLED) printf("\033[%sm", COL_RESET);

    return val;
}

/**
 * Gets the parent process ID (PPID) and name of a given process ID (PID).
 * @param pid The input PID
 * @return Process struct with the found PPID and name; pid is -1 if something went wrong
 */
Process getParentProcess(int pid)
{
    Process result = { -1, "" };

    // Open the process's status file
    char pidPath[PATH_MAX];
    snprintf(pidPath, sizeof(pidPath), "/proc/%d/status", pid);
    FILE *pidStatus = fopen(pidPath, "r");
    if (!pidStatus) return result;

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pidStatus))
    {
        // Look for the PPid field in the status file
        if (sscanf(buffer, "PPid: %d", &result.pid) == 1)
        {
            fclose(pidStatus);

            // Get parent process' name
            char commPath[PATH_MAX];
            snprintf(commPath, sizeof(commPath), "/proc/%d/comm", result.pid);
            FILE *comm = fopen(commPath, "r");
            if (!comm) { result.pid = -1; return result; }

            fgets(result.name, sizeof(result.name), comm);
            result.name[strcspn(result.name, "\n")] = '\0';
            fclose(comm);

            return result;
        }
    }

    fclose(pidStatus);
    return result;
}

/**
 * @return winsize struct containing the current terminal size in columns and rows
 */
struct winsize getTerminalSize(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        ws.ws_col = 80;
        ws.ws_row = 24;
    }
    return ws;
}

/**
 * @param currPath Current working directory path
 * @param entry Directory entry to check
 */
int isFileExecutable(char *currPath, struct dirent *entry)
{
    char filePath[PATH_MAX + 256];
    snprintf(filePath, PATH_MAX + 256, "%s/%s", currPath, entry->d_name);
    if (access(filePath, X_OK) == 0) return 1;
    else return 0;
}

/**
 * @param prog Program's executable name
 * @param isExec Flags if the function should also check if a found program has
 *               execute permissions
 * @returns 1 if program is installed; 0 if not
 */
int isProgramInstalled(char *prog, int isExec)
{
    int mode = isExec ? X_OK : F_OK;

    char *path = getenv("PATH");
    if (!path)
    {
        char cmd[64];
        snprintf(cmd, 64, "%s --version > /dev/null 2>&1", prog);
        return (system(cmd) == 0);
    }

    char *paths = strdup(path);
    char *dir = strtok(paths, ":");
    while (dir)
    {
        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dir, prog);
        if (access(fullPath, mode) == 0)
        {
            free(paths);
            return 1;
        }
        dir = strtok(NULL, ":");
    }
    free(paths);

    // Also try /usr/libexec
    char libexecPath[PATH_MAX];
    snprintf(libexecPath, PATH_MAX, "/usr/libexec/%s", prog);
    if (access(libexecPath, mode) == 0) return 1;

    return 0;
}

/**
 * Calculates the square root of a given number (and helps us avoid including
 * math.h) - integer variant.
 * @param x Input value
 * @returns Square root of the input value; -1 if imaginary/invalid
 */
int iSqrt(int x)
{
    // Return -1 to flag imaginary result
    if (x < 0) return -1;
    // sqrt(0 or 1) = number itself anyway
    if (x < 2) return x;

    long low = 1;
    long high = x / 2;
    int result = 0;

    // Let's do a binary search
    while (low <= high)
    {
        long mid = low + (high - low) / 2;
        long square = mid * mid;

        // If perfect square found, get out now
        if (square == x) return (int)mid;
        else if (square < x)
        {
            result = (int)mid;
            low = mid + 1;
        }
        // If square is too large, go lower
        else high = mid - 1;
    }

    return result;
}

/**
 * Checks if a given process name is presently running and via the /proc 
 * filesystem.
 * @param name The process name to find
 * @param strict Flags if we are looking for an exact match (1) or not (0)
 * @return 1 if found; 0 if not found or error
 */
int procExists(const char *name, const int strict)
{
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL)
    {
        // Skip non-numeric (not PID) entries
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;

        // Build path to process' comm (command) file
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);

        FILE *commFile = fopen(path, "r");
        if (!commFile) continue;

        char commVal[TASK_COMM_LEN];
        int found = 0;

        if (fgets(commVal, TASK_COMM_LEN, commFile))
        {
            // Strip trailing newline
            commVal[strcspn(commVal, "\n")] = '\0';

            // If strict, we look for an exact match
            if (strict)
                found = strcmp(commVal, name) == 0;
            // If not, we look for a substring
            else
                found = strstr(commVal, name) != NULL;
        }

        fclose(commFile);

        if (found)
        {
            closedir(proc);
            return 1;
        }
    }

    closedir(proc);
    return 0;
}

/**
 * Reads a single hexadecimal number for a given text file.
 * @param path Path to file to open
 * @return The read number as an integer (0 as fallback)
 */
int readHexFile(const char *path)
{
    FILE *fStream = fopen(path, "r");
    if (!fStream) return 0;
    int val;
    if (fscanf(fStream, "%x", &val) != 1) val = 0;
    fclose(fStream);
    return val;
}

/**
 * Splits a given string via any newline escape sequences into an array of strings.
 * @param text Text to split
 * @param textLines Text once split
 * @param totalLines Number of newlines detected
 */
void splitText(char *text, char *textLines[], int totalLines)
{
    int count = 0;
    char *curr = text;
    char *start = text;

    while (*curr && count < totalLines)
    {
        if (*curr == '\n')
        {
            *curr = '\0';
            textLines[count++] = start;
            start = curr + 1;
        }
        curr++;
    }

    if (count < totalLines)
        textLines[count++] = start;
}
