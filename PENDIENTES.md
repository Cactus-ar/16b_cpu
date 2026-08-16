# Pendientes — Corrección de discrepancias

Detectadas en la revisión cruzada de documentos, agosto 2026. Ordenadas por severidad.

Cuando un ítem se resuelva, marcarlo y anotar en qué documento quedó la versión buena.

---

## Críticas — contradicciones directas entre documentos

### [ ] 1. Tabla de `IMM_SEL` inconsistente

`00-bus-spec.md` define:
```
01 = imm9 signo
11 = imm9 ceros
```

`01-isa-spec.md` define:
```
01 = imm8 << 8
11 = imm8 ceros
```

**Correcta: la del ISA.** La del bus quedó de antes de resolver la composición de constantes de 16 bits con `LUI` + `ORI`.

**Acción:** eliminar la tabla de `00-bus-spec.md` y remitir al ISA. Un solo lugar de verdad.

---

### [ ] 2. `IMM_SEL0` / `IMM_SEL1` figuran como señales del backplane

`00-bus-spec.md` las lista en J2, pines B22 y B23. `modulos/09-control.md` establece que son **internas** de la tarjeta de control, porque el generador de inmediatos se mudó allí junto con el IR.

**Acción:** marcar B22 y B23 como reservados en `00-bus-spec.md`.

---

### [ ] 3. Las interrupciones no están documentadas en ninguna parte

Se decidió implementarlas en la v1 y no quedó registrado. Falta:

- Flip-flop de habilitación (`IE`) en la tarjeta de control.
- Convención de retorno: `R7 ← PC` al atender, mismo camino de datos que `JALR`.
- Dirección del vector de interrupción.
- Instrucciones de habilitar, deshabilitar y retornar.
- Secuencia de atención en el microcódigo:
  ```
  T0:  si (IRQ_n=0 y IE=1) → secuencia de atención
  Ta:  PC_OUT_N + REG_WE(R7)
  Tb:  IMM_OUT_N(vector) + PC_LD, IE ← 0
  ```
- `IRQ_n` deja de ser "Reservado" en `00-bus-spec.md`.

**Decisión pendiente:** las instrucciones de interrupción pueden ocupar uno de los tres opcodes libres (`1101`–`1111`) o entrar dentro de un **prefijo de expansión**. El prefijo se propuso para no quedarse sin opcodes a futuro y sigue sin decidirse.

---

### [ ] 4. Dimensionamiento de la ROM de control desactualizado

`modulos/09-control.md` calcula 13 bits de dirección y especifica 4 × 28C64 (8K × 8).

Con `IRQ_n` e `IE` en la dirección son **14 bits**, y el 28C64 queda sin margen.

**Acción:** recalcular con 14 bits y actualizar a AT28C256 (32K × 8, 15 bits de dirección).

---

### [ ] 5. Conflicto de números de parte entre módulo 09 y componentes

| Uso | `09-control.md` | `componentes.md` |
|---|---|---|
| ROM de microcódigo | 28C64 | AT28C256 |
| Generador de inmediatos | 28C128 | AT28C256 |

**Correcta: `componentes.md`.** La estandarización en un único chip de 28 pines para toda la memoria fue una decisión posterior.

---

## Importantes — documentos desactualizados

### [ ] 6. README con información vieja del ISA

| Punto | Dice | Debe decir |
|---|---|---|
| Formato L | `op(4) · rd(3) · imm(9)` | `op(4) · rd(3) · —(1) · imm8(8)` |
| `LUI` | `rd ← imm << 7` | `rd ← imm8 << 8` |
| `HALT` | No aparece | Opcode `1100` |
| Opcodes libres | `1100`–`1111` | `1101`–`1111` |

---

### [ ] 7. README y bus-spec listan diez módulos

El módulo 4 (registro de instrucción) se fusionó con el 9. **El proyecto tiene nueve tarjetas** más el PS/2 opcional.

**Acción:** actualizar la tabla de módulos en `README.md` y en la sección de orden de construcción de `00-bus-spec.md`. Mantener el hueco en el número 4 para no invalidar referencias existentes.

---

### [ ] 8. Estructura de carpetas del README obsoleta

Muestra el layout plano anterior (`Cpu16_cad/`, documentos sin numerar). Actualizar a la estructura actual.

---

## Menores

### [ ] 9. Frecuencia máxima inconsistente

`00-bus-spec.md` dice "1 Hz a 500 kHz". El circuito real da **0,7 Hz a 480 kHz** según los tres rangos de capacitor. Unificar en el valor real.

---

### [ ] 10. Numeración de microciclos inconsistente

`00-bus-spec.md` sección 3b usa `T2/T3/T4` para la secuencia de la ALU. `01-isa-spec.md` usa `T1/T2/T3`. Unificar con el ISA.

---

### [ ] 11. Módulo 01: R7 y nomenclatura del pull-up

- El documento especifica **R7 = 12 kΩ fijo**. Se decidió cambiarlo por un **trimpot multivuelta de 20 kΩ**: con resistores del 10 %, el umbral de detección de caída puede irse a 5,20 V y dejar la máquina en reset permanente.
- El paso 8 del procedimiento de prueba pasa de "verificar" a "ajustar y verificar".
- El pull-up de `HALT_N` figura como `RP` en la documentación y como **`R10`** en el esquemático. `RP` no es un designador válido en KiCad.

---

## Huecos — falta escribir

### [ ] 12. `docs/03-memoria-spec.md` no existe

Ningún documento define cuánta memoria hay ni qué vive en cada rango. Falta decidir y escribir:

- Tamaño de RAM y de ROM de programa.
- Mapa de memoria: monitor, BIOS, programa de usuario, buffer de pantalla, pila, vector de interrupción.
- Rangos que decodifica cada tarjeta.
- Previsión de expansión: segundo banco, zócalos sin poblar.

**Criterio del autor:** el costo no es restricción, la variable a minimizar es tener que rehacer. Diseñar el decodificador para el rango completo de 64K palabras aunque se pueble menos.

---

### [ ] 13. Registro de decisiones de diseño

`docs/decisiones.md`. El razonamiento detrás de cada decisión mayor está en `CLAUDE.md` en forma resumida, pero merece un documento propio con el desarrollo completo.

---

### [ ] 14. Prefijo de expansión de opcodes

Quedan tres opcodes libres. Se propuso reservar uno como **prefijo de expansión**: una instrucción que modifica el significado de la siguiente, dando espacio ilimitado a futuro sin costo hoy.

Sin decidir. Relevante para multiplicación por hardware, corrimientos múltiples y acceso a bytes, que podrían hacer falta para el buffer gráfico.

---

## Limpieza

### [ ] 15. Eliminar el diagrama de datapath single-cycle

Si el SVG del datapath está en el repo, retirarlo o marcarlo como histórico. Muestra la arquitectura single-cycle con multiplexores, abandonada en favor del bus compartido multiciclo. Contradice todos los documentos actuales.

---

### [ ] 16. Estado en `componentes.md`

Agregar columna de estado en las tablas: *por verificar / confirmado / recibido / descartado*, y fecha de última actualización en el encabezado. El valor del documento está en mantenerse al día, no en la foto inicial.

---

## Nota sobre las prioridades

Los ítems 1 a 5 son contradicciones que llevarían a construir mal. Los ítems 6 a 8 confunden a quien lea el repo. El ítem 12 es el único que **bloquea el diseño de tarjetas**: sin mapa de memoria no se pueden diseñar los módulos 7 y 8.
