; restore_context_arm64.asm

.code
PUBLIC RestoreContextAndJump_arm64

; RCX = CONTEXT*

RestoreContextAndJump_arm64 PROC
    mov x0, rcx

    ldr x19, [x0, #0xA0]     ; R19 = context->X19
    ldr x20, [x0, #0xA8]     ; R20 = context->X20
    ldr x21, [x0, #0xB0]     ; R21 = context->X21
    ldr x22, [x0, #0xB8]     ; R22 = context->X22
    ldr x23, [x0, #0xC0]     ; R23 = context->X23
    ldr x24, [x0, #0xC8]     ; R24 = context->X24
    ldr x25, [x0, #0xD0]     ; R25 = context->X25
    ldr x26, [x0, #0xD8]     ; R26 = context->X26
    ldr x27, [x0, #0xE0]     ; R27 = context->X27
    ldr x28, [x0, #0xE8]     ; R28 = context->X28

    ldr x29, [x0, #0xF0]     ; FP = context->Fp (x29)
    ldr x30, [x0, #0xF8]     ; LR = context->Lr (x30)

    ldr sp,  [x0, #0x70]     ; SP = context->Sp (offset 0x70)

    ; PC = context->Pc
    ldr x1, [x0, #0x80]      ; x1 = PC (offset 0x80)

    mov x0, #1               ; 

    br x1                    ; 

RestoreContextAndJump_arm64 ENDP

END
