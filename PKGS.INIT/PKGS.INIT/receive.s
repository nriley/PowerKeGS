;
;  receive.s (large memory model)
;  PKGS.Init
;
;  Created by Nicholas Riley on 7/16/2026.
;
;

	mcopy receive.macros
	keep receive

	case on		; case-sensitive to match C code

receiveRegister start
	longa off	; 8-bit accumulator
	longi off       ; 8-bit index register

	phb		; save data bank register to stack [offset 1]
	phd		; save direct page register to stack [offset 2]

	tsc		; SP to accumulator C
	tcd		; then to direct page register

	lda [7]		; 0 or data length - 1 [offset 4-6 is RTL address]
	beq exit	; no data (doesn't touch adbDataLen)
	ina		; data length

	sta >adbDataLen

	tay		; y <- length

loop	lda [7],y
	dey

	tyx
	sta >adbData,x

	bne loop

exit	lda #$01
	sta >adbComplete
	pld		; restore direct page register from stack
	plb		; restore data bank register from stack
	clc

	rtl
	end

	longa on	; 16-bit mode
	longi on
