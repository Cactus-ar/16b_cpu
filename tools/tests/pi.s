; pi.s — decimales de pi por el spigot de Rabinowitz-Wagon, en 16 bits.
;
; La maquina no tiene MUL, ni DIV, ni forma de leer la bandera C (ninguna
; instruccion la consume — los saltos son solo sobre Z). Este programa
; implementa todo eso por software:
;   - mul16: multiplicacion shift-add con deteccion de overflow
;   - div16: division restoring de 16 pasos (cociente y resto)
;   - overflow: (a+b) desbordo  <=>  (a+b) <u a, con el idiom de
;     comparacion sin signo  x <u y  <=>  SLT(x^0x8000, y^0x8000)
;
; Con LEN=1750 produce 525 decimales correctos y termina (FIN).
; Con LEN=1800 la aritmetica de 16 bits desborda en la pasada 5 y el
; programa lo detecta e imprime OVERFLOW. La frontera se midio con la
; referencia pi_ref.c (misma semantica uint16, mismos chequeos).
;
; Correr:  asm16 tests/pi.s  &&  emu16 tests/pi.bin --steps 2000000000

.equ LEN,  1750             ; tamano del arreglo de residuos (subir a 1800 -> OVERFLOW)
.equ MAXD, 525              ; limite de validez: 3*LEN/10 digitos

; variables en RAM (0x0000..0x001F: alcanzables con imm6 sobre R0)
.equ V_N,     0             ; div16: dividendo
.equ V_D,     1             ; div16: divisor
.equ V_Q,     2             ; div16: cociente
.equ V_R,     3             ; div16: resto
.equ V_CNT,   4             ; div16: contador de pasos
.equ V_A,     5             ; mul16: factor A
.equ V_B,     6             ; mul16: factor B
.equ V_P,     7             ; mul16: producto
.equ V_OV,    8             ; bandera de overflow (software)
.equ V_CARRY, 9             ; acarreo del spigot
.equ V_I,     10            ; indice de la pasada
.equ V_PRED,  11            ; predigito retenido
.equ V_NINES, 12            ; nueves retenidos
.equ V_FIRST, 13            ; 1 hasta emitir el primer digito
.equ V_PRT,   14            ; digitos impresos

.equ ARR, USER_DATA         ; arreglo de residuos, LEN palabras
; registros: R6 = base del arreglo (constante), R7 = enlace, R1-R5 scratch

        JMP  inicio

; ---------------------------------------------------------------- mul16
; V_P = V_A * V_B ; marca V_OV si el producto no entra en 16 bits
mul16:  CLR  R3             ; P = 0
        LW   R1, V_A(R0)
        LW   R2, V_B(R0)
m_loop: BEQ  R2, R0, m_fin
        SHR  R4, R2         ; B >> 1
        SW   R4, V_B(R0)
        SHL  R5, R4
        SUB  R5, R2, R5     ; R5 = bit 0 de B
        BEQ  R5, R0, m_shift
        ADD  R2, R3, R1     ; suma = P + A   (B ya esta a salvo en RAM)
        LUI  R4, 0x80
        XOR  R5, R2, R4
        XOR  R4, R3, R4
        SLT  R4, R5, R4     ; 1 si suma <u P  ->  la suma desbordo
        BEQ  R4, R0, m_ok
        SW   R4, V_OV(R0)
m_ok:   MOV  R3, R2         ; P = suma
m_shift:
        LW   R2, V_B(R0)
        BEQ  R2, R0, m_fin  ; no quedan bits: no hace falta shiftear A
        SLT  R4, R1, R0     ; bit 15 de A (negativo con signo)
        BEQ  R4, R0, m_sh2
        SW   R4, V_OV(R0)   ; el shift perderia un bit que aun se usa
m_sh2:  SHL  R1, R1
        JMP  m_loop
m_fin:  SW   R3, V_P(R0)
        JR   R7

; ---------------------------------------------------------------- div16
; V_Q = V_N / V_D ; V_R = V_N % V_D  (sin signo, restoring, 16 pasos)
div16:  CLR  R3             ; REM
        CLR  R4             ; Q
        LW   R1, V_N(R0)
        LW   R2, V_D(R0)
        ADDI R5, R0, 16
        SW   R5, V_CNT(R0)
d_loop: SLT  R5, R1, R0     ; bit 15 de N
        SHL  R1, R1
        SW   R1, V_N(R0)
        SHL  R3, R3
        ADD  R3, R3, R5     ; REM = REM<<1 | bit
        SHL  R4, R4
        LUI  R1, 0x80
        XOR  R5, R3, R1
        XOR  R1, R2, R1
        SLT  R5, R5, R1     ; 1 si REM <u D
        BNE  R5, R0, d_next
        SUB  R3, R3, R2     ; REM -= D
        ADDI R4, R4, 1      ; Q |= 1
d_next: LW   R1, V_N(R0)
        LW   R5, V_CNT(R0)
        ADDI R5, R5, -1
        SW   R5, V_CNT(R0)
        BNE  R5, R0, d_loop
        SW   R4, V_Q(R0)
        SW   R3, V_R(R0)
        JR   R7

; ----------------------------------------------------------------- emit
; imprime el digito de R3 (0..9); tras el primero, imprime el punto
emit:   LI   R4, '0'
        ADD  R4, R3, R4
        OUT  R4, 0
        LW   R4, V_PRT(R0)
        ADDI R4, R4, 1
        SW   R4, V_PRT(R0)
        ADDI R5, R0, 1
        BNE  R4, R5, e_fin
        LI   R4, '.'
        OUT  R4, 0
e_fin:  JR   R7

; ---------------------------------------------------------------- inicio
inicio: LI   R6, ARR
        LI   R1, LEN        ; a[i] = 2 para todo i
        CLR  R2
        ADDI R3, R0, 2
ini_l:  ADD  R4, R6, R2
        SW   R3, 0(R4)
        ADDI R2, R2, 1
        BNE  R2, R1, ini_l
        SW   R0, V_OV(R0)
        SW   R0, V_NINES(R0)
        SW   R0, V_PRED(R0)
        SW   R0, V_PRT(R0)
        ADDI R1, R0, 1
        SW   R1, V_FIRST(R0)

; una pasada del arreglo = un digito
pass:   LW   R1, V_PRT(R0)
        LI   R2, MAXD
        SLT  R3, R1, R2
        BNE  R3, R0, p_go
        JMP  fin
p_go:   SW   R0, V_CARRY(R0)
        LI   R1, LEN-1
        SW   R1, V_I(R0)

loop_i: LW   R1, V_I(R0)    ; x10 = a[i] * 10
        ADD  R2, R6, R1
        LW   R3, 0(R2)
        SW   R3, V_A(R0)
        ADDI R3, R0, 10
        SW   R3, V_B(R0)
        LI   R5, mul16
        JALR R7, R5
        LW   R1, V_P(R0)    ; x = x10 + carry, con chequeo
        LW   R2, V_CARRY(R0)
        ADD  R3, R1, R2
        LUI  R4, 0x80
        XOR  R5, R3, R4
        XOR  R4, R1, R4
        SLT  R4, R5, R4
        BEQ  R4, R0, x_ok
        SW   R4, V_OV(R0)
x_ok:   SW   R3, V_N(R0)
        LW   R1, V_I(R0)    ; D = 2i+1
        SHL  R2, R1
        ADDI R2, R2, 1
        SW   R2, V_D(R0)
        LI   R5, div16
        JALR R7, R5
        LW   R1, V_I(R0)    ; a[i] = resto
        ADD  R2, R6, R1
        LW   R3, V_R(R0)
        SW   R3, 0(R2)
        LW   R3, V_Q(R0)    ; carry = cociente * i
        SW   R3, V_A(R0)
        SW   R1, V_B(R0)
        LI   R5, mul16
        JALR R7, R5
        LW   R3, V_P(R0)
        SW   R3, V_CARRY(R0)
        LW   R4, V_OV(R0)   ; algun paso desbordo?
        BEQ  R4, R0, no_ov
        JMP  overflow
no_ov:  LW   R1, V_I(R0)
        ADDI R1, R1, -1
        SW   R1, V_I(R0)
        BEQ  R1, R0, digit  ; procesamos i = LEN-1 .. 1
        JMP  loop_i

; digito: x = 10*a[0] + carry ; q = x/10 ; a[0] = x%10
digit:  LW   R3, 0(R6)
        SW   R3, V_A(R0)
        ADDI R3, R0, 10
        SW   R3, V_B(R0)
        LI   R5, mul16
        JALR R7, R5
        LW   R1, V_P(R0)
        LW   R2, V_CARRY(R0)
        ADD  R3, R1, R2
        LUI  R4, 0x80
        XOR  R5, R3, R4
        XOR  R4, R1, R4
        SLT  R4, R5, R4
        BEQ  R4, R0, x2_ok
        SW   R4, V_OV(R0)
x2_ok:  LW   R4, V_OV(R0)
        BEQ  R4, R0, d_go
        JMP  overflow
d_go:   SW   R3, V_N(R0)
        ADDI R1, R0, 10
        SW   R1, V_D(R0)
        LI   R5, div16
        JALR R7, R5
        LW   R1, V_R(R0)
        SW   R1, 0(R6)

; regla de predigitos: q=9 retiene; q=10 propaga acarreo; otro emite
        LW   R1, V_Q(R0)
        ADDI R2, R0, 9
        BNE  R1, R2, not9
        LW   R3, V_NINES(R0)
        ADDI R3, R3, 1
        SW   R3, V_NINES(R0)
        JMP  pass

not9:   ADDI R2, R0, 10
        BNE  R1, R2, not10
        LW   R3, V_PRED(R0) ; q=10: el predigito sube en 1...
        ADDI R3, R3, 1
        LI   R5, emit
        JALR R7, R5
z_l:    LW   R3, V_NINES(R0)  ; ...y los 9 retenidos se vuelven 0
        BEQ  R3, R0, z_fin
        ADDI R3, R3, -1
        SW   R3, V_NINES(R0)
        CLR  R3
        LI   R5, emit
        JALR R7, R5
        JMP  z_l
z_fin:  SW   R0, V_PRED(R0)
        JMP  pass

not10:  LW   R2, V_FIRST(R0)  ; caso comun: emitir el predigito anterior
        BNE  R2, R0, skipem
        LW   R3, V_PRED(R0)
        LI   R5, emit
        JALR R7, R5
skipem: SW   R0, V_FIRST(R0)
        SW   R1, V_PRED(R0)
n_l:    LW   R3, V_NINES(R0)  ; los 9 retenidos eran 9 de verdad
        BEQ  R3, R0, n_fin
        ADDI R3, R3, -1
        SW   R3, V_NINES(R0)
        ADDI R3, R0, 9
        LI   R5, emit
        JALR R7, R5
        JMP  n_l
n_fin:  JMP  pass

; ------------------------------------------------------------- terminales
fin:    ADDI R3, R0, 10
        OUT  R3, 0
        HALT

overflow:
        ADDI R3, R0, 10
        OUT  R3, 0
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
        ADDI R3, R0, 10
        OUT  R3, 0
        HALT
