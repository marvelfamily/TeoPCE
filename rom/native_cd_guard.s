; Bounded native-generator adapter. No persistent game RAM; exact pinned caller only.
.setcpu "HuC6280"
.include "constants.inc"
.export callback, disarm, failed
.macro eq value
    cmp #value
    beq :+
    jmp failed
:
.endmacro
.segment "CALLBACK"
callback:
    jsr disarm
    ldy #0
    lda ($30),y
.if DYNAMIC
    cmp $d600
    beq :+
    jmp failed
:
    lda $d600
    bne :+
    jmp failed
:
    cmp #129
    bcc :+
    jmp failed
:
    lda $d601
    bne :+
    jmp failed
:
    cmp #129
    bcc :+
    jmp failed
:
.else
    eq STOCK_LEN
.endif
    iny
    lda ($30),y
    eq 0
    iny
    lda ($30),y
    eq 0
    iny
    lda ($30),y
    eq 0
    iny
    lda ($30),y
    sta $38
    iny
    lda ($30),y
    sta $39
    cmp #$22
    bcs :+
    jmp failed
:
    cmp #$40
    bcc :+
    jmp failed
:
    cmp #$3f
    bne @pointer_ok
    lda $38
.if DYNAMIC
    clc
    adc $d600
    bcc @pointer_ok
    beq @pointer_ok
.else
    cmp #(257-STOCK_LEN)
    bcc @pointer_ok
.endif
    jmp failed
@pointer_ok:
    iny
    lda ($30),y
    eq 0
    iny
    lda ($30),y
    eq 1
    iny
    lda ($30),y
    eq $80
    iny
    lda ($30),y
    eq 1
    ldy #0
@packet:
    lda ($38),y
    cmp $d580,y
    beq :+
    jmp failed
:
    iny
.if DYNAMIC
    cpy $d600
.else
    cpy #STOCK_LEN
.endif
    bne @packet
    ldy #0
.if DYNAMIC
    lda $d601
.else
    lda #GUARD_LEN
.endif
    sta ($30),y
    ldy #4
    lda #0
    sta ($30),y
    iny
    lda #$d5
    sta ($30),y
    jmp $f064
failed:
    sei
@idle: bra @idle
.segment "DISARM"
disarm:
    php
    sei
    tma #4
    pha
    lda #$86
    sta $1ff8
    lda #$4c
    sta $1ff8
    lda #$ff
    sta $1ff8
    lda #$80
    sta $1ff8
    lda #$20
    sta $1ff6
    lda #$64
    sta $1ff6
    lda #$f0
    sta $1ff6
    stz $1ff8
    stz $1ff8
    stz $1ff8
    stz $1ff8
    lda #$0a
    tam #4
    lda $4c86
    eq $20
    lda $4c87
    eq $64
    lda $4c88
    eq $f0
    pla
    tam #4
    plp
    rts
