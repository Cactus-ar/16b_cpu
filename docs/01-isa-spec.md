# CPU 16 bits — Especificación del ISA y microcódigo

**Versión 0.2 — normativo, salvo §6.3** · Ante conflicto con otros documentos, este tiene precedencia.

> **Nota sobre esta versión.** El documento original del ISA se perdió antes de entrar al repositorio. Esta versión se reconstruyó en agosto de 2026 a partir de: las correcciones registradas en `PENDIENTES.md` (que citan la versión buena), la tabla de instrucciones del `README.md`, y las señales de control definidas en `00-bus-spec.md`. Las decisiones de diseño derivadas de nuevo fueron **validadas por el autor el 16-ago-2026** (registro en §11). La única parte todavía no validada son las secuencias de microcódigo de §6.3, marcadas `[PROPUESTO — validar]`.

---

## 1. Modelo del programador

- **16 bits** de ancho de datos y de instrucción. Instrucción de ancho fijo: una palabra.
- **Direccionamiento por palabra.** No hay acceso a bytes; `PC+1` es la siguiente instrucción. Esto elimina toda la lógica de selección de carriles.
- **Load/store:** la memoria solo se toca con `LW` y `SW`; todo lo demás opera entre registros.
- **8 registros de propósito general**, R0–R7, de 16 bits.
  - **R0 está cableado a cero.** Escribirlo es legal y no tiene efecto. Habilita las pseudo-instrucciones (§8).
  - **R7 es el registro de enlace por convención:** `JALR` puede usar cualquier `rd`, pero la secuencia de interrupción escribe la dirección de retorno **siempre en R7** (§7). El código que use interrupciones no debe tener valores vivos en R7.
- **Dos banderas: Z (cero) y C (acarreo)**, en un registro de la tarjeta de la ALU, cargadas solo cuando el microcódigo activa `FLAGS_LD`. RISC-V no tiene banderas porque optimiza para pipeline; esta máquina no tiene pipeline y las banderas son el mecanismo de salto condicional más barato en chips.
- **E/S por espacio de puertos propio** (`IN`/`OUT`), separado de la memoria. 256 puertos posibles (imm8). El número de puerto viaja por el bus de direcciones vía MAR: cero señales nuevas en el backplane.

---

## 2. Formatos de instrucción

Cuatro formatos, deliberadamente pocos: cada formato adicional es lógica de decodificación que hay que dibujar a mano.

```
R:  op[15:12] · rd[11:9] · rs[8:6] · rt[5:3] · funct[2:0]
I:  op[15:12] · rd[11:9] · rs[8:6] · imm6[5:0]
L:  op[15:12] · rd[11:9] · —[8]    · imm8[7:0]
J:  op[15:12] · imm12[11:0]
```

**Por qué el formato L lleva imm8 y no imm9:** las constantes de 16 bits se componen con `LUI` (mitad alta) + `ORI` (mitad baja). Con imm8 las dos mitades son simétricas: `LUI` carga los bits [15:8] y `ORI` completa los [7:0], sin solapamiento ni bit sobrante. El bit 8 del formato queda sin uso y se codifica en cero.

---

## 3. Opcodes

| Opcode | Instrucción | Formato | Operación |
|---|---|---|---|
| `0000` | R-type | R | `rd ← rs funct rt` (ver §4) |
| `0001` | `ADDI rd, rs, imm` | I | `rd ← rs + sext(imm6)` |
| `0010` | `LW rd, imm(rs)` | I | `rd ← mem[rs + sext(imm6)]` |
| `0011` | `SW rd, imm(rs)` | I | `mem[rs + sext(imm6)] ← rd` |
| `0100` | `BEQ rd, rs, imm` | I | si `rd = rs`: `PC ← PC+1 + sext(imm6)` |
| `0101` | `BNE rd, rs, imm` | I | si `rd ≠ rs`: `PC ← PC+1 + sext(imm6)` |
| `0110` | `LUI rd, imm` | L | `rd ← imm8 << 8` |
| `0111` | `ORI rd, imm` | L | `rd ← rd \| imm8` |
| `1000` | `JALR rd, rs` | R | `rd ← PC+1; PC ← rs` |
| `1001` | `JMP imm` | J | `PC ← PC+1 + sext(imm12)` |
| `1010` | `IN rd, puerto` | L | `rd ← puerto[imm8]` |
| `1011` | `OUT rd, puerto` | L | `puerto[imm8] ← rd` |
| `1100` | `HALT` | — | Detiene el reloj vía `HALT_n` |
| `1101` | Sistema | — | `EI` / `DI` / `RETI` según subcampo (§7) |
| `1110` | — | — | **Libre** |
| `1111` | — | — | **Prefijo de expansión — reservado** (§9) |

Notas:

- En los saltos relativos (`BEQ`, `BNE`, `JMP`), la base es **PC+1** — el PC ya incrementado tras el fetch. El desplazamiento se cuenta desde la instrucción *siguiente*. (Validado por el autor, ago 2026.)
- `OUT` usa el campo `rd` del formato L como **fuente**; el nombre del campo indica posición, no dirección de datos.
- `HALT` y las instrucciones de sistema codifican en cero todos los bits no usados. (Validado.)

---

## 4. R-type: campo `funct`

Los 3 bits de `funct` van **directo** al selector de la ALU, sin traducción intermedia. Eso elimina un decodificador entero. El orden de la tabla sigue el listado del README; la asignación numérica exacta quedó validada por el autor (ago 2026):

| funct | Op | Operación | Z | C |
|---|---|---|---|---|
| `000` | ADD | `rd ← rs + rt` | ✓ | acarreo de la suma |
| `001` | SUB | `rd ← rs − rt` | ✓ | *borrow* invertido: C=1 si `rs ≥ rt` (sin signo) |
| `010` | AND | `rd ← rs & rt` | ✓ | 0 |
| `011` | OR | `rd ← rs \| rt` | ✓ | 0 |
| `100` | XOR | `rd ← rs ^ rt` | ✓ | 0 |
| `101` | SHL | `rd ← rs << 1` | ✓ | bit 15 expulsado |
| `110` | SHR | `rd ← rs >> 1` (lógico) | ✓ | bit 0 expulsado |
| `111` | SLT | `rd ← (rs < rt) ? 1 : 0` — **con signo** | ✓ | 0 |

Semántica de banderas y detalles (validados por el autor, ago 2026):

- **SLT compara con signo.** Decisión del autor: como los únicos saltos condicionales son `BEQ`/`BNE`, SLT es el primitivo de comparación de la máquina, y la aritmética de programas normales es con signo. Costo en hardware: el resultado es `N ⊕ V` sobre la resta interna — el signo del resultado corregido por overflow. La detección de overflow para la resta es `V = (s_rs ⊕ s_rt) ∧ (s_rs ⊕ s_resultado)`: dos XOR y un AND sobre bits de signo, que caben en el 74HC86 ya inventariado en la tarjeta de la ALU.
- **Comparación sin signo por software**, con el idiom de invertir el bit de signo: `sin_signo(a < b) = SLT(a ^ 0x8000, b ^ 0x8000)`. El ensamblador puede ofrecerlo como pseudo-secuencia.
- **Los corrimientos son de un bit por instrucción.** Corrimientos múltiples se hacen en bucle, o en el futuro con el prefijo de expansión (§9). El campo `rt` se ignora en SHL/SHR y se codifica en cero.
- El `funct` ocupa `ALU_OP[2:0]` del backplane. **`ALU_OP3` queda reservado** (posibles usos futuros: acarreo de entrada para suma extendida, operaciones adicionales). Las instrucciones no R-type que usan la ALU (`ADDI`, `ORI`, cálculo de direcciones, saltos) reciben su operación desde la ROM de control por las mismas líneas.

---

## 5. Generador de inmediatos: `IMM_SEL`

El generador de inmediatos vive en la **tarjeta de control** (junto con el IR); `IMM_SEL0/1` son señales internas de esa tarjeta, no del backplane. Esta tabla es la **única fuente de verdad** — `00-bus-spec.md` remite acá.

| SEL1 | SEL0 | Campo | Extensión | Lo usan |
|---|---|---|---|---|
| 0 | 0 | imm6 [5:0] | Signo | `ADDI`, `LW`, `SW`, `BEQ`, `BNE` |
| 0 | 1 | imm8 [7:0] | **← desplazado 8 posiciones** (bits 15–8, resto en cero) | `LUI` |
| 1 | 0 | imm12 [11:0] | Signo | `JMP` |
| 1 | 1 | imm8 [7:0] | Ceros | `ORI`, `IN`, `OUT` |

**Por qué está en compuertas y no en una EEPROM:** una tabla lo haría con menos chips, pero ocultaría los cuatro modos de extensión, que en lógica discreta son visibles y verificables. Es exactamente la zona gris del proyecto, y se resolvió a favor de la claridad. Revisar solo con motivo.

---

## 6. Microcódigo

### 6.1 Convenciones

- Las señales son las de `00-bus-spec.md`. Las cargas (`*_LD`, `REG_WE`) ocurren en el flanco de subida de CLK; las habilitaciones de salida (`*_OUT_n`) son por nivel.
- **En cada microciclo hay exactamente un emisor sobre el bus de datos.**
- `RSA`, `RSB` y `RSW` (selección de registros) las emite la tarjeta de control derivándolas de los campos del IR; qué campo va a qué puerto depende de la instrucción y se anota en cada secuencia.
- El contador de microciclo es interno de la tarjeta de control. **T0 es el fetch**, común a todas las instrucciones; el microcódigo de cada instrucción empieza en T1. La secuencia más larga (salto tomado) llega a T6: **8 estados, contador de 3 bits, con margen.**
- Una salida de la ROM de control marca **fin de instrucción** y reinicia el contador a T0. Es lo que permite que cada instrucción dure distinto. (Ambos puntos validados por el autor, ago 2026.)

### 6.2 Fetch — T0, común a todas

```
T0:  PC_AOUT_n + ROM_OUT_n + IR_LD + PC_INC
     ; el PC maneja el bus de direcciones, la ROM de programa emite
     ; la instrucción, el IR la captura y el PC incrementa en el
     ; mismo flanco (el IR captura la instrucción del PC viejo).
```

En T0 la tarjeta de control también evalúa la condición de interrupción (§7): si corresponde atender, en lugar del fetch se ejecuta la secuencia de atención.

### 6.3 Secuencias por instrucción `[PROPUESTO — validar]`

Las secuencias completas se derivaron de nuevo para esta reconstrucción. La de R-type coincide con la registrada en `00-bus-spec.md` §3b (renumerada T1–T3, ítem 10 de PENDIENTES); las demás son derivación nueva.

**R-type** — `RSA=rs, RSB=rt, RSW=rd`

```
T1:  RA_OUT_n + TMPA_LD                  ; TMPA ← rs
T2:  RB_OUT_n + TMPB_LD                  ; TMPB ← rt
T3:  ALU_OUT_n + REG_WE + FLAGS_LD       ; rd ← TMPA funct TMPB, banderas
```

**ADDI** — `RSA=rs, RSW=rd`, ALU=ADD, IMM_SEL=00

```
T1:  RA_OUT_n  + TMPA_LD                 ; TMPA ← rs
T2:  IMM_OUT_n + TMPB_LD                 ; TMPB ← sext(imm6)
T3:  ALU_OUT_n + REG_WE + FLAGS_LD       ; rd ← rs + imm, banderas
```

**LW** — `RSA=rs, RSW=rd`, ALU=ADD, IMM_SEL=00

```
T1:  RA_OUT_n  + TMPA_LD                 ; TMPA ← rs
T2:  IMM_OUT_n + TMPB_LD                 ; TMPB ← sext(imm6)
T3:  ALU_OUT_n + MAR_LD                  ; MAR ← rs + imm
T4:  RAM_OUT_n + REG_WE                  ; rd ← mem[MAR]
     ; PC_AOUT_n inactivo: el MAR maneja el bus de direcciones
```

**SW** — `RSA=rs, RSB=rd`, ALU=ADD, IMM_SEL=00

```
T1:  RA_OUT_n  + TMPA_LD                 ; TMPA ← rs
T2:  IMM_OUT_n + TMPB_LD                 ; TMPB ← sext(imm6)
T3:  ALU_OUT_n + MAR_LD                  ; MAR ← rs + imm
T4:  RB_OUT_n  + RAM_WE_n                ; mem[MAR] ← rd (leído por puerto B)
```

**BEQ / BNE** — `RSA=rd, RSB=rs`, ALU=SUB, IMM_SEL=00

```
T1:  RA_OUT_n + TMPA_LD                  ; TMPA ← rd
T2:  RB_OUT_n + TMPB_LD                  ; TMPB ← rs
T3:  ALU_OUT_n + FLAGS_LD                ; banderas ← rd − rs
     ; --- la ROM de control lee Z en T4 ---
     ; BEQ no tomado (Z=0) / BNE no tomado (Z=1): fin de instrucción
     ; tomado:
T4:  PC_OUT_n  + TMPA_LD                 ; TMPA ← PC+1
T5:  IMM_OUT_n + TMPB_LD                 ; TMPB ← sext(imm6)
T6:  ALU_OUT_n + PC_LD                   ; PC ← PC+1 + imm   (ALU=ADD)
```

> **Efecto colateral documentado:** `BEQ`/`BNE` **modifican Z y C** (necesitan `FLAGS_LD` para que la ROM de control vea el resultado de la resta en el ciclo siguiente, porque las banderas del backplane son la salida registrada de la tarjeta de la ALU). No escribir código que dependa de banderas a través de un salto.

**LUI** — `RSW=rd`, IMM_SEL=01. Una sola microinstrucción: el inmediato ya sale desplazado del generador, sin pasar por la ALU. Por eso **LUI no toca banderas**.

```
T1:  IMM_OUT_n + REG_WE                  ; rd ← imm8 << 8
```

**ORI** — `RSA=rd, RSW=rd`, ALU=OR, IMM_SEL=11

```
T1:  RA_OUT_n  + TMPA_LD                 ; TMPA ← rd
T2:  IMM_OUT_n + TMPB_LD                 ; TMPB ← imm8 (ceros)
T3:  ALU_OUT_n + REG_WE + FLAGS_LD       ; rd ← rd | imm8, banderas
```

**JALR** — `RSA=rs, RSW=rd`

```
T1:  PC_OUT_n + REG_WE                   ; rd ← PC+1 (dirección de retorno)
T2:  RA_OUT_n + PC_LD                    ; PC ← rs
```

**JMP** — IMM_SEL=10, ALU=ADD

```
T1:  PC_OUT_n  + TMPA_LD                 ; TMPA ← PC+1
T2:  IMM_OUT_n + TMPB_LD                 ; TMPB ← sext(imm12)
T3:  ALU_OUT_n + PC_LD                   ; PC ← PC+1 + imm
```

**IN** — `RSW=rd`, IMM_SEL=11

```
T1:  IMM_OUT_n + MAR_LD                  ; MAR ← número de puerto
T2:  IO_OUT_n  + REG_WE                  ; rd ← puerto[MAR]
```

**OUT** — `RSA=rd` (fuente), IMM_SEL=11

```
T1:  IMM_OUT_n + MAR_LD                  ; MAR ← número de puerto
T2:  RA_OUT_n  + IO_LD                   ; puerto[MAR] ← rd
```

**HALT**

```
T1:  HALT                                ; la tarjeta de control tira HALT_n a masa
```

La tarjeta de reloj detiene CLK en bajo, sincrónicamente (ver `modulos/01-reloj.md`). El PC queda apuntando a la instrucción siguiente. La máquina se reanuda por pulso manual o reset. (Validado.)

---

## 7. Interrupciones

Decidido para la v1: el costo real es un flip-flop y dos bits más en la ROM; postergarlas obligaría a reprogramar cuatro EEPROM.

### Hardware

- **`IRQ_n`** (J2-A8): línea de solicitud, activa en bajo, nivel. Colector abierto con pull-up si llega a haber más de una fuente. (Validado.)
- **`IE`**: flip-flop de habilitación en la tarjeta de control. **Arranca en 0 tras reset** — el código debe ejecutar `EI` explícitamente cuando está listo para atender.
- La dirección del vector la emite el generador de inmediatos como constante. **El valor del vector queda pendiente de `03-memoria-spec.md`.**

### Secuencia de atención

En T0, la tarjeta de control evalúa: si `IRQ_n = 0` **y** `IE = 1`, en lugar del fetch ejecuta:

```
Ta:  PC_OUT_n  + REG_WE (RSW=R7 forzado)  ; R7 ← PC (dirección de retorno)
Tb:  IMM_OUT_n(vector) + PC_LD            ; PC ← vector,  IE ← 0
```

Mismo camino de datos que `JALR`: no se agrega hardware, solo microcódigo. `IE ← 0` evita reentradas; el periférico debe mantener `IRQ_n` hasta ser atendido y soltarlo cuando su rutina lo limpie.

### Instrucciones de sistema — opcode `1101`

El subcampo va en los bits [2:0] (la posición del `funct`); los bits [11:3] en cero. Codificación validada por el autor (ago 2026):

| Bits [2:0] | Instrucción | Operación | Microcódigo |
|---|---|---|---|
| `000` | `EI` | `IE ← 1` | `T1: IE←1` |
| `001` | `DI` | `IE ← 0` | `T1: IE←0` |
| `010` | `RETI` | `PC ← R7; IE ← 1` | `T1: RA_OUT_n (RSA=R7 forzado) + PC_LD, IE←1` |
| `011`–`111` | — | Reservado | |

`RETI` existe como instrucción propia (y no como `JALR R0, R7` + `EI`) porque el retorno y la rehabilitación deben ser **atómicos**: con dos instrucciones habría una ventana en la que una segunda interrupción pisaría R7 antes de completar el retorno.

---

## 8. Pseudo-instrucciones

Las traduce el ensamblador; el hardware no las conoce.

| Pseudo | Se traduce a | Nota |
|---|---|---|
| `NOP` | `ADDI R0, R0, 0` | R0 absorbe la escritura |
| `MOV rd, rs` | `ADDI rd, rs, 0` | |
| `CLR rd` | `ADDI rd, R0, 0` | Validado |
| `LI rd, const16` | `LUI rd, hi8` + `ORI rd, lo8` | La razón del rediseño del formato L |
| `JR rs` | `JALR R0, rs` | Salto sin enlace. Validado |

---

## 9. Prefijo de expansión — opcode `1111`

**Reservado desde ya, sin implementar.** Un prefijo es una instrucción que modifica el significado de la siguiente: da espacio de opcodes ilimitado a futuro sin costo presente. Candidatos que podrían necesitarlo: multiplicación por hardware, corrimientos múltiples, acceso a bytes para el buffer gráfico.

Hasta que se implemente, **ejecutar `1111` es comportamiento indefinido**. El ensamblador debe rechazarlo. Reservarlo ahora significa que ningún programa legal lo usa, y que implementarlo después no rompe nada.

---

## 10. Apéndice — notas de implementación de la tarjeta de control

*Este apéndice es temporal: su lugar definitivo es `modulos/09-control.md` cuando ese documento se escriba. Se registra acá para no perderlo de nuevo.*

- **ROM de control: 14 bits de dirección** (con `IRQ_n` e `IE` incluidos en la dirección), lo que descarta el 28C64 y lleva a **4 × AT28C256** (32K × 8, 15 bits — un bit de dirección de margen). Coincide con la estandarización de toda la memoria en la huella JEDEC de 28 pines (`componentes.md`).
- Desglose exacto de los 14 bits de dirección (opcode, contador de microciclo, Z, C, IRQ, IE) — borrador: op(4) + T(3) + funct(3) + Z + C + IRQ + IE = 14. A fijar en `modulos/09-control.md`.
- Las cuatro EEPROM en paralelo dan hasta 32 señales de control de salida.
- `IMM_SEL0/1` y el contador de microciclo son **internos** de la tarjeta de control; no viajan por el backplane (B22/B23 quedan reservados).
- El registro de microinstrucción debe resetear a un valor con **todas las habilitaciones de bus inactivas** (son activas en bajo → reset a unos, no a ceros).

---

## 11. Registro de validación

**Validado por el autor el 16-ago-2026:**

1. Base de los saltos relativos: PC+1 (§3).
2. Bits no usados codificados en cero (§3, §4).
3. Asignación numérica del campo `funct` (§4).
4. Semántica de C por operación; corrimientos de a un bit (§4).
5. **SLT con signo** — decisión explícita del autor, contra la alternativa sin signo. Implica la lógica `N ⊕ V` en la tarjeta de la ALU (§4).
6. Contador de microciclo de 3 bits y señal de fin de instrucción (§6.1).
7. BEQ/BNE modifican banderas — efecto colateral aceptado a cambio de no llevar Z combinacional a la ROM de control (§6.3).
8. Semántica de reanudación tras HALT (§6.3).
9. `IRQ_n` por nivel / colector abierto (§7).
10. Codificación del subcampo de sistema: 000=EI, 001=DI, 010=RETI (§7).
11. Pseudo-instrucciones CLR y JR (§8).
12. Desglose de los 14 bits de dirección de la ROM de control: borrador aceptado, se fija al escribir `modulos/09-control.md` (§10).

**Pendiente de validación:**

- Las secuencias de microcódigo de §6.3, salvo la R-type (respaldada por `00-bus-spec.md` §3b). Requieren lectura ciclo por ciclo del autor: verificar operandos, orden y señales de cada instrucción contra la intención original del diseño.
