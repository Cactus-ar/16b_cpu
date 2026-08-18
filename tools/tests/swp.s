; swp.s — carga de programa al banco alto y modelo de EEPROM ocupada
; Ejecuta desde el banco bajo (monitor) y escribe el banco alto con SWP,
; como lo hara el monitor real. El segundo SWP llega sin esperar los 10 ms
; y DEBE producir el aviso del emulador; el tercero espera y pasa limpio.
; Esperado: aviso solo en el segundo SWP; R3=0 al final.

        LI   R1, USER_BASE
        LI   R2, 0x1234

        SWP  R2, 0(R1)      ; primera escritura: banco alto, ok
        SWP  R2, 1(R1)      ; SIN esperar -> aviso "escritura corrupta"

; espera calibrada > 10 ms: ~11 ciclos por vuelta x 500 = 5500 > 4800
        LI   R3, 500
espera: ADDI R3, R3, -1
        BNE  R3, R0, espera

        SWP  R2, 2(R1)      ; con la EEPROM ya libre: sin aviso
        HALT
