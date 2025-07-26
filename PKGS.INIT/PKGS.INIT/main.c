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
#include <misctool.h>
#include <stdio.h>
#include <string.h>
#include <types.h>
#include <gsbug.h>

const char bootInfoString[] = "PKGS.Init             v1.0d1";
LongWord version = 0x01006001; /* in rVersion format */

extern Word *unloadFlagPtr;

void setUnloadFlag(void)
{
    if (*unloadFlagPtr == 0)
        *unloadFlagPtr = 1;
}

int main(void)
{
    ShowBootInfo((Pointer)bootInfoString, NULL /* iconPtr */);

#if 0
    Str255 msg = {0, "It worked!"};
    msg.textLength = strlen((const char *)msg.text);
    DebugStr(&msg);
#endif

    return 0;
}
