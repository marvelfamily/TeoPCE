; Indirect C calls on HuC6280: the direct page is at CPU $2000.
; Matches cc65 libsrc/pce/call.s (von Bassewitz / King), not NES callax.
.export callax
.importzp ptr1
.segment "CODE"
callax:
    sta ptr1
    stx ptr1+1
    jmp (ptr1+$2000)
