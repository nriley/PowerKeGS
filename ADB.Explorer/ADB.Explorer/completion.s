;
;  completion.s
;  ADB Explorer
;
;  Created by Nicholas Riley on 7/4/2025.
;
;

	mcopy completion.macros
	keep completion

	case on

receiveRegister3 start
	phb		; save data bank register to stack
	phd		; save direct register to stack

	tsc		; push return address to stack
	tcd		; push direct page to stack

	lda [6]		; length
	sta adbRegister3Len
	beq noData	; no data
	tay		; y <- length 
	iny		; y <- length + 1

	inc adbHasData

loop	lda [6],y
	sta adbRegister3,y
	dey
	bne loop
	bra exit

noData	inc adbNoData

exit	brk
	pld
	plb
	clc

	rtl
	end
