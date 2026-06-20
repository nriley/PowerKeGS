	case	on

patchUserShutDown START

Temp	equ 1e

;	brk $ea

	phd		; save direct page register to stack [offset 2]
	pha		; space for my vars
	pha
	tsc
	tcd

	pha
	pha
	pea 0		; system
	pea $11		; GS/OS Loader (tool $11)
	~GetTSPtr

	pla
	sta <Temp
	sta old_table

	pla
	sta <Temp+2
	sta old_table+2 

	lda [<Temp]	; # of functions in tool set

	sta FPT

	pea 0		; user tool set
	pea $11		; GS/OS Loader (tool $11)
	pea FPT|-16
	pea FPT

	~SetTSPtr

	inc active

quit	pla
	pla
	pld
	rtl


active		dc i2'0'
old_table	ds 4

UserShutDown	anop

*  |-----------
*  | rtlb         (4)
*  |-----------
*  | flag         (4)
*  |-----------
*  | userID       (4)
*  |-----------
*  | Space        (4)
*  |-----------
*  | Previous contents
*  |
*  |

	phb		;;even up stack
	pla
	sta 5,s		;;move the return address
	pla
	sta 5,s
	pla		;;clean up the stack
	pla
	plb

FPT	dc i4'$40'	; function pointer table
	dc i4'LoaderInitialization-1' ; 01
	ds $10*4	;functions I don't care about
	dc i4'UserShutDown-1' ; 12
	ds 20*4		;extra room if needed...

	END
