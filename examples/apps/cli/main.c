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

#include <arpa/inet.h>
#include <assert.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <openthread-core-config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <openthread/config.h>
#include <openthread/message.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/inotify.h>
#include <openthread/cli.h>
#include <openthread/diag.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <openthread/platform/logging.h>

#include "openthread-system.h"
#include "cli/cli_config.h"
#include "common/code_utils.hpp"

int id;
/**
 * This function initializes the CLI app.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
extern void otAppCliInit(otInstance *aInstance, char * networkKey, char * panId, int int useAsMaster);
void        bzero();

#if OPENTHREAD_EXAMPLES_SIMULATION
#include <setjmp.h>
#include <unistd.h>

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
    FILE *             file = fopen("log.txt", "a");

    fprintf(file, "main");
    fflush(file);
    otInstance *instance;
    // Initialize the dataset struct to null
    dataset * ds;
    // Set the dataset struct to something but let all values be null
    ds = (dataset *)malloc(sizeof(dataset));

    // Convert char * to int
    id = atoi(argv[1]);

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

    fprintf(file, "beforePseudo_reset");
    fflush(file);
pseudo_reset:
    otSysInit(argc, argv, ds);
    fprintf(file, "afterPseudo_reset");
    fflush(file);

#if OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE
    // Call to query the buffer size
    (void)otInstanceInit(NULL, &otInstanceBufferLength);

    // Call to allocate the buffer
    otInstanceBuffer = (uint8_t *)malloc(otInstanceBufferLength);
    assert(otInstanceBuffer);

    // Initialize OpenThread with the buffer
    instance = otInstanceInit(otInstanceBuffer, &otInstanceBufferLength);
#else
    fprintf(file, "beforeinstance");
    instance = otInstanceInitSingle();
    fprintf(file, "instancecreated");
    fflush(file);
#endif
    assert(instance);
    if (ds->networkKey != NULL) {
    }
    fprintf(file, "BeforeotAppCliInit");
    fflush(file);
    otAppCliInit(instance, ds->networkKey, ds->panId, ds->int useAsMaster);
    fprintf(file, "afterotAppCliInit");
    fflush(file);

#if OPENTHREAD_POSIX && !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
    otCliSetUserCommands(kCommands, OT_ARRAY_LENGTH(kCommands), instance);
#endif


    struct sockaddr_in serverAddr, clientAddr;
    int                listenAddr   = 5000 + id;
    char              *listenDomain = "127.0.0.1";
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        fprintf(file, "socket");
        fflush(file);
        perror("socket");
        exit(1);
    }

    int rc = fcntl(sockfd, F_SETFL, O_NONBLOCK);
    if (rc < 0)
    {
        fprintf(file, "fcntl");
        fflush(file);
        perror("fcntl failed");
        close(sockfd);
        exit(-1);
    }

    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(listenAddr);
    serverAddr.sin_addr.s_addr = inet_addr(listenDomain);
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        fprintf(file, "bind");
        fflush(file);
        perror("bind");
        exit(1);
    }

    fprintf(file, "beforewhile");
    fflush(file);
    while (!otSysPseudoResetWasRequested())
    {
        fprintf(file, "indsicewhileloop");
        fflush(file);

        otTaskletsProcess(instance);
        fprintf(file, "beforeprocessDrivers");
        fflush(file);
        otSysProcessDrivers(instance);
        fprintf(file, "afterprocessDrivers");
        fprintf(file, "getmessage");
        fflush(file);
        // Get message
        char      buffer[1024];
        socklen_t clilen = sizeof(clientAddr);
        ssize_t   n      = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddr, &clilen);
        fprintf(file, "n: %li ", n);
        fflush(file);
        if (n > 0)
        {
            otMessageSettings *settings    = malloc(sizeof(otMessageSettings));
            settings->mLinkSecurityEnabled = 0;
            settings->mPriority            = 1;
            otMessage *aMessage;
            aMessage = otUdpNewMessage(instance, settings);

            if (otMessageSetLength(aMessage, sizeof(buffer)) != OT_ERROR_NONE)
            {
                perror("message write");
            };
            if (0 > otMessageWrite(aMessage, 0, buffer, sizeof(buffer)))
            {
                perror("message write");
            };
            otMessageInfo *b = malloc(sizeof(otMessageInfo));
            b->mLinkInfo     = malloc(2);
            b->mHopLimit     = 255;

            fprintf(file, "beforehandleudp");
            fflush(file);
            handleUDP(instance, aMessage, b);
        }
    }
    fclose(file);

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

void bzero(void *s, size_t n)
{
    memset(s, 0, n);
}
