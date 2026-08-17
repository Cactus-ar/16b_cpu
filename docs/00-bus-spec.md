# CPU 16 bits — Especificación de bus y tarjetas

**Versión 0.2** · Documento normativo. Todo módulo debe cumplirlo. Ante conflicto con `01-isa-spec.md`, tiene precedencia el ISA.

---

## 1. Arquitectura

- Bus compartido de 16 bits, tri-state, con un solo emisor habilitado por vez.
- Ejecución multiciclo controlada por microcódigo (ROM en la tarjeta de control).
- ISA: load/store, instrucción fija de 16 bits, 8 registros de propósito general, R0 cableado a cero. Detalle completo en `01-isa-spec.md`.
- Familia lógica: **74HC**, alimentación **5 V**.
- Frecuencia: 0,7 Hz a 480 kHz (los tres rangos reales del oscilador — ver `modulos/01-reloj.md`).

### Regla de oro del tri-state

En todo momento, **exactamente un** módulo maneja el bus de datos. Dos emisores simultáneos generan contención: corriente alta, chips calientes y datos basura. Toda habilitación de salida es activa en bajo y por defecto (reset, arranque) debe estar **desactivada**.

---

## 2. Formato físico de la tarjeta

| Parámetro | Valor |
|---|---|
| Tamaño | 100 mm (alto) × 160 mm (ancho) |
| Conectores | Dos tiras de pines 2×28, paso 2,54 mm |
| Ubicación | Borde inferior de 160 mm |
| J1 | Datos, direcciones y banderas — desde 6 mm del borde izquierdo |
| J2 | Control y alimentación — desde 86 mm del borde izquierdo |
| Género | Macho en la tarjeta, hembra en el backplane |
| Espesor PCB | 1,6 mm |
| Capas | 2 (plano de masa en cara inferior) |

### Polarización — obligatoria

Las tiras de pines no tienen mecanismo antierror. En cada conector:

- Retirar el **pin A28** del header macho de la tarjeta.
- Tapar el contacto A28 correspondiente de la hembra del backplane (gota de epoxi o pin cortado).

Sin esto, tarde o temprano se enchufa una tarjeta rotada 180° y se destruye.

### Sujeción mecánica

Las tiras de pines no soportan peso. Cada slot necesita **guías laterales** para la tarjeta: perfil plástico en U, riel de aluminio o pieza impresa en 3D. Sin guías, el peso de la tarjeta hace palanca sobre los pines y termina fracturando soldaduras.

### Vida útil del conector

Una tira macho/hembra tolera del orden de decenas de inserciones, no cientos. Es suficiente para este proyecto, pero conviene no enchufar y desenchufar por deporte. Si aparecen conectores de borde recuperados de placas madre viejas (ISA, PCI), son mecánicamente superiores y el lado de la tarjeta sale gratis: son simplemente pads en el PCB.

### Reglas de layout obligatorias

1. Un capacitor cerámico de **100 nF** por integrado, entre VCC y GND, físicamente pegado al chip.
2. Un electrolítico de **10–47 µF** en la entrada de alimentación de cada tarjeta.
3. Plano de masa continuo en la cara inferior, sin cortes bajo los buses.
4. **Ninguna entrada CMOS flotante.** Toda entrada no usada va a VCC o a GND por pista o por resistor.
5. Serigrafía con nombre del módulo, versión y fecha en el borde superior.

---

## 3. Mapa de pines

Cada conector tiene dos filas de 28 contactos, designadas **A** (fila exterior, hacia el borde de la tarjeta) y **B** (fila interior).

### J1 — Datos, direcciones y banderas

**Fila A**

| Pin | Señal |
|---|---|
| A1 | GND |
| A2–A9 | D0 … D7 |
| A10 | GND |
| A11–A18 | D8 … D15 |
| A19 | GND |
| A20 | **Z** — bandera de cero |
| A21 | **C** — bandera de acarreo |
| A22 | GND |
| A23–A25 | +5 V |
| A26, A27 | GND |
| A28 | *(pin retirado — polarización)* |

**Fila B**

| Pin | Señal |
|---|---|
| B1 | GND |
| B2–B9 | A0 … A7 |
| B10 | GND |
| B11–B18 | A8 … A15 |
| B19 | GND |
| B20–B22 | Reservado |
| B23–B25 | +5 V |
| B26–B28 | GND |

**Bus de datos:** bidireccional, tri-state.

**Bus de direcciones:** dos emisores posibles, el PC y el MAR, arbitrados por `PC_AOUT_n`. Cuando está activo (bajo) maneja el PC; cuando está inactivo maneja el MAR. La tarjeta del MAR deriva su propia habilitación invirtiendo esta señal — no requiere un pin adicional.

**Banderas Z y C:** las emite la tarjeta de la ALU y las consume la de control para resolver saltos condicionales. No son tri-state: un solo emisor permanente.

### J2 — Control, temporización y alimentación

**Fila A**

| Pin | Señal | Función |
|---|---|---|
| A1 | GND | |
| A2 | **CLK** | Flanco activo: subida |
| A3 | GND | Blindaje del reloj |
| A4 | **CLK_n** | Reloj invertido |
| A5 | GND | Blindaje del reloj |
| A6 | **RESET_n** | Activo en bajo, asíncrono |
| A7 | **HALT_n** | Colector abierto, wired-OR |
| A8 | **IRQ_n** | Solicitud de interrupción, activa en bajo. La consume la tarjeta de control (ver `01-isa-spec.md` §7) |
| A9 | GND | |
| A10 | PC_OUT_n | Contador de programa al bus de datos |
| A11 | RA_OUT_n | Puerto A del banco de registros al bus |
| A12 | RB_OUT_n | Puerto B del banco de registros al bus |
| A13 | ALU_OUT_n | Salida de la ALU al bus |
| A14 | RAM_OUT_n | Memoria de datos al bus |
| A15 | ROM_OUT_n | Memoria de programa al bus |
| A16 | IMM_OUT_n | Inmediato extendido al bus |
| A17 | IO_OUT_n | Entrada de E/S al bus |
| A18 | GND | |
| A19–A22 | ALU_OP0 … ALU_OP3 | Selección de operación |
| A23 | PC_INC | Incremento del PC |
| A24 | **PC_AOUT_n** | PC maneja el bus de direcciones |
| A25 | GND | |
| A26, A27 | +5 V | |
| A28 | *(pin retirado — polarización)* | |

**Fila B**

| Pin | Señal | Función |
|---|---|---|
| B1 | GND | |
| B2 | PC_LD | Carga del PC desde el bus |
| B3 | REG_WE | Escritura en banco de registros |
| B4 | MAR_LD | Carga del registro de direcciones |
| B5 | IR_LD | Carga del registro de instrucción |
| B6 | RAM_WE_n | Escritura en memoria de datos |
| B7 | FLAGS_LD | Carga del registro de banderas |
| B8 | IO_LD | Carga del puerto de salida |
| B9 | **TMPA_LD** | Carga del operando A de la ALU |
| B10 | **TMPB_LD** | Carga del operando B de la ALU |
| B11 | GND | |
| B12–B14 | RSA0 … RSA2 | Dirección de lectura, puerto A |
| B15–B17 | RSB0 … RSB2 | Dirección de lectura, puerto B |
| B18–B20 | RSW0 … RSW2 | Dirección de escritura |
| B21 | GND | |
| B22, B23 | — | Reservado *(eran IMM_SEL0/1; el generador de inmediatos se mudó a la tarjeta de control junto con el IR y esas señales son internas)* |
| B24 | **PROG_WE_n** | Escritura en memoria de programa (carga) |
| B25 | — | Reservado |
| B26 | +5 V | |
| B27, B28 | GND | |

### Codificación de IMM_SEL

La tabla vive en `01-isa-spec.md` §5 — un solo lugar de verdad. Las señales `IMM_SEL0/1` son internas de la tarjeta de control.

**Nota sobre HALT_n:** cualquier tarjeta puede tirarlo a masa para detener la máquina. Requiere un resistor de pull-up de 4k7 **en el backplane, uno solo en todo el sistema**. Si cada tarjeta pone el suyo, los pull-ups se suman en paralelo y el nivel bajo no llega a bajar lo suficiente.

**Nota sobre las fases de microciclo:** no viajan por el backplane. El contador de microciclo queda dentro de la tarjeta de control, que ya emite las señales finales. Menos pines y menos ruido.

---

## 3b. Por qué la ALU necesita dos registros temporales

El bus entrega un valor por ciclo, pero la ALU necesita dos operandos simultáneos. Con un único temporal, la secuencia obligaría a que el banco de registros y la ALU manejen el bus en el mismo ciclo: contención.

Secuencia correcta para una operación de tres registros:

```
T1:  RA_OUT_n  + TMPA_LD                ; TMPA ← rs
T2:  RB_OUT_n  + TMPB_LD                ; TMPB ← rt
T3:  ALU_OUT_n + REG_WE + FLAGS_LD      ; rd ← TMPA op TMPB
```

(T0 es el fetch, común a todas las instrucciones; numeración según `01-isa-spec.md` §6.)

En ningún ciclo hay más de un emisor sobre el bus.

---

## 4. Convenciones de señal

- Sufijo **`_n`** = activa en bajo. Sin sufijo = activa en alto.
- Las cargas en registros ocurren en el **flanco de subida** de CLK.
- Las habilitaciones de salida son **por nivel**, no por flanco.
- Los buffers de bus son 74HC245 (bidireccional) o 74HC244 (unidireccional).

---

## 5. Orden de construcción

El proyecto tiene **nueve tarjetas** (más el PS/2 opcional): el registro de instrucción se fusionó con la unidad de control y el número 4 queda vacante para no invalidar referencias existentes.

| # | Módulo | Estado |
|---|---|---|
| 1 | Reloj | Esquemático listo |
| 2 | Tarjeta de pruebas de bus (LEDs + switches) | Pendiente |
| 3 | Contador de programa | Pendiente |
| 4 | — *(fusionado con el 9)* | |
| 5 | Banco de registros | Pendiente |
| 6 | ALU | Pendiente |
| 7 | Memoria de datos + MAR | Pendiente |
| 8 | Memoria de programa | Pendiente |
| 9 | Unidad de control (IR + microcódigo) | Pendiente |
| 10 | E/S y display | Pendiente |
| 11 | Teclado PS/2 *(opcional, posterior)* | Pendiente |

El módulo 2 no es opcional: sin una tarjeta que te deje ver el bus y forzar valores a mano, depurar los siguientes es a ciegas.

---

## 6. Alimentación

### Fuente

Fuente de laboratorio regulable, ajustada a **5,00 V** con limitación de corriente activa. No se emplea etapa rectificadora ni regulador a bordo: la fuente ya entrega continua regulada.

**El ajuste de tensión se verifica con multímetro sobre el backplane, no en el display de la fuente.** La caída en los cables de banana con 1 A es de decenas de milivoltios y el display no la ve.

### Consumo estimado

| Origen | Corriente |
|---|---|
| ~130 integrados 74HC a 480 kHz | ≈ 130 mA |
| SRAM y EEPROM | ≈ 350 mA |
| LEDs indicadores | 200 – 500 mA |
| **Total** | **0,7 – 1,0 A** |

La lógica CMOS consume poco. **El grueso del presupuesto de corriente son los LEDs indicadores.** Conviene usar LEDs de alta eficiencia con resistores de 2k2 a 4k7 en lugar de los 330 Ω habituales: se ven perfectamente y consumen entre un tercio y un quinto.

### Límite de corriente por etapa

Regla de trabajo obligatoria: **antes de energizar una tarjeta nueva, ajustar el límite de corriente al consumo esperado más 50 mA.**

| Etapa | Límite sugerido |
|---|---|
| Tarjeta de reloj sola | 100 mA |
| Reloj + tarjeta de pruebas | 250 mA |
| Cada tarjeta lógica adicional | +100 mA sobre el anterior |
| Sistema completo | 1,5 A |

Si al encender la fuente entra en corriente constante, **apagar y revisar**. Nunca subir el límite para "ver si arranca".

### Conector de entrada

Conector **polarizado** en el backplane — XT60, Molex de potencia o bornera con marcación indeleble. Con fichas banana es cuestión de tiempo invertir la polaridad, y 5 V al revés destruye toda la lógica CMOS simultáneamente.

Protección adicional recomendada: MOSFET de canal P en serie por el positivo, con la caída de tensión despreciable (decenas de milivoltios). Un diodo Schottky en serie **no sirve** acá: sus 0,4 V dejarían el riel en 4,6 V.

### Capacitores de bulk en el backplane

| Ubicación | Valor |
|---|---|
| Entrada de alimentación | 1000 µF electrolítico + 100 nF cerámico |
| Extremo izquierdo del riel | 100 µF electrolítico |
| Extremo derecho del riel | 100 µF electrolítico |

Los dos de los extremos no son redundantes: con diez tarjetas conmutando, la caída a lo largo del riel es medible, y la tarjeta más lejana de la entrada es la que más sufre.

### Corriente de arranque

El total de capacitancia del sistema ronda los 1,7 mF. Al encender, la fuente va a entrar momentáneamente en corriente constante mientras carga los capacitores. Es normal y no indica falla. Si la fuente oscila o no logra salir de ese estado, conectar la carga con la salida apagada y recién después habilitarla.

### Distribución en el backplane

- Pistas de alimentación de **3 mm mínimo**, o vertido de cobre dedicado.
- Plano de masa continuo en la cara inferior, sin cortes bajo los buses.
- Los pines de +5 V y GND de cada conector se unen todos entre sí: son paralelos deliberados para repartir corriente y bajar la inductancia.

### Detección de caída de tensión

Con fuente de laboratorio, el modo de corriente constante **es** una caída de tensión: ante un corto en cualquier tarjeta, el riel se hunde de forma gradual en lugar de cortarse. Durante ese descenso la lógica entra en zona indefinida y la máquina puede escribir basura en memoria antes de detenerse.

Por eso la tarjeta de reloj incorpora detección de caída: un comparador que fuerza `RESET_n` cuando el riel baja de aproximadamente 4,6 V. Ver `modulos/01-reloj.md`.

---

## 7. Criterio de fabricación

- **A PCB** va únicamente lo que ya funcionó al menos una vez.
- Lo que tenga lógica sin validar se arma primero en placa universal.
- El reloj califica para PCB directo: circuito conocido, sin incertidumbre de diseño.
