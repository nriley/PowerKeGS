/*
 * main.c
 * PKGS.Init
 *
 * Created by Nicholas Riley on 7/26/2025.
 * Copyright (c) 2025 Nicholas Riley. All rights reserved.
 *
 */

#pragma rtl

#include <orca.h>
#include <locator.h>
#include <misctool.h>
#include <stdio.h>
#include <string.h>
#include <gsbug.h>

const char bootInfoString[] = "PKGS.Init             v1.0d1";
LongWord version = 0x01006001; /* in rVersion format - XXX this is wrong */

const Word os_p8_switch = 0x2d;

extern Word *unloadFlagPtr;

extern void p8switch(void);
extern void patchWindStatus(void);

void setUnloadFlag(void)
{
    if (*unloadFlagPtr == 0)
        *unloadFlagPtr = 1;
}

void powerOff(void)
{
    SysBeep();
}

int main(void)
{
    ShowBootInfo((Pointer)bootInfoString, NULL /* iconPtr */);

    Pointer prevAddr = GetVector(os_p8_switch);
    SetVector(os_p8_switch, (Pointer)p8switch);

    return 0;
}
