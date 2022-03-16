#ifndef OPENTHREAD_CLI_SHITTY_LOGGER_H
#define OPENTHREAD_CLI_SHITTY_LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void shitty_log(const char *tag, const char *toLog)
{
    time_t now;
    time(&now);

    FILE *fp = fopen("log.txt", "a+");
    fprintf(fp, "%s [%s]: %s\n", ctime(&now), tag, toLog);
    fflush(fp);
    fclose(fp);
}

void rotate_shitty_log()
{
    // Rotates the logs
    // Check if file exists
    FILE *fp = fopen("log.txt", "r");
    if (fp == NULL)
    {
        // File does not exist
        return;
    }
    // if it does exist, then move it to a backup file
    fclose(fp);
    fp           = fopen("log.txt", "r");
    FILE *fp_bak = fopen("log.bak", "w");
    char  c;
    while ((c = fgetc(fp)) != EOF)
    {
        fputc(c, fp_bak);
    }
    fclose(fp);
    fclose(fp_bak);
    // remove the original file
    remove("log.txt");
    // rename the backup file to the original file
}

#endif // OPENTHREAD_CLI_SHITTY_LOGGER_H
