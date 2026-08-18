; suma.s — suma 1..10, resultado esperado: R1 = 55 (0x37)
; Ejercita: ADDI, ADD, BNE con salto hacia atras, HALT.

        CLR  R1             ; acumulador
        CLR  R2             ; contador
        ADDI R3, R0, 10     ; limite

bucle:  ADDI R2, R2, 1      ; contador++
        ADD  R1, R1, R2     ; acumulador += contador
        BNE  R2, R3, bucle  ; hasta contador == 10

        HALT
