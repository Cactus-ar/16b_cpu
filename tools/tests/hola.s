; hola.s — imprime "HOLA CPU16\n" por el puerto 0 (consola provisoria del emulador)
; Ejercita: LI, ADDI con literal de caracter, SW/LW indexados, OUT, BNE, MOV.
; Nota de diseno: el texto se arma en RAM porque el ISA no lee programa
; como dato (no hay LWP) — en la maquina real lo cargaria el monitor.

        LI   R2, USER_DATA  ; base del texto en RAM
        ADDI R4, R0, 11     ; longitud

        LI   R3, 'H'
        SW   R3, 0(R2)
        LI   R3, 'O'
        SW   R3, 1(R2)
        LI   R3, 'L'
        SW   R3, 2(R2)
        LI   R3, 'A'
        SW   R3, 3(R2)
        LI   R3, ' '
        SW   R3, 4(R2)
        LI   R3, 'C'
        SW   R3, 5(R2)
        LI   R3, 'P'
        SW   R3, 6(R2)
        LI   R3, 'U'
        SW   R3, 7(R2)
        LI   R3, '1'
        SW   R3, 8(R2)
        LI   R3, '6'
        SW   R3, 9(R2)
        ADDI R3, R0, 10     ; '\n'
        SW   R3, 10(R2)

        CLR  R5             ; indice
imprime:
        ADD  R1, R2, R5     ; direccion = base + indice
        LW   R3, 0(R1)
        OUT  R3, 0          ; consola
        ADDI R5, R5, 1
        BNE  R5, R4, imprime

        HALT
