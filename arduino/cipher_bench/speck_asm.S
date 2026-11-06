; SPECK64/128 round functions in AVR assembly.
;
; void speck_encrypt_asm(uint32_t *blk, const uint32_t *rk);
; void speck_decrypt_asm(uint32_t *blk, const uint32_t *rk);
;
; avr-gcc ABI: first arg in r25:r24 (blk), second in r23:r22 (rk).
; Registers used (all call-clobbered, nothing needs saving):
;   x word = r18 (lowest byte) .. r21 (highest byte)
;   y word = r22 (lowest byte) .. r25 (highest byte)
;   r0 = scratch, r1 = zero register (never modified), r26 = loop counter,
;   X (r27:r26) / Z (r31:r30) = pointers.
;
; Trick: ROR8 / ROL8 of a 32-bit word is only a renaming of its bytes, so no
; byte is ever moved. The macros take the CURRENT role of the four x registers
; and the next round is invoked with the registers rotated by one position.
; After 4 rounds the roles are back to the original ones.

        .macro ENC_ROUND x0, x1, x2, x3
        ; x = (ROR8(x) + y) ^ k   (result bytes live in x1, x2, x3, x0)
        add     \x1, r22
        adc     \x2, r23
        adc     \x3, r24
        adc     \x0, r25
        ld      r0, Z+
        eor     \x1, r0
        ld      r0, Z+
        eor     \x2, r0
        ld      r0, Z+
        eor     \x3, r0
        ld      r0, Z+
        eor     \x0, r0
        ; y = ROL3(y)
        lsl     r22
        rol     r23
        rol     r24
        rol     r25
        adc     r22, r1
        lsl     r22
        rol     r23
        rol     r24
        rol     r25
        adc     r22, r1
        lsl     r22
        rol     r23
        rol     r24
        rol     r25
        adc     r22, r1
        ; y ^= x
        eor     r22, \x1
        eor     r23, \x2
        eor     r24, \x3
        eor     r25, \x0
        .endm

        .macro DEC_ROUND x0, x1, x2, x3
        ; y = ROR3(y ^ x)
        eor     r22, \x0
        eor     r23, \x1
        eor     r24, \x2
        eor     r25, \x3
        bst     r22, 0
        lsr     r25
        ror     r24
        ror     r23
        ror     r22
        bld     r25, 7
        bst     r22, 0
        lsr     r25
        ror     r24
        ror     r23
        ror     r22
        bld     r25, 7
        bst     r22, 0
        lsr     r25
        ror     r24
        ror     r23
        ror     r22
        bld     r25, 7
        ; x = ROL8((x ^ k) - y)   (round keys are read backwards: byte 3 first)
        ld      r0, -Z
        eor     \x3, r0
        ld      r0, -Z
        eor     \x2, r0
        ld      r0, -Z
        eor     \x1, r0
        ld      r0, -Z
        eor     \x0, r0
        sub     \x0, r22
        sbc     \x1, r23
        sbc     \x2, r24
        sbc     \x3, r25
        ; new roles after ROL8: (x3, x0, x1, x2)
        .endm

        .section .text.speck_encrypt_asm,"ax",@progbits
        .global speck_encrypt_asm
        .type   speck_encrypt_asm, @function
speck_encrypt_asm:
        push    r24
        push    r25
        movw    r30, r22            ; Z = rk
        movw    r26, r24            ; X = blk
        ld      r18, X+
        ld      r19, X+
        ld      r20, X+
        ld      r21, X+
        ld      r22, X+
        ld      r23, X+
        ld      r24, X+
        ld      r25, X+
        ldi     r26, 6              ; 6 * 4 rounds + 3 rounds = 27
.Lenc_loop:
        ENC_ROUND r18, r19, r20, r21
        ENC_ROUND r19, r20, r21, r18
        ENC_ROUND r20, r21, r18, r19
        ENC_ROUND r21, r18, r19, r20
        dec     r26
        breq    .Lenc_tail
        rjmp    .Lenc_loop
.Lenc_tail:
        ENC_ROUND r18, r19, r20, r21
        ENC_ROUND r19, r20, r21, r18
        ENC_ROUND r20, r21, r18, r19
        ; after 27 rounds the x bytes (low..high) are in r21, r18, r19, r20
        pop     r27
        pop     r26
        st      X+, r21
        st      X+, r18
        st      X+, r19
        st      X+, r20
        st      X+, r22
        st      X+, r23
        st      X+, r24
        st      X+, r25
        ret
        .size   speck_encrypt_asm, .-speck_encrypt_asm

        .section .text.speck_decrypt_asm,"ax",@progbits
        .global speck_decrypt_asm
        .type   speck_decrypt_asm, @function
speck_decrypt_asm:
        push    r24
        push    r25
        movw    r30, r22            ; Z = rk
        adiw    r30, 63             ; Z = rk + 108 (one past the last round key)
        adiw    r30, 45
        movw    r26, r24            ; X = blk
        ld      r18, X+
        ld      r19, X+
        ld      r20, X+
        ld      r21, X+
        ld      r22, X+
        ld      r23, X+
        ld      r24, X+
        ld      r25, X+
        ldi     r26, 6
.Ldec_loop:
        DEC_ROUND r18, r19, r20, r21
        DEC_ROUND r21, r18, r19, r20
        DEC_ROUND r20, r21, r18, r19
        DEC_ROUND r19, r20, r21, r18
        dec     r26
        breq    .Ldec_tail
        rjmp    .Ldec_loop
.Ldec_tail:
        DEC_ROUND r18, r19, r20, r21
        DEC_ROUND r21, r18, r19, r20
        DEC_ROUND r20, r21, r18, r19
        ; after 27 rounds the x bytes (low..high) are in r19, r20, r21, r18
        pop     r27
        pop     r26
        st      X+, r19
        st      X+, r20
        st      X+, r21
        st      X+, r18
        st      X+, r22
        st      X+, r23
        st      X+, r24
        st      X+, r25
        ret
        .size   speck_decrypt_asm, .-speck_decrypt_asm
