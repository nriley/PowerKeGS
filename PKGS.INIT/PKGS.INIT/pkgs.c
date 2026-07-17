/*
 *  pkgs.c
 *  PKGS.Init
 *
 * Created by Nicholas Riley on 7/3/2026.
 *
 */

#include <Event.h>
#include <Locator.h>
#include <MiscTool.h>
#include <QuickDraw.h>
#include <adb.h>
#include <gsbug.h>
#include <stdio.h>
#include <string.h>
#include <texttool.h>

#include "pkgs.h"

#pragma noroot
#pragma memorymodel 1

BOOLEAN adbComplete;
Byte adbDataLen; /* Talk: data length (0 or 2-8) NOT including length byte */
/*
   Talk: data with MSB in adbData[0]
   Listen: command in adbData[0]; data with MSB in adbData[1]
*/
Byte adbData[9];

extern void receiveRegister(void);

void printError(char* error)
{
    ErrWriteLine((Pointer) "\p\rAn error occurred while attempting to power off your Apple IIgs.\r");
    ErrWriteCString((Pointer)error);
    ErrWriteCString((Pointer) "\r\rPress the space bar to continue.\r");
    while ((ReadChar(0) & 0x7f) != ' ')
        ;
}

BOOLEAN talkADB(Byte adbRegister, Byte address, Byte expectedLen, char* errBuf)
{
    Word command = talk + (16 * adbRegister) + address;

    adbComplete = FALSE;
    adbDataLen = 0;
    memset(adbData, 0, 9);

    while (TRUE) {
        AsyncADBReceive((Pointer)receiveRegister, command);
        switch (toolerror()) {
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
    while (!adbComplete) {
        if (TickCount() >= timeoutTicks)
            break;
    }
    if (!adbComplete) {
        sprintf(errBuf, "Register %hhd talk request timed out.", adbRegister);
    } else if (adbComplete && expectedLen != adbDataLen) {
        sprintf(errBuf, "Register %hhd length was %hhd; expected %hhd.",
            adbRegister, adbDataLen, expectedLen);
    } else {
        errBuf[0] = '\0';
    }
    return adbComplete;
}

BOOLEAN listenADB(Byte adbRegister, Byte address)
{
    adbData[0] = (16 * address) + 8 + adbRegister;
    SendInfo(adbDataLen + 1, (Pointer)adbData, transmitADBBytes + adbDataLen);
    if (toolerror()) {
        TOOLFAIL("Can't transmit to ADB device");
        return FALSE;
    }
    return TRUE;
}

BOOLEAN powerOff(void)
{
    char buf[255];

    Word qdVersion = QDVersion();
    if (toolerror()) {
        TOOLFAIL("Can't get QuickDraw version");
        return FALSE;
    }

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

    WriteLine((Pointer)("\pPowerKeGS"));

    if ((qdVersion & 0xffff) < 0x0308) {
        printError("System Software 6.0.1 or later is required.");
        return FALSE;
    }

    ADBStartUp();

    if (!talkADB(3, 7, 2, buf) || buf[0] != '\0') {
        if (adbDataLen == 0) {
            printError("No device was found at ADB address 7.");
            return FALSE;
        }
        if (buf[0] != '\0')
            printError(buf);
        return FALSE;
    }
    if (adbData[1] != 0x22) {
        sprintf(buf, "Device at ADB address 7 has handler $%02hhx; expected PowerKey ($22).", adbData[1]);
        printError(buf);
        return FALSE;
    }

    /* Register 0 - status */
    adbDataLen = 2;
    adbData[1] = 0x01;
    adbData[2] = 0x00;
    listenADB(0, 7);
    if (!talkADB(0, 7, 2, buf) || buf[0]) {
        if (!buf[0])
            printError("Failed to communicate with PowerKey.");
        return FALSE;
    }

    unsigned short version = (adbData[1] & 0xf) + 11;
    sprintf(buf, "Found PowerKey version %hd.%hd.\r", version / 10, version % 10);
    WriteCString((Pointer)buf);

    if (!talkADB(1, 7, 4, buf) || buf[0]) {
        if (!buf[0])
            printError("Unable to read power on timer from PowerKey.");
        return FALSE;
    }
    sprintf(buf, " - power on timer $%02hhx%02hhx%02hhx%02hhx\r",
        adbData[3], adbData[2], adbData[1], adbData[0]);
    WriteCString((Pointer)buf);

    if (!talkADB(2, 7, 4, buf) || buf[0]) {
        if (!buf[0])
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
    adbData[4] = 0xff;
    listenADB(2, 7);

    WriteLine((Pointer) "\pPowering off...");
    if (!talkADB(2, 7, 4, buf) || buf[0]) {
        printError("Unable to set power off timer on PowerKey.");
        return FALSE;
    }

    /* Even with timer set to maximum, PowerKey takes a bit of time to power off */
    Long endTicks = TickCount() + 60;
    while (TickCount() < endTicks);

    return TRUE;
}
