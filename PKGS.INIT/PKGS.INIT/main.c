/*
 * main.c
 * PKGS.Init
 *
 * Created by Nicholas Riley on 7/26/2025.
 * Copyright (c) 2025 Nicholas Riley. All rights reserved.
 *
 */

#pragma rtl

#include <Event.h>
#include <QuickDraw.h>
#include <adb.h>
#include <gsbug.h>
#include <locator.h>
#include <misctool.h>
#include <orca.h>
#include <stdio.h>
#include <string.h>
#include <texttool.h>

#define TOOLFAIL(string)        \
    if (toolerror())            \
        SysFailMgr(toolerror(), \
                   (Pointer) "\p" string "\n\r    Error Code -> $");

const char bootInfoString[] = "PKGS.Init             v1.0d1";
LongWord version = 0x01006001; /* in rVersion format - XXX this is wrong */

const Word os_p8_switch = 0x2d;

extern Word *unloadFlagPtr;

extern void p8switch(void);
extern void patchWindStatus(void);

BOOLEAN adbComplete;
Byte adbDataLen; /* Talk: data length (0 or 2-8) NOT including length byte */
/*
   Talk: data with MSB in adbData[0]
   Listen: command in adbData[0]; data with MSB in adbData[1]
*/
Byte adbData[9];

extern void receiveRegister(void);

void setUnloadFlag(void)
{
    if (*unloadFlagPtr == 0)
        *unloadFlagPtr = 1;
}

BOOLEAN talkADB(Byte adbRegister, Byte address, Byte expectedLen, char *errBuf)
{
    Word command = talk + (16 * adbRegister) + address;
    adbComplete = FALSE;
    adbDataLen = 0;
    memset(adbData, 0, 9);

    while (TRUE)
    {
        AsyncADBReceive((Pointer)receiveRegister, command);
        switch (toolerror())
        {
        case adbBusy:
            continue;
        case noError:
            break;
        default:
            TOOLFAIL("Can't receive from ADB device");
            return FALSE;
        }
        break;
    }
    /* XXX We only hit this timeout on emulators. fmradio.s doesn't have one -
     * it just loops on adbBusy */
    Long timeoutTicks = TickCount() + 60;
    while (!adbComplete)
    {
        if (TickCount() >= timeoutTicks)
            break;
    }
    if (!adbComplete)
    {
        sprintf(errBuf, " - register %hhd talk request timed out", adbRegister);
    }
    else if (adbComplete && expectedLen != adbDataLen)
    {
        sprintf(errBuf, " - register %hhd length %hhd (expected %hhd)",
                adbRegister, adbDataLen, expectedLen);
    }
    else
    {
        errBuf[0] = '\0';
    }
    return adbComplete;
}

BOOLEAN listenADB(Byte adbRegister, Byte address)
{
    adbData[0] = (16 * address) + 8 + adbRegister;
    SendInfo(adbDataLen + 1, (Pointer)adbData, transmitADBBytes + adbDataLen);
    if (toolerror())
    {
        TOOLFAIL("Can't transmit to ADB device");
        return FALSE;
    }
    return TRUE;
}

void printError(char *error) {
    ErrWriteCString((Pointer)error);
    ErrWriteCString((Pointer)"\rPress the space bar to proceed.\r");
    while ((ReadChar(0) & 0x7f) != ' ');
}

BOOLEAN powerOff(void)
{
    char buf[255];
    GrafOff();
    TextStartUp();
    SetInGlobals(0xff, 0x80);
    SetOutGlobals(0xff, 0x80);
    SetInputDevice(0, 3);
    SetOutputDevice(0, 3);
    SetErrorDevice(0, 3);
    InitTextDev(0);
    InitTextDev(1);
    InitTextDev(2);

    WriteLine((Pointer)("\pShutting down..."));

    if (!talkADB(3, 7, 2, buf) || adbDataLen == 0 || buf[0] || adbData[1] != 0x22) {
        printError("No PowerKey found.");
        return FALSE;
    }

    /* Register 0 - status */
    adbDataLen = 2;
    adbData[1] = 0x01;
    adbData[2] = 0x00;
    listenADB(0, 7);
    if (!talkADB(0, 7, 2, buf) || buf[0])
    {
        printError("Failed to communicate with PowerKey.");
        return FALSE;
    }

    unsigned short version = (adbData[1] & 0xf) + 11;
    sprintf(buf, "Found PowerKey version %hd.%hd\r", version / 10, version % 10);
    WriteCString((Pointer)buf);

    if (!talkADB(1, 7, 4, buf) || buf[0])
    {
        printError("Unable to read power on timer from PowerKey.");
        return FALSE;
    }
    sprintf(buf, " - power on timer $%02hhx%02hhx%02hhx%02hhx\r",
            adbData[3], adbData[2], adbData[1], adbData[0]);
    WriteCString((Pointer)buf);

    if (!talkADB(2, 7, 4, buf) || buf[0])
    {
        printError("Unable to read power off timer from PowerKey.");
        return FALSE;
    }
    sprintf(buf, " - power off timer $%02hhx%02hhx%02hhx%02hhx\r",
            adbData[0], adbData[1], adbData[2], adbData[3]);
    WriteCString((Pointer)buf);

    adbDataLen = 4;
    adbData[1] = 0xff;
    adbData[2] = 0xff;
    adbData[3] = 0xff;
    adbData[4] = 0xc0;
    listenADB(2, 7);

    WriteLine((Pointer)"\pPowering off...");
    if (!talkADB(2, 7, 4, buf) || buf[0])
    {
        printError("Unable to set power off timer on PowerKey.");
        return FALSE;
    }
    return TRUE;
}

int main(void)
{
    ShowBootInfo((Pointer)bootInfoString, NULL /* iconPtr */);

    Pointer prevAddr = GetVector(os_p8_switch);
    SetVector(os_p8_switch, (Pointer)p8switch);

    return 0;
}
