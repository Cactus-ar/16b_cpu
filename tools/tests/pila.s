; pila.s — convencion de pila (03-memoria-spec §5) y subrutina con JALR/JR
; Esperado: R1=0x1111, R2=0x2222 (intercambiados via la pila),
;           R4=1 (SLT con signo: -1 < 2), R6=STACK_TOP+1 (pila balanceada)
; Ejercita: LI, PUSH/POP, JALR con enlace en R7, JR, SLT de signo mixto.

        LI   R6, STACK_TOP
        ADDI R6, R6, 1      ; SP: primera direccion libre hacia abajo

        LI   R1, 0x2222
        LI   R2, 0x1111

        PUSH R1
        PUSH R2
        POP  R1             ; R1 <- 0x1111
        POP  R2             ; R2 <- 0x2222

; subrutina "menor": entradas R3, R4; salida R4 = (R3 < R4) con signo
        LI   R5, menor
        ADDI R3, R0, -1
        ADDI R4, R0, 2
        JALR R7, R5         ; R7 <- direccion de retorno (convencion: enlace en R7)
        HALT

menor:  SLT  R4, R3, R4    ; -1 < 2 (con signo) -> 1
        JR   R7            ; volver
