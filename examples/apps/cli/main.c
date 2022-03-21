/*
 *  Copyright (c) 2016, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <openthread-core-config.h>
#include <stdio.h>
#include <openthread/config.h>
#include <openthread/message.h>

#include <stdlib.h>
#include <time.h>

#include <sys/inotify.h>
#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <openthread/platform/logging.h>

#include "openthread-system.h"
#include "cli/cli_config.h"
#include "cli/cli_shitty_logger.h"
#include "common/code_utils.hpp"
/**
 * This function initializes the CLI app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
extern void otAppCliInit(otInstance *aInstance);
int         fd;
int         wd;
#define MAX_EVENTS 1024 /* Maximum number of events to process*/
#define LEN_NAME                                                                              \
    16                                            /* Assuming that the length of the filename \
                             won't exceed 16 bytes*/
#define EVENT_SIZE (sizeof(struct inotify_event)) /*size of one event*/
#define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + LEN_NAME))
#if OPENTHREAD_EXAMPLES_SIMULATION
#include <setjmp.h>
#include <unistd.h>

#include <pthread.h>

void   *listenLocal(void *instance);
jmp_buf gResetJump;

void __gcov_flush();
#endif

#ifndef OPENTHREAD_ENABLE_COVERAGE
#define OPENTHREAD_ENABLE_COVERAGE 0
#endif

#if OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE
void *otPlatCAlloc(size_t aNum, size_t aSize)
{
    return calloc(aNum, aSize);
}

void otPlatFree(void *aPtr)
{
    free(aPtr);
}
#endif

void otTaskletsSignalPending(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
}

#if OPENTHREAD_POSIX && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
static void ProcessExit(void *aContext, uint8_t aArgsLength, char *aArgs[])
{
    OT_UNUSED_VARIABLE(aContext);
    OT_UNUSED_VARIABLE(aArgsLength);
    OT_UNUSED_VARIABLE(aArgs);

    exit(EXIT_SUCCESS);
}
static const otCliCommand kCommands[] = {{"exit", ProcessExit}};
#endif

int main(int argc, char *argv[])
{
    otInstance *instance;

#if OPENTHREAD_EXAMPLES_SIMULATION
    if (setjmp(gResetJump))
    {
        alarm(0);
#if OPENTHREAD_ENABLE_COVERAGE
        __gcov_flush();
#endif
        execvp(argv[0], argv);
    }
#endif

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    size_t   otInstanceBufferLength = 0;
    uint8_t *otInstanceBuffer       = NULL;
#endif

pseudo_reset:
    rotate_shitty_log();
    otSysInit(argc, argv);

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    otInstanceBuffer = (uint8_t *)malloc(otInstanceBufferLength);
    assert(otInstanceBuffer);

    // Initialize OpenThread with the buffer
    instance = otInstanceInit(otInstanceBuffer, &otInstanceBufferLength);
#else
    instance = otInstanceInitSingle();
#endif
    assert(instance);

    otAppCliInit(instance);

#if OPENTHREAD_POSIX && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    otCliSetUserCommands(kCommands, OT_ARRAY_LENGTH(kCommands), instance);
#endif
    int       rc;
    pthread_t listenThread;
    rc = pthread_create(&listenThread, NULL, listenLocal, (void *)instance);

    if (rc)
    {
        printf("Error:unable to create thread, %d\n", rc);
        exit(-1);
    }
    shitty_log("Info", "Before Thread Start");

    while (!otSysPseudoResetWasRequested())
    {
        otTaskletsProcess(instance);
        otSysProcessDrivers(instance);
    }
    shitty_log("Info", "OT factory reset was initialized");

    pthread_cancel(listenThread);

    otInstanceFinalize(instance);
#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    free(otInstanceBuffer);
#endif

    goto pseudo_reset;

    return 0;
}

#if OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_APP
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    va_list ap;

    va_start(ap, aFormat);
    otCliPlatLogv(aLogLevel, aLogRegion, aFormat, ap);
    va_end(ap);
}
#endif

void *listenLocal(void *instance)
{
    char *thisPath = "foo.txt";

    if (!thisPath)
    {
        printf("No path found\n");
        exit(1);
    }
    fd = inotify_init();
    wd = inotify_add_watch(fd, thisPath, IN_MODIFY | IN_CREATE | IN_DELETE | IN_IGNORED);
    while (1)
    {
        int  length;
        char buffer[BUF_LEN];
        shitty_log("Info", "Waiting for events...");
        length = read(fd, buffer, BUF_LEN);
        if (length < 0)
        {
            shitty_log("warning", "read error\n");
            continue;
        }
        shitty_log("Info", "Received event.");
        FILE *fp = fopen(thisPath, "r");
        if (fp == NULL)
        {
            shitty_log("Warning", "File not found");
            fclose(fp);
            continue;
        }
        fseek(fp, 0L, SEEK_END);
        int sz = ftell(fp);
        rewind(fp);

        char *buff = malloc(sz);
        fgets(buff, sz, fp);
        fclose(fp);
        char strbuf[1024];
        snprintf(strbuf, sizeof(strbuf), "File sizeee: [%d]", sz);
        shitty_log("Info", strbuf);

        otMessageSettings *settings    = malloc(sizeof(otMessageSettings));
        settings->mLinkSecurityEnabled = 0;
        settings->mPriority            = 1;
        otMessage *aMessage;
        aMessage = otUdpNewMessage(instance, settings);
        otMessageSetLength(aMessage, sz);

        otMessageWrite(aMessage, 0, buff, sz);
        otMessageInfo *b = malloc(sizeof(otMessageInfo));
        b->mLinkInfo = malloc(2);
        b->mHopLimit = 255;

        handleUDP(instance, aMessage, b);
    }
}