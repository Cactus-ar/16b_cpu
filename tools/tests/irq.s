; irq.s — interrupciones completas: vector en 0x0004, ISR con pila, RETI
; Correr con: emu16 irq.bin --irq-at 500
; Esperado: R5=1 (una IRQ atendida), R2=300 (el bucle de fondo termino igual)
; Nota: la ISR no salva banderas porque en este ISA ninguna instruccion
; consume Z/C de una instruccion anterior (BEQ/BNE recomputan) — no hace falta.

        JMP  inicio
        .org IRQ_VECTOR
        JMP  isr

inicio: LI   R6, STACK_TOP
        ADDI R6, R6, 1      ; pila lista ANTES de EI: la ISR la usa
        LI   R1, USER_DATA
        SW   R0, 0(R1)      ; contador de IRQs atendidas = 0
        CLR  R2
        EI

bucle:  ADDI R2, R2, 1      ; trabajo de fondo
        LI   R3, 300
        SLT  R4, R2, R3
        BNE  R4, R0, bucle  ; mientras R2 < 300

        DI
        LW   R5, 0(R1)      ; R5 = IRQs atendidas
        HALT

; --- rutina de atencion: el hardware ya hizo R7<-PC, PC<-0x0004, IE<-0
isr:    PUSH R3
        PUSH R4
        LI   R3, USER_DATA
        LW   R4, 0(R3)
        ADDI R4, R4, 1
        SW   R4, 0(R3)      ; contador++
        POP  R4
        POP  R3
        RETI                ; PC <- R7, IE <- 1, atomico
