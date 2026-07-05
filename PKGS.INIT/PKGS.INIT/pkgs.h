/*
 *  pkgs.h
 *  PKGS.Init
 *
 *  Created by Nicholas Riley on 7/3/2026.
 * 
 */

#ifndef _GUARD_PROJECTPKGS_Init_FILEpkgs_
#define _GUARD_PROJECTPKGS_Init_FILEpkgs_

#include <MiscTool.h>
#include <orca.h>

#define TOOLFAIL(string)        \
    if (toolerror())            \
        SysFailMgr(toolerror(), \
            (Pointer) "\p" string "\n\r    Error Code -> $");

BOOLEAN powerOff(void);

#endif /* define _GUARD_PROJECTPKGS_Init_FILEpkgs_ */
