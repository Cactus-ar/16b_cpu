; fib.s — sucesion de Fibonacci hasta que 16 bits no alcancen.
;
; Imprime F(1)..F(24) en decimal (uno por linea) y al calcular
; F(25)=75025 — que no entra en 16 bits — detecta el overflow por
; software y anuncia "OVERFLOW EN F" + indice. Igual que en pi.s, la
; deteccion es aritmetica: (a+b) desbordo  <=>  (a+b) <u a, porque
; la bandera C del hardware no es observable desde software.
;
; Novedades sobre pi.s:
;   - print16: conversion binario->decimal por divisiones por 10,
;     apilando restos con PUSH/POP (pila real de la ABI, R6) y un
;     CENTINELA (10, imposible como digito) en vez de contador.
;   - enlace anidado: print16 llama a div16, asi que salva R7 en pila.
;
; Esperado: F(24)=46368 es el ultimo impreso; luego "OVERFLOW EN F" y 25.

; variables de div16 (0..31: alcanzables con imm6 sobre R0)
.equ V_N,   0               ; dividendo
.equ V_D,   1               ; divisor
.equ V_Q,   2               ; cociente
.equ V_R,   3               ; resto
.equ V_CNT, 4               ; contador de pasos
.equ V_FA,  5               ; F(n-1)
.equ V_FB,  6               ; F(n)
.equ V_IDX, 7               ; n

        JMP  inicio

; ---------------------------------------------------------------- div16
; V_Q = V_N / V_D ; V_R = V_N % V_D  (sin signo, restoring, 16 pasos)
div16:  CLR  R3
        CLR  R4
        LW   R1, V_N(R0)
        LW   R2, V_D(R0)
        ADDI R5, R0, 16
        SW   R5, V_CNT(R0)
d_loop: SLT  R5, R1, R0
        SHL  R1, R1
        SW   R1, V_N(R0)
        SHL  R3, R3
        ADD  R3, R3, R5
        SHL  R4, R4
        LUI  R1, 0x80
        XOR  R5, R3, R1
        XOR  R1, R2, R1
        SLT  R5, R5, R1
        BNE  R5, R0, d_next
        SUB  R3, R3, R2
        ADDI R4, R4, 1
d_next: LW   R1, V_N(R0)
        LW   R5, V_CNT(R0)
        ADDI R5, R5, -1
        SW   R5, V_CNT(R0)
        BNE  R5, R0, d_loop
        SW   R4, V_Q(R0)
        SW   R3, V_R(R0)
        JR   R7

; -------------------------------------------------------------- print16
; imprime R1 en decimal + '\n'; llama a div16, asi que salva el enlace
print16:
        PUSH R7             ; enlace anidado: R7 se pisa al llamar a div16
        ADDI R2, R0, 10
        PUSH R2             ; centinela: 10 no es un digito posible
p_loop: SW   R1, V_N(R0)
        ADDI R2, R0, 10
        SW   R2, V_D(R0)
        LI   R5, div16
        JALR R7, R5
        LW   R1, V_R(R0)
        PUSH R1             ; el resto es el digito (llegan invertidos)
        LW   R1, V_Q(R0)
        BNE  R1, R0, p_loop
p_out:  POP  R1
        ADDI R2, R0, 10
        BEQ  R1, R2, p_fin  ; centinela: no quedan digitos
        LI   R3, '0'
        ADD  R1, R1, R3
        OUT  R1, 0
        JMP  p_out
p_fin:  ADDI R1, R0, 10     ; '\n'
        OUT  R1, 0
        POP  R7
        JR   R7

; ---------------------------------------------------------------- inicio
inicio: LI   R6, STACK_TOP
        ADDI R6, R6, 1      ; SP: primera libre hacia abajo

        SW   R0, V_FA(R0)   ; F(0) = 0
        ADDI R1, R0, 1
        SW   R1, V_FB(R0)   ; F(1) = 1
        SW   R1, V_IDX(R0)  ; n = 1

f_loop: LW   R1, V_FB(R0)   ; imprimir F(n)
        LI   R5, print16
        JALR R7, R5
        LW   R1, V_FA(R0)   ; t = F(n-1) + F(n), con chequeo
        LW   R2, V_FB(R0)
        ADD  R3, R1, R2
        LUI  R4, 0x80
        XOR  R5, R3, R4
        XOR  R4, R1, R4
        SLT  R4, R5, R4     ; t <u F(n-1)  ->  no entro en 16 bits
        BEQ  R4, R0, f_ok
        JMP  overflow
f_ok:   SW   R2, V_FA(R0)
        SW   R3, V_FB(R0)
        LW   R1, V_IDX(R0)
        ADDI R1, R1, 1
        SW   R1, V_IDX(R0)
        JMP  f_loop

overflow:
        LI   R3, 'O'
        OUT  R3, 0
        LI   R3, 'V'
        OUT  R3, 0
        LI   R3, 'E'
        OUT  R3, 0
        LI   R3, 'R'
        OUT  R3, 0
        LI   R3, 'F'
        OUT  R3, 0
        LI   R3, 'L'
        OUT  R3, 0
        LI   R3, 'O'
        OUT  R3, 0
        LI   R3, 'W'
        OUT  R3, 0
        LI   R3, ' '
        OUT  R3, 0
        LI   R3, 'E'
        OUT  R3, 0
        LI   R3, 'N'
        OUT  R3, 0
        LI   R3, ' '
        OUT  R3, 0
        LI   R3, 'F'
        OUT  R3, 0
        LW   R1, V_IDX(R0)  ; el que no entro: F(n+1)
        ADDI R1, R1, 1
        LI   R5, print16
        JALR R7, R5
        HALT
