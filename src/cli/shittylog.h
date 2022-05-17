#ifndef OPENTHREAD_CLI_SHITTY_LOGGER_H
#define OPENTHREAD_CLI_SHITTY_LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Log prefix:
#define LOG_NAME "child_log.txt"

void shitty_log(const char *tag, const char *toLog)
{
    time_t now;
    time(&now);

    FILE *fp = fopen(LOG_NAME, "a+");
    fprintf(fp, "%s [%s]: %s\n", ctime(&now), tag, toLog);
    fflush(fp);
    fclose(fp);
}

void rotate_shitty_log()
{
    // Rotates the logs
    // Check if file exists
    FILE *fp = fopen(LOG_NAME, "r");
    if (fp == NULL)
    {
        // File does not exist
        return;
    }
    // if it does exist, then move it to a backup file
    fclose(fp);
    fp           = fopen(LOG_NAME, "r");
    // Back name is the current name with a .bak extension
    char *backup = (char *)malloc(strlen(LOG_NAME) + 5);
    strcpy(backup, LOG_NAME);
    strcat(backup, ".bak");
    FILE *fp_bak = fopen(backup, "w");
    char  c;
    while ((c = fgetc(fp)) != EOF)
    {
        fputc(c, fp_bak);
    }
    fclose(fp);
    fclose(fp_bak);
    // remove the original file
    remove(LOG_NAME);
    // rename the backup file to the original file
}

#endif // OPENTHREAD_CLI_SHITTY_LOGGER_H
