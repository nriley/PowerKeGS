/*
 * main.c
 * PKGS.Test
 *
 * Created by Nicholas Riley on 7/3/2026.
 * Copyright (c) 2026 ___ORGANIZATIONNAME___. All rights reserved.
 *
 */



#include <Memory.h>
#include <Locator.h>
#include <Desk.h>
#include <Resources.h>
#include <STDFile.h>
#include <GSOS.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "pkgs.h"

int main(void)
{
    int event;
    Ref toolStartupRef;
    
    unsigned int userid = MMStartUp();
    TOOLFAIL("Unable to start memory manager");
    
    TLStartUp();
    TOOLFAIL("Unable to start tool locator");
    
    powerOff();
    
    TLShutDown();
    TOOLFAIL("Unable to shutdown tool locator");
    
    MMShutDown(userid);
    TOOLFAIL("Unable to shutdown memory manager");
}
