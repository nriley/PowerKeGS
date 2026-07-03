	case on

dummy	private
	bra InitStart
	end

; As documented in Apple II File Type Note for File Type $B6
; in order to permit unloading if hardware is not detected
unloadFlagPtr data
	ds 4
	end

InitStart private
	tay
	tsc
	clc
	adc #4
	sta >unloadFlagPtr
	lda #0
	sta >unloadFlagPtr+2
	tya
	end
