; SPDX-License-Identifier: MIT
; Own one-shot stub for exactly menu SHAa1803cb4...73cbf3. No OS code copied.
; Entered only by original native main JSR at CPU91FF after full init.
.setcpu "HuC6280"
.segment "CODE"
.export entry, fail_closed, fallback, code_end

MEM_ADDR = $1FF8
MEM_DATA = $1FF6
PATH_ROM = $D400
PUSH_BYTE = $B3E3
PUSH_WORD = $B3F9
ALLOC = $9F72
FREE = $9FC7
INSTALL = $6A09
START = $6AEF
STOCK_UI = $7CD6

entry:
    php
    sei
    ; ed_mem_wrd(ADDR_MENU+$51FF, originalJSR, 3), direct PIO only.
    lda #$FF
    sta MEM_ADDR
    lda #$51
    sta MEM_ADDR
    lda #$FE
    sta MEM_ADDR
    lda #$80
    sta MEM_ADDR
    lda #$20
    sta MEM_DATA
    lda #$D6
    sta MEM_DATA
    lda #$7C
    sta MEM_DATA
    ; Published ed_mem_wrd always resets the address/WP latch.
    stz MEM_ADDR
    stz MEM_ADDR
    stz MEM_ADDR
    stz MEM_ADDR
    ; Native system mapper permits CPU reads, but not ordinary CPU writes.
    lda $91FF
    cmp #$20
    bne fail_closed
    lda $9200
    cmp #$D6
    bne fail_closed
    lda $9201
    cmp #$7C
    bne fail_closed
    plp
    bra save_context
fail_closed:
    sei
@idle:
    bra @idle

save_context:
    ; Hardware stack keeps our state across native C routines and their ZP use.
    tma #$04
    pha
    lda $3B9C
    pha
    lda $30
    pha
    lda $31
    pha
    ; Native allocator is fatal on overflow. Reserve the native512 path buffer.
    lda $2395
    cmp #$16
    bcc allocate_path
    bne fallback
    lda $2394
    bne fallback
allocate_path:
    lda #$00
    ldx #$02
    jsr PUSH_WORD
    jsr ALLOC
    sta $38
    stx $39
    ldy #$00
@copy:
    lda PATH_ROM,y
    sta ($38),y
    beq @copied
    iny
    bne @copy
    ; Catalog restricts paths to127 ASCII bytes. Corruption still cannot overrun.
    lda #$01
    pha
    bra release_path
@copied:
    lda $38
    ldx $39
    jsr PUSH_WORD
    lda #$01
    jsr PUSH_BYTE
    jsr INSTALL
    pha
release_path:
    lda #$00
    ldx #$02
    jsr PUSH_WORD
    jsr FREE
    pla
    bne fallback
    ; Exactly native '*s' path: native CartCfg, cheats, BRAM and reset helper.
    jsr START
    ; Any returned start goes to stock UI; the call site is already disarmed.
fallback:
    pla
    sta $31
    pla
    sta $30
    pla
    sta $3B9C
    pla
    tam #$04
    ; Tailcall retains original main JSR's hardware return address9201.
    jmp STOCK_UI
code_end:
