;
;  p8switch.s
;  PKGS.Init
;
;  Created by Nicholas Riley on 6/20/2026.
;
;

        mcopy p8switch.macros
        keep p8switch

	case on		; case-sensitive to match C code

p8switch start
	cmp #$8000	; Shut Down
	bne exit	; if not, we're done
	jsl patchWindStatus

exit    rtl
        end
