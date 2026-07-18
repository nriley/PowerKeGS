/*
 * main.c
 * PKGS.Init
 *
 * Created by Nicholas Riley on 7/26/2025.
 * Copyright (c) 2025 Nicholas Riley. All rights reserved.
 *
 */

#ifdef __PKGS_INIT__

#pragma rtl

#include "pkgs.h"

const char bootInfoString[] = "PKGS.Init             v1.0.1";

const Word os_p8_switch = 0x2d;

extern Word* unloadFlagPtr;

extern void p8switch(void);
extern void patchWindStatus(void);

void setUnloadFlag(void)
{
    if (*unloadFlagPtr == 0)
        *unloadFlagPtr = 1;
}

int main(void)
{
    ShowBootInfo((Pointer)bootInfoString, NULL /* iconPtr */);

    Pointer prevAddr = GetVector(os_p8_switch);
    SetVector(os_p8_switch, (Pointer)p8switch);

    return 0;
}

#endif
