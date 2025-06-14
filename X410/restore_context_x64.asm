; restore_context_x64.asm

.code
PUBLIC RestoreContextAndJump_x64

RestoreContextAndJump_x64 PROC
    ; RCX = CONTEXT*
    mov rax, rcx                 ; 

    mov rbx, qword ptr [rax + 18h]  ; context.Rbx (offset 0x18)
    mov rsi, qword ptr [rax + 28h]  ; context.Rsi (offset 0x28)
    mov rdi, qword ptr [rax + 30h]  ; context.Rdi (offset 0x30)
    mov r12, qword ptr [rax + 38h]  ; context.R12 (offset 0x38)
    mov r13, qword ptr [rax + 40h]  ; context.R13 (offset 0x40)
    mov r14, qword ptr [rax + 48h]  ; context.R14 (offset 0x48)
    mov r15, qword ptr [rax + 50h]  ; context.R15 (offset 0x50)

    mov rcx, qword ptr [rax + 08h]  ; context.Rip (offset 0x08)
    mov rdx, qword ptr [rax + 20h]  ; context.Rbp (offset 0x20)
    mov rsp, qword ptr [rax + 10h]  ; context.Rsp (offset 0x10)
    mov rbp, rdx

    mov rax, 1               ; 

    jmp rcx                  ; 

RestoreContextAndJump_x64 ENDP

END
