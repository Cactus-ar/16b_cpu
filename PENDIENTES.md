# Pendientes — Corrección de discrepancias

Detectadas en la revisión cruzada de documentos, agosto 2026. Ordenadas por severidad.

Cuando un ítem se resuelva, marcarlo y anotar en qué documento quedó la versión buena.

> **Nota (16-ago-2026):** varios ítems de la lista original referían a documentos (`01-isa-spec.md`, `modulos/09-control.md`) que nunca entraron al repositorio y se perdieron. El ISA se **reconstruyó** en `docs/01-isa-spec.md` a partir de lo registrado acá, en el README y en el bus-spec; sus marcas `[PROPUESTO — validar]` esperan revisión del autor (ver ítem 19). Los ítems resueltos citan la versión buena reconstruida.

---

## Críticas — contradicciones directas entre documentos

### [x] 1. Tabla de `IMM_SEL` inconsistente

`00-bus-spec.md` traía la codificación vieja (imm9); la buena es la del ISA (imm8, de la composición de constantes con `LUI` + `ORI`).

**Resuelto (16-ago-2026):** la tabla vive únicamente en `01-isa-spec.md` §5; `00-bus-spec.md` remite allí. Un solo lugar de verdad.

---

### [x] 2. `IMM_SEL0` / `IMM_SEL1` figuraban como señales del backplane

Son internas de la tarjeta de control, porque el generador de inmediatos se mudó allí junto con el IR.

**Resuelto (16-ago-2026):** B22 y B23 marcados como reservados en `00-bus-spec.md`, con nota del motivo.

---

### [~] 3. Las interrupciones no estaban documentadas — PARCIAL

**Resuelto en lo documental (16-ago-2026):** `01-isa-spec.md` §7 define IE, la secuencia de atención (R7 ← PC, PC ← vector, IE ← 0), las instrucciones `EI`/`DI`/`RETI` bajo el opcode `1101`, y `IRQ_n` dejó de ser "Reservado" en `00-bus-spec.md`. La decisión de opcodes se tomó: **`1101` = sistema, `1111` = prefijo de expansión** (ver ítem 14).

**Queda pendiente:**
- Dirección del vector de interrupción → depende de `03-memoria-spec.md` (ítem 12).

*(Las marcas de §7 — subcampo de sistema, IRQ_n por nivel — se validaron el 16-ago-2026, ver ítem 19.)*

---

### [x] 4. Dimensionamiento de la ROM de control desactualizado

Con `IRQ_n` e `IE` en la dirección son **14 bits** y el 28C64 queda sin margen.

**Resuelto (16-ago-2026):** registrado en `01-isa-spec.md` §10: 14 bits de dirección, 4 × AT28C256 (32K × 8 — un bit de margen). El desglose exacto de los 14 bits queda `[PROPUESTO]` y se fijará al escribir `modulos/09-control.md` (ítem 18).

---

### [x] 5. Conflicto de números de parte entre módulo 09 y componentes

**Resuelto (16-ago-2026):** manda `componentes.md` — todo AT28C256, huella JEDEC única de 28 pines. `01-isa-spec.md` §10 usa esos números. El documento del módulo 09 no existe todavía (ítem 18); cuando se escriba, nace conforme.

---

## Importantes — documentos desactualizados

### [x] 6. README con información vieja del ISA

**Resuelto (16-ago-2026):** formato L = `op(4)·rd(3)·—(1)·imm8(8)`, `LUI rd ← imm8 << 8`, `HALT` en `1100`, `1101` sistema, `1110` libre, `1111` prefijo. Tabla del README alineada con `01-isa-spec.md` §3.

---

### [x] 7. README y bus-spec listaban diez módulos

**Resuelto (16-ago-2026):** nueve tarjetas más PS/2 opcional, hueco en el número 4 (IR fusionado con el 9) para no invalidar referencias, en `README.md` y `00-bus-spec.md` §5.

---

### [x] 8. Estructura de carpetas del README obsoleta

**Resuelto (16-ago-2026):** el README muestra la estructura real (`docs/` numerados, `docs/modulos/`, `kicad/comun/`). Además se renombraron los archivos al esquema numerado: `bus-spec-cpu16.md → 00-bus-spec.md`, `componentes-cpu16.md → componentes.md`, `modulo-01-reloj.md → modulos/01-reloj.md`, `kicad-modulo-01.md → modulos/01-reloj-kicad.md`, `kicad/Conector/ → kicad/comun/`.

---

## Menores

### [x] 9. Frecuencia máxima inconsistente

**Resuelto (16-ago-2026):** `00-bus-spec.md` dice 0,7 Hz – 480 kHz, los valores reales de los tres rangos de capacitor.

---

### [x] 10. Numeración de microciclos inconsistente

**Resuelto (16-ago-2026):** `00-bus-spec.md` §3b usa T1/T2/T3 (T0 = fetch), igual que `01-isa-spec.md` §6. Se eliminó además un párrafo duplicado de notas sobre `HALT_n` y microciclos que había quedado en las secciones 3 y 3b.

---

### [x] 11. Módulo 01: R7 y nomenclatura del pull-up

**Resuelto (16-ago-2026)** en `modulos/01-reloj.md`:
- R7 = trimpot multivuelta de 20 kΩ (era 12 kΩ fijo), con la razón documentada (tolerancias del 10 % podían llevar el umbral a 5,20 V → reset permanente). BOM actualizada.
- Paso 8 del procedimiento pasó de "verificar" a "ajustar y verificar".
- `RP` → `R10`, como en el esquemático.

---

## Huecos — falta escribir

### [ ] 12. `docs/03-memoria-spec.md` no existe

Ningún documento define cuánta memoria hay ni qué vive en cada rango. Falta decidir y escribir:

- Tamaño de RAM y de ROM de programa.
- Mapa de memoria: monitor, BIOS, programa de usuario, buffer de pantalla, pila, vector de interrupción.
- Rangos que decodifica cada tarjeta.
- Previsión de expansión: segundo banco, zócalos sin poblar.

**Criterio del autor:** el costo no es restricción, la variable a minimizar es tener que rehacer. Diseñar el decodificador para el rango completo de 64K palabras aunque se pueble menos.

**Sigue siendo el único ítem que bloquea el diseño de tarjetas (módulos 7 y 8), y ahora también fija el vector de interrupción (ítem 3).**

---

### [ ] 13. Registro de decisiones de diseño

`docs/decisiones.md`. El razonamiento detrás de cada decisión mayor está en `CLAUDE.md` en forma resumida, pero merece un documento propio con el desarrollo completo.

---

### [x] 14. Prefijo de expansión de opcodes

**Resuelto (16-ago-2026), decisión del autor:** `1111` queda reservado como prefijo de expansión desde ya (costo cero hoy, espacio ilimitado mañana); las instrucciones de sistema van en `1101` con subcampo; `1110` queda libre. Documentado en `01-isa-spec.md` §3 y §9. Ejecutar `1111` es comportamiento indefinido y el ensamblador debe rechazarlo.

---

### [ ] 17. `docs/02-io-spec.md` no existe *(nuevo, 16-ago-2026)*

`CLAUDE.md` y el README lo listan como documento normativo, pero nunca entró al repo. Falta escribir: mapa de puertos, protocolo del LCD HD44780, teclado hexadecimal, y la reserva de puertos para joystick A/D y flash futuros. Necesario antes de diseñar el módulo 10.

---

### [ ] 18. `docs/modulos/09-control.md` no existe *(nuevo, 16-ago-2026)*

Los ítems 2, 4 y 5 originales lo citaban, pero nunca entró al repo. Su contenido provisorio (ROM de 14 bits, 4 × AT28C256, IMM_SEL interno, reset del registro de microinstrucción con habilitaciones inactivas) está estacionado en `01-isa-spec.md` §10. Al escribirlo, mudar ese apéndice y fijar el desglose de bits de dirección. Recordar: **el layout de esta tarjeta (~18 integrados) se hace antes que el de ninguna otra** — si no entra en 100 × 160 mm, cambia el formato de las nueve.

---

### [x] 19. Validar la reconstrucción del ISA

**Resuelto (16-ago-2026):** validación completa en dos pasadas. Primera: los 12 puntos de decisión — incluida la elección explícita de **SLT con signo** (lógica `N ⊕ V` en la ALU), base PC+1, BEQ/BNE pisan banderas, subcampo de sistema, contador de 3 bits. Segunda: las secuencias de microcódigo de §6.3 ciclo por ciclo, por grupos. En la revisión surgió y se decidió la **restricción `rd ≠ rs` en JALR** (el enlace pisa la fuente; el ensamblador lo rechaza). `01-isa-spec.md` es **v0.3, NORMATIVO completo**, registro en su §11.

---

### [ ] 20. Módulo 01: sincronizar designadores y valores doc ↔ esquemático *(nuevo, 16-ago-2026)*

Verificación por script del esquemático contra `modulos/01-reloj.md`. El esquemático tiene 76 símbolos (U1–U8, R1–R13, RV1, C1–C17, D1–D3, SW1–SW4); el documento designa solo una parte. Discrepancias encontradas, **sin corregir** — hay que decidir en cada caso cuál de los dos manda:

| Punto | Documento | Esquemático |
|---|---|---|
| R7 (umbral de caída) | Trimpot 20 kΩ (decisión del ítem 11) | **Sigue siendo 12 kΩ fijo** — falta aplicar el cambio al esquemático |
| Resistor del LED | 1 kΩ | **R13 = 2k2** (el esquemático cumple la recomendación de LEDs de alta eficiencia del bus-spec §6; el doc quedó viejo) |
| SW3 / SW4 | SW3 = pulsador de reset; la llave SEL no tiene designador | **SW3 = llave SPDT (SEL), SW4 = pulsador de reset** |
| Capacitores | C2 = 1 µF (antirrebote), C3 = 1 µF (reset) | C2 = 10 nF; los 1 µF son C3, C4 y C7 — la numeración del doc no coincide |
| BOM: resistores 10 kΩ | 2 unidades | **4** (R6, R9, R10, R12) |
| BOM: 100 nF de desacople | 7 ("uno por chip") | **9** (C8–C16) con 8 integrados — verificar cuál es el de más o si el filtro de disparo está contado ahí |
| BOM: zócalos | "7" pero el detalle dice 6 + 2 = 8 | 8 integrados |

El punto crítico es el primero: la decisión del trimpot está tomada y documentada pero el esquemático no la refleja. Los demás son consistencia documental.

---

## Limpieza

### [x] 15. Eliminar el diagrama de datapath single-cycle

**Resuelto — N/A (16-ago-2026):** se verificó que el SVG no está en el repositorio; no hay nada que retirar.

---

### [x] 16. Estado en `componentes.md`

**Resuelto (16-ago-2026):** columna de estado (*por verificar / confirmado / recibido / descartado*) en todas las tablas de compra, fecha de última actualización en el encabezado, y leyenda de estados. Todos los ítems arrancan en "por verificar".

---

## Nota sobre las prioridades

Las contradicciones entre documentos (1–11) están resueltas. Quedan cuatro huecos de escritura: el **12 (mapa de memoria)** sigue siendo el único que bloquea diseño de tarjetas (módulos 7 y 8); el **19 (validación del ISA)** bloquea que el ISA sea normativo y conviene hacerlo antes que nada porque es una revisión de lectura, no de escritura; el 17 (E/S) bloquea el módulo 10; el 18 (control) se necesita antes del layout de la tarjeta 9 — que es el primer layout del proyecto.
