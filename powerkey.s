*
* PowerKeGS
* A test app to control the Sophisticated Circuits PowerKey on the Apple IIgs
*
* (c) 2025 Nicholas Riley
* Based on ADB Radio code (c) 2023 Brutal Deluxe Software (brutaldeluxe.fr)
*

                        xc
                        xc
                        mx  %00

                        rel
                        lst off

*----------

                        use 4/ADB.Macs
                        use 4/Int.Macs
                        use 4/Locator.Macs
                        use 4/Mem.Macs
                        use 4/Misc.Macs
                        use 4/Text.Macs
                        use 4/Util.Macs

GSOS                    =   $e100a8

*---------- ADB
* [references from fmradio.s removed]
*
*  Tashtari's notes
*   https://github.com/lampmerchant/tashnotes/blob/main/macintosh/adb/protocols/powerkey.md
*
* ADB commands
*  Talk: from a device to the host - It means: "Device, talk to me, send me data"
*  Listen: from the host to a device - It means: "Device, listen to me, eat my data"
*
*  Command syntax
*   7654 32 10 Command
*   --------------------
*   xxxx 00 00 SENDRESET
*   ADDR 00 01 FLUSH
*   XXXX 00 10 RESERVED
*   XXXX 00 11 RESERVED
*   XXXX 01 XX RESERVED
*   ADDR 10 RR LISTEN
*   ADDR 11 RR TALK
*
*   ADDR = 0..15
*   RR = 0..3
*

*---------- Apple IIgs SendInfo ADB Commands (the ADB Tool Set *only*)

transmitADBBytes        =   %01000111               ; $47
enableSRQ               =   %01010000               ; $50
disableSRQ              =   %01110000               ; $70
listen                  =   %10000000               ; $80
talk                    =   %11000000               ; $C0

*---------- Error codes

cmdIncomplete           =   $0910                   ; Command not completed
cantSync                =   $0911                   ; Can't synchronize with system
adbBusy                 =   $0982                   ; ADB busy (command pending)
devNotAtAddr            =   $0983                   ; Device not present at address
srqListFull             =   $0984                   ; SRQ list full

* Register 0 - Status (read only except bit 8; see below)
*   Bit  Description
*  15-9  Always zero
*     8  If set, Talk 0 returns the contents of 0; if clear, returns nothing
*     7  If set, relay is closed and outlets are powered; if clear, relay is open and outlets are not powered
*     6  If set, relay was last closed as a result of register 1 overflowing
*     5  If set, relay was last closed as a result of the power key being pressed on the keyboard
*     4  Always zero
*   3-0  Firmware version (0b0000 = 1.1, incrementing by 0.1 through 0b1111 = 2.6; 1.2 and 1.4 have been observed)
*
* Registers 1 and 2 - 32-bit timers (read/write)
*
*  Set bit 31 of a register to enable its timer
*  Register 1 turns off power (opens relay) when it overflows
*  Register 2 turns on power (closes relay) when it overflows
*
* Register 3 - Status and device identification information
*
*  For PowerKey Classic (PK-1), this is 0x6200
*   Default address is 0x7 ("Appliance") - FM Radio also uses this; we don't handle collisions (address 8-15)
*   Default handler ID is 0x22
*
* -------------------------------
* 1 1 1 1 1
* 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
* -------------------------------
* 0 ! ! 0 ------- ---------------
*   ! !   Address Dev Handler ID (theoretically can be changed to modify behavior)
*   ! > Service Request enable (always 0 = disabled)
*   > Exceptional event (always 1 = not used)
**
*----------------------------
* ENTRY POINT
*----------------------------

                        phk
                        plb

                        clc
                        xce
                        rep #$30

                        _TLStartUp
                        pha
                        _MMStartUp
                        pla
                        sta appID
                        ora #$0100
                        sta myID

                        _MTStartUp
                        _TextStartUp
                        _IMStartUp
                        _ADBStartUp

*----------

                        PushWord #$00FF
                        PushWord #$0080
                        _SetInGlobals
                        PushWord #$00FF
                        PushWord #$0080
                        _SetOutGlobals
                        PushWord #$00FF
                        PushWord #$0080
                        _SetErrGlobals

                        PushWord #0
                        PushLong #3
                        _SetInputDevice
                        PushWord #0
                        PushLong #3
                        _SetOutputDevice
                        PushWord #0
                        PushLong #3
                        _SetErrorDevice

                        PushWord #0
                        _InitTextDev
                        PushWord #1
                        _InitTextDev
                        PushWord #2
                        _InitTextDev

                        PushWord #$0c               ; home
                        _WriteChar

*----------------------------
* MENU: MAIN
*----------------------------

mainMENU
                        PushLong #strMAINMENU
                        _WriteCString

                        jsr waitFORKEY
                        cmp #"Q"
                        beq doQUIT
                        cmp #"q"
                        beq doQUIT
                        cmp #"1"
                        bne mmNOT1
                        jmp statusMENU
mmNOT1                  cmp #"2"
                        bne mainMENU
                        jmp powerOffMENU

*----------------------------
* QUIT PROGRAM
*----------------------------

doQUIT
                        _ADBShutDown
                        _IMShutDown
                        _TextShutDown
                        _MTShutDown

                        PushWord myID
                        _DisposeAll

                        PushWord appID
                        _MMShutDown

                        _TLShutDown

                        jsl GSOS
                        dw  $2029
                        adrl proQUIT

                        brk $bd

*---------- Data

strMAINMENU
                        asc 0d'PowerKeGS'0d
                        asc '(c) 2025, Nicholas Riley'0d
                        asc ' 1. Display status'0d
                        asc ' 2. Power off'0d
                        asc ' Q. Quit'0d00

*----------------------------
* MENU: DISPLAY STATUS
*----------------------------
*

statusMENU
                        jmp mainMENU

*----------------------------
* MENU: POWER OFF
*----------------------------
* Turn the connected devices off
* (delay to wait for GS/OS to power off?)

powerOffMENU
                        jsr PKSendRegister1
                        jmp mainMENU

*------------------------------------------------
*
* ADB TOOL SET ROUTINES
*
*------------------------------------------------

*-----------------------
* LISTEN routines to send
* data to the PowerKey
*-----------------------

PKSendRegister0                                     ; get status

]lp                     PushWord #1+2               ; command (1) + data (2)
                        PushLong #dataListen0
                        PushWord #transmitADBBytes+2 ; + number of data bytes
                        _SendInfo
                        bcc FMLR0OK
                        cmp #adbBusy
                        beq ]lp
FMLR0OK                 rts

*-----------

dataListen0
                        dfb %0111_10_00             ; ADB command: Address (0111) + Listen (10) + Register (00)
                        dfb %0000_00_01             ; set bit 8
                        dfb %0000_00_00

*-----------

PKSendRegister1                                     ; send power off timer setting to PowerKey

]lp                     PushWord #1+4               ; command (1) + data (4)
                        PushLong #dataListen1
                        PushWord #transmitADBBytes+4 ; + number of data bytes
                        _SendInfo
                        bcc FMLR1OK
                        cmp #adbBusy
                        beq ]lp
FMLR1OK                 rts

*-----------

dataListen1
                        dfb %0111_10_01             ; ADB command: Address (0111) + Listen (10) + Register (01)
powerOffTimer           hex ffffffff                ; turn on timer and immediately overflow (power off)

*-----------------------

PKSendRegister2                                     ; send power on timer setting to PowerKey

]lp                     PushWord #1+4               ; command (1) + data (4)
                        PushLong #dataListen2
                        PushWord #transmitADBBytes+4 ; + number of data bytes
                        _SendInfo
                        bcc FMLR2OK
                        cmp #adbBusy
                        beq ]lp
FMLR2OK                 rts

*-----------

dataListen2
                        dfb %0111_10_10             ; ADB command: Address (0111) + Listen (10) + Register (10)
powerOnTimer            hex 00000000                ; turn off timer

*-----------------------
* TALK routines to get
* data from the radio
*-----------------------

PKTalkRegister0                                     ; ask the PowerKey to talk to me on register 0

                        lda #dataTalk0
                        sta cpBuffer+1

]lp                     PushLong #completionRoutine
                        PushWord #%11_00_0111       ; talk (11) + register (00) + address (0111)
                        _AsyncADBReceive
                        bcc FMTR0OK
                        cmp #adbBusy
                        beq ]lp
FMTR0OK                 rts

*-----------

dataTalk0               ds  1                       ; always zero
                        ds  1                       ; status/firmware version
                        ds  6                       ; padding (needed?)

*-----------------------

FMTalkRegister1                                     ; ask the PowerKey to talk to me on register 1

                        lda #dataTalk1
                        sta cpBuffer+1

]lp                     PushLong #completionRoutine
                        PushWord #%11_01_0111       ; talk (11) + register (01) + address (0111)
                        _AsyncADBReceive
                        bcc FMTR1OK
                        cmp #adbBusy
                        beq ]lp
FMTR1OK                 rts

*-----------

dataTalk1               ds  4                       ; timer value
                        ds  4                       ; padding (needed?

*-----------------------

FMTalkRegister2                                     ; ask the PowerKey to talk to me on register 2

                        lda #dataTalk2
                        sta cpBuffer+1

]lp                     PushLong #completionRoutine
                        PushWord #%11_10_0111       ; talk (11) + register (10) + address (0111)
                        _AsyncADBReceive
                        bcc FMTR2OK
                        cmp #adbBusy
                        beq ]lp
FMTR2OK                 rts

*-----------

dataTalk2               ds  4                       ; timer value
                        ds  4                       ; padding (needed?)

*-----------------------

fmTalkRegister3                                     ; ask the PowerKey to talk to me on register 3

                        lda #dataTalk3
                        sta cpBuffer+1

]lp                     PushLong #completionRoutine
                        PushWord #%11_11_0111       ; talk (11) + register (11) + address (0111)
                        _AsyncADBReceive
                        bcc FMTR3OK
                        cmp #adbBusy
                        beq ]lp
FMTR3OK                 rts

*-----------

dataTalk3               ds  2                       ; address & handler
                        ds  6                       ; padding

*-----------------------

                        mx  %11                     ; enter the 8-bit world

completionRoutine                                   ; nearly standard ADB completion routine
                        phd
                        tsc
                        tcd
                        lda [6]
                        beq endpool
                        tay
                        iny
]lp                     lda [6],y
                        tyx
cpBuffer                stal dataTalk0,x            ; patch this address please
                        dey
                        bne ]lp
endpool                 pld
                        rtl

                        mx  %00                     ; back to the 16-bit world

*------------------------------------------------
*
* SUB-ROUTINES
*
*------------------------------------------------

*----------------------------
* TEXT ROUTINES
*----------------------------

*---------- Display a word

showWORD                pha                         ; from a word to a string
                        pha
                        pha                         ; <= here, really
                        _HexIt
                        PullLong strHEX

                        PushLong #strHEX            ; show the string
                        _WriteCString
                        rts

*--- Data

strHEX                  asc '0000'00

*---------- Wait for a key in a range 0-Acc
* A: high key
* X: high ptr to C string
* Y: low ptr to C string

keyINRANGE              sta keyHIGH
                        sty strKEY
                        stx strKEY+2

]lp                     PushLong strKEY
                        _WriteCString

                        PushWord #0
                        PushWord #1                 ; echo char
                        _ReadChar
                        pla
                        and #$ff
                        cmp #"0"
                        bcc ]lp
                        cmp keyHIGH
                        bcc keyINRANGE9
                        beq keyINRANGE9
                        bra ]lp

keyINRANGE9             sec
                        sbc #"0"
                        pha
                        bra waitKEY8

*--- Data

strKEY                  ds  4                       ; pointer to string
keyHIGH                 ds  2

*---------- Wait for a key

waitKEY                 PushWord #$0d
                        _WriteChar

                        PushWord #0
                        PushWord #0                 ; don't echo char
                        _ReadChar
                        bra waitKEY1                ; go below

*---------- Wait for a key

waitFORKEY              PushLong #strINPUT
                        _WriteCString

                        PushWord #0                 ; wait for key
                        PushWord #1                 ; echo char
                        _ReadChar

waitKEY1                lda 1,s                     ; check CR
                        and #$ff                    ; of typed
                        sta 1,s                     ; in char
                        cmp #$8d
                        beq waitKEY9

waitKEY8                PushWord #$0d               ; return
                        _WriteChar

waitKEY9                pla                         ; restore entered char
                        rts

*--- Data

strINPUT                asc 'Select an entry: '00

*----------------------------
* DATA
*----------------------------

*--- GS/OS

proQUIT                 dw  2                       ; pcount
                        ds  4                       ; pathname
                        ds  2                       ; flags

*---------- Application

appID                   ds  2
myID                    ds  2
