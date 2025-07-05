;
;  completion.s
;  ADB Explorer
;
;  Created by Nicholas Riley on 7/4/2025.
;
;

	mcopy completion.macros
	keep completion

	case on		; case-sensitive to match C code

receiveRegister3 start
	longa off	; 8-bit mode
	longi off

	phb		; save data bank register to stack [offset 1]
	phd		; save direct page to stack [offset 2]

	tsc		; SP to accumulator C
	tcd		; and to direct register

	phk		; load our data bank
	plb		; make it the data bank

	lda [7]		; data length [offset 4-6 is RTL address]
	sta adbRegister3Len
	beq exit	; no data
	tay		; y <- length (NOT length + 1 as in sample code)

loop	lda [7],y
	dey
	sta adbRegister3,y
	bne loop
	bra exit

exit	inc adbComplete
	pld		; restore
	plb
	clc

	rtl
	end

	longa on       ; 16-bit mode
	longi on
