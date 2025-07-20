/*
 * main.c
 * ADB Explorer
 *
 * Created by Nicholas Riley on 6/29/2025.
 * Copyright (c) 2025 Nicholas Riley. All rights reserved.
 *
 */

#include <Control.h>
#include <Desk.h>
#include <Event.h>
#include <GSOS.h>
#include <Locator.h>
#include <Memory.h>
#include <Menu.h>
#include <MiscTool.h>
#include <Print.h>
#include <QuickDraw.h>
#include <Resources.h>
#include <STDFile.h>
#include <TextEdit.h>
#include <Window.h>
#include <adb.h>
#include <gsbug.h>

#include <limits.h>
#include <orca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"

/* Defines and macros */

#define TOOLFAIL(string)                                                       \
    if (toolerror())                                                           \
        SysFailMgr(toolerror(),                                                \
                   (Pointer) "\p" string "\n\r    Error Code -> $");

#define MAX_DOCUMENT_NAME 80

#define PRINT_RECORD_SIZE 140

/* Types */

typedef struct tDocument {
    struct tDocument *nextDocument;
    struct tDocument *prevDocument;
    GrafPortPtr wPtr;
    char documentName[MAX_DOCUMENT_NAME + 1];
    BOOLEAN isOnDisk;
    ResultBuf255Hndl pathName;
    PrRecHndl printRecordHandle;
} tDocument;

/* Globals */

BOOLEAN shouldQuit;
EventRecord myEvent;
unsigned int userID;
tDocument *documentList;

BOOLEAN adbComplete;
Byte adbDataLen; /* Talk: data length (0 or 2-8) NOT including length byte */
/*
   Talk: data with MSB in adbData[0]
   Listen: command in adbData[0]; data with MSB in adbData[1]
*/
Byte adbData[9];

extern void receiveRegister(void);

/* Forward declarations */

void doFileSave(void);

/* Implementation */

const char *resourceString(int resourceId, const char *defaultValue)
{
    Handle handle;
    const char *result = defaultValue;

    handle = LoadResource(rCString, resourceId);
    if (toolerror() == 0) {
        HLock(handle);
        result = (const char *)(*handle);
    }
    return result;
}

void freeResourceString(int resourceId)
{
    ReleaseResource(-3, rCString, resourceId);
}

void showErrorAlert(int resourceId, int toolErrorCode)
{
    char buffer[40] = "";
    const char *substArray[2];
    const char *toolErrorFormat;

    if (toolErrorCode != 0) {
        toolErrorFormat =
            resourceString(TOOL_ERROR_STRING, "\n\nTool error code = $%04x");
        sprintf(buffer, toolErrorFormat, toolErrorCode);
        freeResourceString(TOOL_ERROR_STRING);
    }

    substArray[0] = resourceString(resourceId, "Unknown error has occurred");
    substArray[1] = buffer;

    AlertWindow(awCString + awResource, (Pointer)substArray,
                ERROR_ALERT_STRING);
    freeResourceString(resourceId);
}

tDocument *findDocumentFromWindow(GrafPortPtr wPtr)
{
    tDocument *result = documentList;

    while (result != NULL) {
        if (result->wPtr == wPtr)
            break;

        result = result->nextDocument;
    }

    return result;
}

#pragma databank 1
void drawContents(void) { DrawControls(GetPort()); }
#pragma databank 0

const char *getWindowTitle(void)
{
    static int documentNum = 1;
    static char buffer[MAX_DOCUMENT_NAME + 1];
    const char *titleFormat =
        resourceString(TITLE_STRING, "  ADB Explorer %d  ");

    sprintf(buffer + 1, titleFormat, documentNum);
    buffer[0] = strlen(buffer + 1);

    freeResourceString(TITLE_STRING);
    documentNum++;

    // Returns a Pascal string with a length byte prefix
    return buffer;
}

const char *documentNameFromGSOSString(ResultBuf255Ptr gsosString)
{
    static char buffer[MAX_DOCUMENT_NAME + 1];
    int nameLen = gsosString->bufString.length;

    strcpy(buffer + 1, "  ");
    nameLen = gsosString->bufString.length;
    if (nameLen > MAX_DOCUMENT_NAME - 5)
        nameLen = MAX_DOCUMENT_NAME - 5;
    strncat(buffer + 1, gsosString->bufString.text, nameLen);
    strcat(buffer + 1, "  ");
    buffer[0] = strlen(buffer + 1);

    return buffer;
}

const char *documentNameFromPath(wStringPtr pathString)
{
    static char buffer[MAX_DOCUMENT_NAME + 1];
    int nameLen = pathString->length;
    char *path = pathString->text;
    int lastSeparator = 0;
    int offset;

    for (offset = 0; offset < nameLen; offset++) {
        if ((path[offset] == ':') || (path[offset] == '/'))
            lastSeparator = offset + 1;
    }

    path = &(path[lastSeparator]);
    nameLen -= lastSeparator;

    strcpy(buffer + 1, "  ");
    if (nameLen > MAX_DOCUMENT_NAME - 5)
        nameLen = MAX_DOCUMENT_NAME - 5;
    strncat(buffer + 1, path, nameLen);
    strcat(buffer + 1, "  ");
    buffer[0] = strlen(buffer + 1);

    return buffer;
}

BOOLEAN appendToDocument(tDocument *documentPtr, char *strToAppend)
{
    Handle teH =
        (Handle)GetCtlHandleFromID(documentPtr->wPtr, CONTROL_TEXT_EDIT);
    if (toolerror() != 0)
        return FALSE;

    TESetSelection((Pointer)UINT_MAX, (Pointer)UINT_MAX, teH);
    if (toolerror() != 0)
        return FALSE;

    Long strLength = strlen(strToAppend);
    TEInsert(teRefIsPtr | teDataIsCString, (Ref)strToAppend, strLength, 0, NULL,
             teH);
    if (toolerror() != 0)
        return FALSE;

    return TRUE;
}

BOOLEAN talkADB(Byte adbRegister, Byte address)
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
    return adbComplete;
}

BOOLEAN listenADB(Byte adbRegister, Byte address)
{
    /* Command format (e.g. Inside Mac ADB Manager figure 5-2)
     *   7654 32 10
     *   ----------
     *   ADDR 10 RR
     * The command above is 0x08.
     * The listen constant in the ORCA/C headers is 0x80 and
     * its use is undocumented in Table 3-4 of the Apple IIgs
     * Toolbox Reference. It's listed in the table on page 3-28
     * and is documented in the internal tool set docs; it's
     * also the same constant as transmit2ADBBytes. We use the
     * more general transmitADBBytes instead, as we sometimes
     * need to transmit more than 2 bytes.
     */
    adbData[0] = (16 * address) + 8 + adbRegister;
    /*
    char buf[255];
    sprintf(buf + 1, "adbDataLen:%hd adbData:%p adbData[0]:$%hx",
            adbDataLen + 1, (Pointer)adbData, adbData[0]);
    buf[0] = (Byte)strlen(buf + 1);
    DebugStr((StringPtr)buf);
    */
    SendInfo(adbDataLen + 1, (Pointer)adbData, transmitADBBytes + adbDataLen);
    if (toolerror()) {
        TOOLFAIL("Can't transmit to ADB device");
        return FALSE;
    }
    return TRUE;
}

void scanADB(tDocument *documentPtr)
{
    Word version = ADBVersion();
    char buf[255];
    /* XXX use resources for string constants when output finalized */
    sprintf(buf, " using ADB Tool Set version %d\r", version);
    appendToDocument(documentPtr, buf);

    /* sprintf(buf, "listenADB:%p adbData:%p", listenADB, (Pointer)adbData); */
    appendToDocument(documentPtr, buf);
    for (Byte address = 1; address <= 0xE; address++) {
        char *deviceDescription;
        switch (address) {
            /* 0x0 is reserved for the computer */
            case 0x1:
                deviceDescription = "Dongle";
                break; /* Protection */
            case 0x2:
                deviceDescription = "Keyboard";
                break; /* Encoded */
            case 0x3:
                deviceDescription = "Mouse";
                break; /* Relative */
            case 0x4:
                deviceDescription = "Tablet";
                break; /* Absolute */
            case 0x5:
                deviceDescription = "Modem";
                break; /* Data transfer */
            case 0x6:
                deviceDescription = "Reserved";
                break;
            case 0x7:
                deviceDescription = "Appliance";
                break;
            default:
                deviceDescription = "Relocated";
                break;
        }
        sprintf(buf, "\rAddress $0%x (%s):\t", address, deviceDescription);
        appendToDocument(documentPtr, buf);

        if (!talkADB(3, address)) {
            appendToDocument(documentPtr, "register 3 talk request timed out");
            continue;
        }
        if (adbComplete && adbDataLen == 0) {
            appendToDocument(documentPtr, "no device");
            continue;
        }
        if (adbDataLen != 2) {
            sprintf(buf, " - register 3 length %hhx (expected 2)", adbDataLen);
            continue;
        }
        sprintf(buf, "handler $%02hhx", adbData[1]);
        appendToDocument(documentPtr, buf);
        if (adbData[0] & 0x20) {
            appendToDocument(documentPtr, " - SRQ enabled");
        }
        /* XXX Address is bogus on ROM 01 hardware, yet correct on emulators?
            if ((adbData[0] & 0x0F) != address) {
                sprintf(buf, " - unexpected address $%02hhx", adbData[0] & 0xF);
                appendToDocument(documentPtr, buf);
                continue;
            }
        */
        if (address == 7 && adbData[1] == 0x22) { /* PowerKey? */
            /* Register 0 - status */
            adbDataLen = 2;
            adbData[1] = 0x01;
            adbData[2] = 0x00;
            listenADB(0, address);
            if (!talkADB(0, address)) {
                appendToDocument(documentPtr,
                                 " - register 0 talk request timed out");
                continue;
            }
            if (adbDataLen != 2) {
                sprintf(buf, " - register 0 length %hhx (expected 2)",
                        adbDataLen);
                appendToDocument(documentPtr, buf);
                continue;
            }
            unsigned short version = (adbData[1] & 0xf) + 11;
            sprintf(buf, " version %hd.%hd", version / 10, version % 10);
            appendToDocument(documentPtr, buf);

            if ((adbData[1] & 128) == 0)
                appendToDocument(documentPtr, " powered off, ");

            if (adbData[1] & 32)
                appendToDocument(documentPtr, " last powered on from keyboard");
            else if (adbData[1] & 64)
                appendToDocument(documentPtr, " last powered on from timer");

            if (!talkADB(1, address)) {
                appendToDocument(documentPtr,
                                 " - register 1 talk request timed out");
                continue;
            }
            if (adbDataLen != 4) {
                sprintf(buf, " - register 1 length %hhx (expected 4)",
                        adbDataLen);
                appendToDocument(documentPtr, buf);
                continue;
            }
            sprintf(buf, " - power on timer $%02hhx%02hhx%02hhx%02hhx",
                    adbData[3], adbData[2], adbData[1], adbData[0]);
            appendToDocument(documentPtr, buf);

            if (!talkADB(2, address)) {
                appendToDocument(documentPtr,
                                 " - register 2 talk request timed out");
                continue;
            }
            if (adbDataLen != 4) {
                sprintf(buf, " - register 2 length %hhx (expected 4)",
                        adbDataLen);
                appendToDocument(documentPtr, buf);
                continue;
            }
            sprintf(buf, " - power off timer $%02hhx%02hhx%02hhx%02hhx",
                    adbData[0], adbData[1], adbData[2], adbData[3]);
            appendToDocument(documentPtr, buf);

            adbDataLen = 4;
            adbData[1] = 0xff;
            adbData[2] = 0xff;
            adbData[3] = 0xfc;
            adbData[4] = 0x00;
            listenADB(2, address);

            if (!talkADB(2, address)) {
                appendToDocument(documentPtr,
                                 " - register 2 talk request timed out");
                continue;
            }
            if (adbDataLen != 4) {
                sprintf(buf, " - register 2 length %hhx (expected 4)",
                        adbDataLen);
                appendToDocument(documentPtr, buf);
                continue;
            }
            sprintf(buf, " - now $%02hhx%02hhx%02hhx%02hhx", adbData[0],
                    adbData[1], adbData[2], adbData[3]);
            appendToDocument(documentPtr, buf);

        } else if (address == 3 &&
                   (adbData[1] == 0x01 ||
                    adbData[1] == 0x02)) { /* Mouse - XXX testing */
            adbDataLen = 2;
            adbData[2] = (adbData[1] == 0x01) ? 0x02 : 0x01;
            adbData[1] = 0x00;
            listenADB(3, address);
            if (!talkADB(3, address)) {
                appendToDocument(documentPtr,
                                 " - register 3 talk request timed out");
                continue;
            }
            if (adbComplete && adbDataLen == 0) {
                appendToDocument(documentPtr, " - device disappeared?");
                continue;
            }
            sprintf(buf, " - changed to $%02hhx", adbData[1]);
            appendToDocument(documentPtr, buf);
        }
    }
}

tDocument *newDocument(const char *windowName)
{
    // windowName is a Pascal string with a length byte prefix
    tDocument *documentPtr;

    documentPtr = malloc(sizeof(tDocument));
    if (documentPtr == NULL) {
        showErrorAlert(MALLOC_ERROR_STRING, 0);
        return (NULL);
    }
    documentPtr->printRecordHandle =
        (PrRecHndl)NewHandle(PRINT_RECORD_SIZE, userID, 0, NULL);
    if (toolerror() != 0) {
        showErrorAlert(MALLOC_ERROR_STRING, toolerror());
        free(documentPtr);
        return NULL;
    }
    PrDefault(documentPtr->printRecordHandle);

    documentPtr->isOnDisk = FALSE;
    documentPtr->pathName = NULL;

    documentPtr->documentName[0] = windowName[0];
    strncpy(documentPtr->documentName + 1, windowName + 1, windowName[0]);

    documentPtr->wPtr =
        NewWindow2(documentPtr->documentName, 0, drawContents, NULL,
                   refIsResource, WINDOW_RESID, rWindParam1);
    if (documentPtr->wPtr == NULL) {
        showErrorAlert(NEW_WINDOW_ERROR_STRING, toolerror());
        DisposeHandle((Handle)documentPtr->printRecordHandle);
        free(documentPtr);
        return NULL;
    }

    documentPtr->nextDocument = documentList;
    documentPtr->prevDocument = NULL;
    if (documentList != NULL)
        documentList->prevDocument = documentPtr;

    documentList = documentPtr;

    scanADB(documentPtr);

    return documentPtr;
}

BOOLEAN isDocumentDirty(tDocument *documentPtr)
{
    BOOLEAN isDirty = FALSE;
    CtlRecHndl controlHdl;
    CtlRec *controlPtr;

    controlHdl = GetCtlHandleFromID(documentPtr->wPtr, CONTROL_TEXT_EDIT);
    if (toolerror() == 0) {
        HLock((Handle)controlHdl);
        controlPtr = *controlHdl;
        isDirty = ((controlPtr->ctlFlag & fRecordDirty) != 0);
        HUnlock((Handle)controlHdl);
    }
    return isDirty;
}

void clearDocumentDirty(tDocument *documentPtr)
{
    CtlRecHndl controlHdl;
    CtlRec *controlPtr;

    controlHdl = GetCtlHandleFromID(documentPtr->wPtr, CONTROL_TEXT_EDIT);
    if (toolerror() == 0) {
        HLock((Handle)controlHdl);
        controlPtr = *controlHdl;
        controlPtr->ctlFlag &= (~fRecordDirty);
        HUnlock((Handle)controlHdl);
    }
}

void saveDocument(tDocument *documentPtr)
{
    RefNumRecGS closeRecord;
    CreateRecGS createRecord;
    NameRecGS destroyRecord;
    OpenRecGS openRecord;
    IORecGS writeRecord;

    GrafPortPtr tmpPort;
    LongWord dataLength;
    Handle dataHandle;

    tmpPort = GetPort();
    SetPort(documentPtr->wPtr);
    dataLength = TEGetText(teTextIsNewHandle | teDataIsCString,
                           (Ref)&dataHandle, 0, 0, (Ref)NULL, NULL);
    if (toolerror() != 0) {
        showErrorAlert(SAVE_FILE_ERROR_STRING, toolerror());
        SetPort(tmpPort);
        return;
    }

    HLock((Handle)documentPtr->pathName);

    destroyRecord.pCount = 1;
    destroyRecord.pathname = &((*(documentPtr->pathName))->bufString);
    DestroyGS(&destroyRecord);

    createRecord.pCount = 5;
    createRecord.pathname = &((*(documentPtr->pathName))->bufString);
    createRecord.access = destroyEnable | renameEnable | readWriteEnable;
    createRecord.fileType = 0x04;
    createRecord.auxType = 0x0000;
    createRecord.storageType = 1;
    CreateGS(&createRecord);

    if (toolerror() != 0) {
        showErrorAlert(SAVE_FILE_ERROR_STRING, toolerror());
    } else {
        openRecord.pCount = 3;
        openRecord.pathname = &((*(documentPtr->pathName))->bufString);
        openRecord.requestAccess = writeEnable;
        OpenGS(&openRecord);

        if (toolerror() != 0) {
            showErrorAlert(SAVE_FILE_ERROR_STRING, toolerror());
        } else {
            HLock(dataHandle);

            writeRecord.pCount = 4;
            writeRecord.refNum = openRecord.refNum;
            writeRecord.dataBuffer = *dataHandle;
            writeRecord.requestCount = dataLength;
            WriteGS(&writeRecord);

            if (toolerror() != 0) {
                showErrorAlert(SAVE_FILE_ERROR_STRING, toolerror());
            } else {
                documentPtr->isOnDisk = TRUE;
                clearDocumentDirty(documentPtr);
            }

            HUnlock(dataHandle);

            closeRecord.pCount = 1; /* close the file */
            closeRecord.refNum = openRecord.refNum;
            CloseGS(&closeRecord);
        }
    }

    DisposeHandle(dataHandle);
    HUnlock((Handle)documentPtr->pathName);
    SetPort(tmpPort);
}

BOOLEAN loadDocument(tDocument *documentPtr)
{
    BOOLEAN result = TRUE;

    RefNumRecGS closeRecord;
    OpenRecGS openRecord;
    IORecGS readRecord;
    GrafPortPtr tmpPort;
    Handle dataHandle;

    openRecord.pCount = 12;
    HLock((Handle)documentPtr->pathName);
    openRecord.pathname = &((*(documentPtr->pathName))->bufString);
    openRecord.requestAccess = readEnable;
    openRecord.resourceNumber = 0;
    openRecord.optionList = NULL;
    OpenGS(&openRecord);

    if (toolerror() != 0) {
        showErrorAlert(OPEN_FILE_ERROR_STRING, toolerror());
        result = FALSE;
    } else {
        dataHandle = NewHandle(openRecord.eof, userID, attrLocked, NULL);

        if (toolerror() != 0) {
            showErrorAlert(MALLOC_ERROR_STRING, toolerror());
            result = FALSE;
        } else {
            readRecord.pCount = 4;
            readRecord.refNum = openRecord.refNum;
            readRecord.dataBuffer = *dataHandle;
            readRecord.requestCount = openRecord.eof;
            ReadGS(&readRecord);

            if (toolerror() != 0) {
                showErrorAlert(OPEN_FILE_ERROR_STRING, toolerror());
                result = FALSE;
            } else {
                tmpPort = GetPort();
                SetPort(documentPtr->wPtr);
                TESetText(teTextIsPtr | teDataIsTextBlock, (Ref)*dataHandle,
                          openRecord.eof, teCtlStyleIsPtr, NULL, NULL);
                SetPort(tmpPort);
            }
            DisposeHandle(dataHandle);
        }

        closeRecord.pCount = 1;
        closeRecord.refNum = openRecord.refNum;
        CloseGS(&closeRecord);
    }

    HUnlock((Handle)documentPtr->pathName);
    clearDocumentDirty(documentPtr);

    return result;
}

void closeDocument(GrafPortPtr wPtr)
{
    tDocument *documentPtr;
    char documentName[MAX_DOCUMENT_NAME];
    char *alertStrings[] = {documentName};
    int documentNameLen;
    char *tmpPtr;
    int buttonClicked;

    if (wPtr == NULL)
        return;

    documentPtr = findDocumentFromWindow(wPtr);
    if (documentPtr != NULL) {
        while (isDocumentDirty(documentPtr)) {
            /* The documentName in the documentPtr is actually a PString so the
               first byte is the length of the string. */
            tmpPtr = documentPtr->documentName;
            documentNameLen = *tmpPtr;
            tmpPtr++;

            /* The documentName has spaces before and after the real name to
               format the string for the window title bar.  Strip those spaces
               out and store the name into the documentName local array. */
            while ((documentNameLen > 0) &&
                   (tmpPtr[documentNameLen - 1] == ' ')) {
                documentNameLen--;
            }
            while (*tmpPtr == ' ') {
                tmpPtr++;
                documentNameLen--;
            }
            strncpy(documentName, tmpPtr, documentNameLen);
            documentName[documentNameLen] = '\0';

            buttonClicked =
                AlertWindow(awCString + awResource, (Pointer)alertStrings,
                            SAVE_BEFORE_CLOSING);
            switch (buttonClicked) {
                case 0:
                    doFileSave();
                    break;

                case 1:
                    clearDocumentDirty(documentPtr);
                    break;

                case 2:
                    return;
            }
        }
    }

    CloseWindow(wPtr);

    if (documentPtr == NULL)
        return;

    if (documentPtr->pathName != NULL) {
        DisposeHandle((Handle)documentPtr->pathName);
        documentPtr->pathName = NULL;
    }
    if (documentPtr->printRecordHandle != NULL) {
        DisposeHandle((Handle)documentPtr->printRecordHandle);
        documentPtr->printRecordHandle = NULL;
    }
    if (documentList == documentPtr) {
        documentList = documentPtr->nextDocument;
    } else if (documentPtr->prevDocument != NULL) {
        documentPtr->prevDocument->nextDocument = documentPtr->nextDocument;
    }

    if (documentPtr->nextDocument != NULL)
        documentPtr->nextDocument->prevDocument = documentPtr->prevDocument;

    free(documentPtr);
}

void printDocument(tDocument *documentPtr)
{
    GrafPortPtr printerPort;
    LongWord lineNumber;
    PrStatusRec printerStatus;
    Rect pageRect;

    printerPort = PrOpenDoc(documentPtr->printRecordHandle, NULL);
    if (toolerror() != 0) {
        showErrorAlert(PRINT_ERROR_STRING, toolerror());
        return;
    }

    HLock((Handle)documentPtr->printRecordHandle);
    pageRect = (*(documentPtr->printRecordHandle))->prInfo.rPage;
    HUnlock((Handle)documentPtr->printRecordHandle);

    if (toolerror() != 0) {
        showErrorAlert(PRINT_ERROR_STRING, toolerror());
    } else {
        lineNumber = 0;
        while (lineNumber != -1) {
            PrOpenPage(printerPort, NULL);
            if (toolerror() != 0) {
                showErrorAlert(PRINT_ERROR_STRING, toolerror());
                break;
            } else {
                lineNumber =
                    TEPaintText(printerPort, lineNumber, &pageRect, 0,
                                (Handle)GetCtlHandleFromID(documentPtr->wPtr,
                                                           CONTROL_TEXT_EDIT));
                PrClosePage(printerPort);
            }
        }
    }
    PrCloseDoc(printerPort);

    if (PrError() == 0)
        PrPicFile(documentPtr->printRecordHandle, NULL, &printerStatus);
}

void doAppleAbout(void)
{
    AlertWindow(awCString + awResource, NULL, ABOUT_ALERT_STRING);
}

void doFileNew(void) { newDocument(getWindowTitle()); }

void doFileOpen(void)
{
    tDocument *documentPtr;
    SFTypeList2 fileTypes;
    SFReplyRec2 reply;
    ResultBuf255Hndl nameHandle;

    /* By default, we want to open text files only which is what
       the following fileTypes request.  Customize as necessary. */
    fileTypes.numEntries = 1;
    fileTypes.fileTypeEntries[0].flags = 0x0000;
    fileTypes.fileTypeEntries[0].fileType = 0x04;
    fileTypes.fileTypeEntries[0].auxType = 0x0000;

    reply.nameRefDesc = refIsNewHandle;
    reply.pathRefDesc = refIsNewHandle;
    SFGetFile2(30, 30, refIsResource, OPEN_FILE_STRING, NULL, &fileTypes,
               &reply);
    if (toolerror() != 0) {
        showErrorAlert(OPEN_FILE_ERROR_STRING, toolerror());
        return;
    }

    if (reply.good) {
        nameHandle = (ResultBuf255Hndl)reply.nameRef;
        HLock((Handle)nameHandle);
        documentPtr = newDocument(documentNameFromGSOSString(*nameHandle));
        DisposeHandle((Handle)nameHandle);

        if (documentPtr == NULL) {
            DisposeHandle((Handle)reply.pathRef);
        } else {
            documentPtr->pathName = (ResultBuf255Hndl)reply.pathRef;
            documentPtr->isOnDisk = loadDocument(documentPtr);

            if (!documentPtr->isOnDisk)
                closeDocument(documentPtr->wPtr);
        }
    }
}

void doFileSaveAs(void)
{
    ResultBuf255Hndl nameHandle;
    SFReplyRec2 reply;
    const char *documentName;
    tDocument *documentPtr = findDocumentFromWindow(FrontWindow());

    if (documentPtr == NULL)
        return;

    reply.nameRefDesc = refIsNewHandle;
    reply.pathRefDesc = refIsNewHandle;

    if (documentPtr->pathName == NULL)
        SFPutFile2(30, 30, refIsResource, SAVE_FILE_STRING, refIsPointer,
                   (Ref) & (documentPtr->pathName), &reply);
    else
        SFPutFile2(30, 30, refIsResource, SAVE_FILE_STRING, refIsPointer,
                   (Ref) & ((*(documentPtr->pathName))->bufString), &reply);

    if (toolerror() != 0) {
        showErrorAlert(SAVE_FILE_ERROR_STRING, toolerror());
        return;
    }

    if (reply.good) {
        nameHandle = (ResultBuf255Hndl)reply.nameRef;
        HLock((Handle)nameHandle);
        documentName = documentNameFromGSOSString(*nameHandle);
        documentPtr->documentName[0] = documentName[0];
        strncpy(documentPtr->documentName + 1, documentName + 1,
                documentName[0]);
        DisposeHandle((Handle)nameHandle);

        SetWTitle(documentPtr->documentName, documentPtr->wPtr);

        documentPtr->pathName = (ResultBuf255Hndl)reply.pathRef;
        documentPtr->isOnDisk = TRUE;
        saveDocument(documentPtr);
    }
}

void doFileSave(void)
{
    tDocument *documentPtr = findDocumentFromWindow(FrontWindow());

    if (documentPtr == NULL)
        return;

    if (documentPtr->isOnDisk)
        saveDocument(documentPtr);
    else
        doFileSaveAs();
}

void doFileClose(void) { closeDocument(FrontWindow()); }

void doFilePageSetup(void)
{
    tDocument *documentPtr = findDocumentFromWindow(FrontWindow());

    if (documentPtr == NULL)
        return;

    PrStlDialog(documentPtr->printRecordHandle);
}

void doFilePrint(void)
{
    tDocument *documentPtr = findDocumentFromWindow(FrontWindow());

    if (documentPtr == NULL)
        return;

    if (PrJobDialog(documentPtr->printRecordHandle))
        printDocument(documentPtr);
}

void doFileQuit(void) { shouldQuit = TRUE; }

void doEditUndo(void)
{
    /* Nothing extra to do here.  The text edit control handles this for us. */
}

void doEditCut(void)
{
    /* Nothing extra to do here.  The text edit control handles this for us. */
}

void doEditCopy(void)
{
    /* Nothing extra to do here.  The text edit control handles this for us. */
}

void doEditPaste(void)
{
    /* Nothing extra to do here.  The text edit control handles this for us. */
}

void doEditClear(void)
{
    /* Nothing extra to do here.  The text edit control handles this for us. */
}

void handleMenu(void)
{
    Word menuNum;
    Word menuItemNum;

    menuNum = (Word)(myEvent.wmTaskData >> 16);
    menuItemNum = (Word)myEvent.wmTaskData;

    switch (menuItemNum) {
        case APPLE_ABOUT:
            doAppleAbout();
            break;

        case FILE_NEW:
            doFileNew();
            break;

        case FILE_OPEN:
            doFileOpen();
            break;

        case FILE_SAVE:
            doFileSave();
            break;

        case FILE_SAVE_AS:
            doFileSaveAs();
            break;

        case FILE_CLOSE:
            doFileClose();
            break;

        case FILE_PAGE_SETUP:
            doFilePageSetup();
            break;

        case FILE_PRINT:
            doFilePrint();
            break;

        case FILE_QUIT:
            doFileQuit();
            break;

        case EDIT_UNDO:
            doEditUndo();
            break;

        case EDIT_CUT:
            doEditCut();
            break;

        case EDIT_COPY:
            doEditCopy();
            break;

        case EDIT_PASTE:
            doEditPaste();
            break;

        case EDIT_CLEAR:
            doEditClear();
            break;
    }
    HiliteMenu(FALSE, menuNum);
}

void dimMenus(void)
{
    static BOOLEAN windowWasOpen = TRUE;
    static BOOLEAN applicationWindowWasInFront = TRUE;
    BOOLEAN windowIsOpen;
    BOOLEAN applicationWindowIsInFront;

    GrafPortPtr wPtr = FrontWindow();
    windowIsOpen = (wPtr != NULL);
    applicationWindowIsInFront = (findDocumentFromWindow(wPtr) != NULL);

    if ((windowIsOpen == windowWasOpen) &&
        (applicationWindowIsInFront == applicationWindowWasInFront)) {
        return;
    }

    windowWasOpen = windowIsOpen;
    applicationWindowWasInFront = applicationWindowIsInFront;

    if (windowIsOpen) {
        EnableMItem(FILE_CLOSE);
        SetMenuFlag(enableMenu, EDIT_MENU);
    } else {
        DisableMItem(FILE_CLOSE);
        SetMenuFlag(disableMenu, EDIT_MENU);
    }

    if (applicationWindowIsInFront) {
        EnableMItem(FILE_SAVE);
        EnableMItem(FILE_SAVE_AS);
        EnableMItem(FILE_PAGE_SETUP);
        EnableMItem(FILE_PRINT);
    } else {
        DisableMItem(FILE_SAVE);
        DisableMItem(FILE_SAVE_AS);
        DisableMItem(FILE_PAGE_SETUP);
        DisableMItem(FILE_PRINT);
    }

    DrawMenuBar();
}

void handleMessages(void)
{
#if MESSAGE_CENTER == 1
    Handle msgHandle;
    MessageRecGSPtr msgPtr;
    wStringPtr pathPtr;
    tDocument *documentPtr;
    ResultBuf255Ptr resultBufPtr;

    msgHandle = NewHandle(1, userid, 0, NULL);
    if (toolerror() != 0) {
        showErrorAlert(MALLOC_ERROR_STRING, toolerror());
        return;
    }

    MessageCenter(getMessage, fileInfoTypeGS, msgHandle);
    if (toolerror() != 0) {
        DisposeHandle(msgHandle);
        return;
    }

    MessageCenter(deleteMessage, fileInfoTypeGS, msgHandle);
    HLock(msgHandle);
    msgPtr = (MessageRecGSPtr)(*msgHandle);

    for (pathPtr = msgPtr->fileNames; pathPtr->length != 0;
         pathPtr = (wStringPtr)(pathPtr->text + pathPtr->length)) {
        documentPtr = newDocument(documentNameFromPath(pathPtr));
        if (documentPtr == NULL)
            continue;

        documentPtr->pathName =
            (ResultBuf255Hndl)NewHandle(pathPtr->length + 4, userid, 0, NULL);
        if (toolerror() != 0) {
            showErrorAlert(MALLOC_ERROR_STRING, toolerror());
            closeDocument(documentPtr->wPtr);
            continue;
        }
        HLock((Handle)documentPtr->pathName);
        resultBufPtr = *(documentPtr->pathName);
        resultBufPtr->bufSize = pathPtr->length + 4;
        resultBufPtr->bufString.length = pathPtr->length;
        memcpy(resultBufPtr->bufString.text, pathPtr->text, pathPtr->length);
        HUnlock((Handle)documentPtr->pathName);

        documentPtr->isOnDisk = loadDocument(documentPtr);

        if (!documentPtr->isOnDisk) {
            closeDocument(documentPtr->wPtr);
            continue;
        }

        if (msgPtr->printFlag) {
            doFilePrint();
            closeDocument(documentPtr->wPtr);
        }
    }

    if (msgPtr->printFlag)
        doFileQuit();

    DisposeHandle(msgHandle);
#endif
}

void initMenus(void)
{
    int height;
    MenuBarRecHndl menuBarHand;

    menuBarHand = NewMenuBar2(refIsResource, MENU_BAR, NULL);
    TOOLFAIL("Unable to create menu bar");

    SetSysBar(menuBarHand);
    TOOLFAIL("Unable to set system menu bar");

    SetMenuBar(NULL);
    TOOLFAIL("Unable to set menu bar");

    FixAppleMenu(APPLE_MENU);
    TOOLFAIL("Unable to fix Apple menu");

    height = FixMenuBar();
    TOOLFAIL("Unable to fix menu bar");

    DrawMenuBar();
    TOOLFAIL("Unable to draw menu bar");
}

void initGlobals(void)
{
    documentList = NULL;
    shouldQuit = FALSE;
    myEvent.wmTaskMask = 0x001F7FFF;
}

int main(void)
{
    int event;
    Ref toolStartupRef;

    userID = MMStartUp();
    TOOLFAIL("Unable to start memory manager");

    TLStartUp();
    TOOLFAIL("Unable to start tool locator");

    toolStartupRef = StartUpTools(userID, refIsResource, TOOL_STARTUP);
    TOOLFAIL("Unable to start tools");

    ADBStartUp();

    initGlobals();
    initMenus();
    InitCursor();

    handleMessages();

    while (!shouldQuit) {
        HandleDiskInsert(hdiScan | hdiHandle, 0);
        dimMenus();
        event = TaskMaster(everyEvent, &myEvent);
        TOOLFAIL("Unable to handle next event");

        switch (event) {
            case wInSpecial:
            case wInMenuBar:
                handleMenu();
                break;
            case wInGoAway:
                closeDocument((GrafPortPtr)myEvent.wmTaskData);
                break;
        }
    }

    ADBShutDown();

    ShutDownTools(refIsHandle, toolStartupRef);
    TOOLFAIL("Unable to shut down tools");

    TLShutDown();
    TOOLFAIL("Unable to shut down tool locator");

    MMShutDown(userID);
    TOOLFAIL("Unable to shut down memory manager");
}

#append "receive.s"
