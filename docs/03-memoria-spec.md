# CPU 16 bits — Mapa de memoria

**Versión 0.1** · Documento normativo. Decisiones validadas por el autor (18-ago-2026). Ante conflicto, `01-isa-spec.md` tiene precedencia.

---

## 1. Los tres espacios

La máquina tiene **tres espacios de direcciones independientes**. No es una decisión de este documento sino una consecuencia de la arquitectura de señales, y conviene entenderla así:

| Espacio | Tamaño | Quién responde | Señales |
|---|---|---|---|
| **Programa** | 64K palabras | EEPROM (tarjeta 8) | `ROM_OUT_n`, `PROG_WE_n` |
| **Datos** | 64K palabras | SRAM (tarjeta 7) | `RAM_OUT_n`, `RAM_WE_n` |
| **Puertos** | 256 | E/S (tarjeta 10) | `IO_OUT_n`, `IO_LD` |

La separación es **por señal de control, no por decodificación de direcciones**: la ROM emite cuando el microcódigo la habilita, la RAM ídem. El mismo valor de dirección significa cosas distintas según qué señal esté activa. Consecuencias:

- La v1 necesita **cero lógica de decodificación**. No hay comparadores de rango en ninguna tarjeta.
- Cada espacio es completo: 64K palabras de programa **más** 64K de datos, el doble que un mapa unificado.
- El espacio de puertos reutiliza el bus de direcciones vía MAR (`01-isa-spec.md` §1): cero señales nuevas.

Direccionamiento **por palabra** (16 bits); no existen los bytes en ninguno de los tres espacios.

---

## 2. Población v1 y previsión de expansión

**Espacio de programa: los dos bancos van poblados desde la v1** — 4 chips en dos pares, decodificados por A15 (media 74HC139 en la tarjeta 8):

| Banco | A15 | Chips | Contenido | Quién lo escribe |
|---|---|---|---|---|
| Bajo | 0 | 2 × 28C256 | Monitor/BIOS | Solo el programador (T48) |
| Alto | 1 | 2 × 28C256 | Programa de usuario | `SWP` desde el monitor |

No es margen: es **requisito funcional de `SWP`** (§6) — el banco que se ejecuta y el banco que se escribe deben ser chips distintos.

**Espacio de datos: se puebla la mitad baja** (32K palabras, 2 chips). La tarjeta 7 lleva el zócalo del segundo banco sin poblar y su media 74HC139 de A15; pero la mitad alta tiene un destino más interesante que "más RAM": periféricos mapeados a memoria (§4).

---

## 3. Mapa del espacio de programa

```
0x0000 ─ RESET: el PC arranca acá → entrada del monitor          ┐
0x0004 ─ Vector de interrupción                                  │ BANCO BAJO
0x0008 ─ Monitor/BIOS — 4K palabras (0x0008–0x0FFF)              │ (A15=0)
0x1000 ─ Reservado BIOS/tablas — resto del banco (hasta 0x7FFF)  ┘ solo T48
0x8000 ─ USER_BASE: programa de usuario                          ┐ BANCO ALTO
        │ 32K palabras (0x8000–0xFFFF)                           │ (A15=1)
0xFFFF ─┘                                                        ┘ SWP
```

| Símbolo | Dirección | Definición |
|---|---|---|
| `RESET_ENTRY` | `0x0000` | El PC resetea a cero (hardware de la tarjeta 3: `RESET_n` limpia el contador) |
| `IRQ_VECTOR` | `0x0004` | Constante emitida por el generador de inmediatos en la secuencia de atención (`01-isa-spec.md` §7) |
| `MONITOR_BASE` | `0x0008` | |
| `USER_BASE` | `0x8000` | Donde el monitor carga programas — **primer word del banco alto** |

**Por qué el usuario vive en el banco alto y no a continuación del monitor:** ver §6 — mientras una EEPROM ejecuta su ciclo interno de escritura, *todo el chip* deja de devolver datos. Si monitor y usuario compartieran chips, el primer `SWP` haría que el propio monitor se leyera como basura. Bancos separados = el monitor sigue ejecutando mientras el banco de usuario se graba. *(Corregido el 18-ago-2026; el borrador original ponía `USER_BASE` en `0x1000`.)*

**Por qué el vector va en dirección baja:** la emite el generador de inmediatos como constante cableada; una dirección con casi todos los bits en cero es la constante más barata de generar y de verificar con el osciloscopio. En `0x0000` y `0x0004` normalmente viven sendos `JMP` hacia el código real del monitor.

**Por qué 4K de monitor:** un monitor serio (inspección de memoria, carga por teclado, guardado, E/S de LCD) cabe en mucho menos; 4K es margen deliberado, y el resto del banco bajo (28K) queda para la BIOS futura y tablas — todo grabado con el T48.

---

## 4. Mapa del espacio de datos

```
0x0000 ─┐ Variables del monitor — 256 palabras (0x0000–0x00FF)
0x0100 ─┤
        │ Datos de usuario (crecen hacia arriba)
        │ ...
        │ Pila (crece hacia abajo desde 0x7FFF; puntero: R6)
0x8000 ─┤
        │ Sin poblar — reservado para periféricos mapeados a
        │ memoria: VRAM del LCD gráfico futuro, ventana de flash
0xFFFF ─┘
```

| Símbolo | Dirección | Definición |
|---|---|---|
| `MON_VARS` | `0x0000`–`0x00FF` | Reservado para el monitor; el usuario no debe tocarlo si quiere volver al monitor limpio |
| `USER_DATA` | `0x0100` | |
| `STACK_TOP` | `0x7FFF` | Valor inicial de R6; la pila crece hacia abajo |

**La reserva de la mitad alta es la jugada de expansión más importante del mapa:** un LCD gráfico futuro con VRAM propia se mapea como memoria de datos en `0x8000+` — `SW` escribe pixels a velocidad de bus, que es exactamente el refresco fluido que el Pong exige y que los puertos de E/S no dan. La media 74HC139 de la tarjeta 7 ya rutea ese rango fuera de la SRAM.

---

## 5. Convenciones de software (ABI)

El hardware no conoce ninguna de estas; las impone el ensamblador y la disciplina.

| Registro | Rol | Nota |
|---|---|---|
| R0 | Cero cableado | Hardware |
| R1–R5 | Propósito general | |
| **R6** | **Puntero de pila** | Inicializa en `STACK_TOP`, crece hacia abajo. `PUSH rd` = `ADDI R6, R6, -1` + `SW rd, 0(R6)`; `POP` al revés |
| **R7** | **Enlace** | Ya normativo: `JALR` por convención y la interrupción por hardware (`01-isa-spec.md` §7) |

---

## 6. Carga de programas — `SWP`

Decisión del autor (18-ago-2026): el opcode **`1110`** se asigna a **`SWP rd, imm(rs)`** — *store word to program* — el espejo de `SW` sobre el espacio de programa, usando `PROG_WE_n`. Es la instrucción que hace real el objetivo del proyecto: el monitor puede **cargar y guardar programas** desde el teclado, sin sacar las EEPROM. La definición normativa y el microcódigo viven en `01-isa-spec.md` §3 y §6.3.

**La regla de los dos bancos — restricción normativa.** Durante el ciclo interno de escritura (~10 ms), la EEPROM entera deja de devolver datos: cualquier lectura entrega el estado de polling, no el contenido. En una Harvard que solo ejecuta desde ROM, eso significa que **el código que ejecuta `SWP` no puede vivir en el mismo chip que está escribiendo** — ni siquiera para esperar. De ahí los dos bancos de §2/§3: `SWP` solo es válido hacia el banco que **no** se está ejecutando (en la práctica: el monitor, banco bajo, escribe el banco alto). Un `SWP` al banco propio es comportamiento indefinido y el ensamblador/monitor deben impedirlo.

Consideraciones adicionales que el monitor debe respetar:

- **Ciclo de escritura interno: hasta 10 ms.** Tras cada `SWP`, esperar ≥10 ms (bucle de demora calibrado) antes del siguiente `SWP` o de saltar al banco recién escrito. A velocidad de tipeo humano es invisible.
- **SDP (Software Data Protection):** los AT28C256 pueden venir con protección activada. Política v1: **grabar las EEPROM de programa con SDP deshabilitado** desde el programador. Si más adelante se quiere SDP (protege de escrituras espurias durante caídas de tensión), el monitor puede emitir la secuencia de desbloqueo con la propia `SWP` — son escrituras a direcciones mágicas.
- **Limitación conocida: no existe lectura de programa como dato** (no hay `LWP`). El monitor no puede verificar lo que escribió releyéndolo; confía en la demora y en que la EEPROM completó el ciclo. Si alguna vez hace falta verificación real, `LWP` es candidato natural al prefijo de expansión `1111`. Registrado acá para no perderlo.

---

## 7. Espacio de puertos

El detalle pertenece a `02-io-spec.md` (pendiente de escribir). Este documento solo reserva:

| Rango | Uso |
|---|---|
| `0x00`–`0x0F` | Sistema: LCD, teclado hexadecimal |
| `0x10`–`0xFF` | Libre: joystick A/D, flash, expansión |

---

## 8. Resumen para el emulador y el ensamblador

- Tres espacios independientes: `prog[65536]`, `data[65536]`, `port[256]`, todos de palabras de 16 bits.
- Poblado real v1: `prog` completo (dos bancos; A15 separa monitor de usuario), `data[0x0000..0x7FFF]`. Acceso a datos fuera de lo poblado: lee basura (bus flotante), no es error de hardware — el emulador debería advertirlo.
- Modelo de EEPROM ocupada: tras `SWP`, el banco escrito queda inválido ~10 ms (4800 ciclos a 480 kHz); leer o ejecutar de ese banco en ese lapso es el bug que los dos bancos evitan — el emulador debería detectarlo y avisar.
- Reset: `PC = 0x0000`, `IE = 0`, registros indefinidos (salvo R0).
- Interrupción: `R7 ← PC`, `PC ← 0x0004`, `IE ← 0`.
- El ensamblador inicializa símbolos: `RESET_ENTRY`, `IRQ_VECTOR`, `MONITOR_BASE`, `USER_BASE`, `MON_VARS`, `USER_DATA`, `STACK_TOP`.
