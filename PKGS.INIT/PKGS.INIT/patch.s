	mcopy patch.macros
	keep patch

	case on

patchWindStatus START

Temp	equ 1

	phd		; save direct page register to stack [offset 2]
	pha		; make space for variables
	pha
	tsc
	tcd

	pha
	pha
	~GetTSPtr 0,#$0e ; system tool, Window Manager (tool $0e)

	pla
	sta <Temp
	sta old_table

	pla
	sta <Temp+2
	sta old_table+2 

	lda [<Temp]	; # of functions in tool set

	sta FPT

	~SetTSPtr 0,#$0e,FPT

	inc active

quit	pla
	pla
	pld
	rtl


active		dc i2'0'
old_table	ds 4

* Technically unneeded as this is all WindBootInit does, too
* (recommended by IIgs Tech Note 101: Patching the Toolbox)
WindBootInit	anop
	lda #0		; invoked by SetTSPtr
	clc
	rtl

WindStatus	anop
	jsl powerOff
	lda #0
	clc
	rtl

FPT	dc i4'$6d'	; size of Window Manager function pointer table (overwritten)
	dc i4'WindBootInit-1' ; 01
	ds $04*4	; no change
	dc i4'WindStatus-1' ; 06
	ds $67*4	; remainder of table (as of GS/OS 6.0.1)

	END
