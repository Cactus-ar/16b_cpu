# Módulo 01 — Reloj y temporización

**Versión 0.1** · Primera tarjeta del sistema · Conforme a `bus-spec-cpu16.md`

---

## 1. Funciones de la tarjeta

1. Oscilador libre de frecuencia y rango ajustables (0,7 Hz – 480 kHz).
2. Pulso único manual, con antirrebote, para avanzar la CPU ciclo por ciclo.
3. Detención sincrónica por `HALT_n` sin generar pulsos espurios.
4. Generación de `RESET_n`: al encender y por pulsador.
5. Distribución de `CLK` y `CLK_n` al backplane con capacidad de carga adecuada.
6. Alojamiento del único pull-up de `HALT_n` del sistema.

---

## 2. Lista de integrados

| Ref | Chip | Función |
|---|---|---|
| U1 | TLC555 | Astable — oscilador principal |
| U2 | TLC555 | Monoestable — antirrebote del pulsador |
| U3 | 74HC00 | Selección de modo y compuerta de halt |
| U4 | 74HC04 | Inversores auxiliares |
| U5 | 74HC74 | Sincronización de HALT |
| U6 | 74HC14 | Schmitt trigger — generación de reset |
| U7 | 74HC244 | Buffer de salida al backplane |
| U8 | LM393 | Comparador — detección de caída de tensión |

U1 y U2 pueden reemplazarse por un único **TLC556** (doble 555) si se prefiere ahorrar espacio.

---

## 3. Oscilador — U1

Configuración astable estándar.

| Pin U1 | Conexión |
|---|---|
| 1 | GND |
| 2 | Unido a pin 6 y a CT |
| 3 | Salida → U3 pin 1 |
| 4 | +5 V |
| 5 | 10 nF a GND |
| 6 | Unido a pin 2 |
| 7 | Unión de R1 y R2 |
| 8 | +5 V |

- **R1** = 1 kΩ, de +5 V a pin 7.
- **R2** = potenciómetro 1 MΩ **en serie con 1 kΩ fijo**, de pin 7 a pin 6.
- **CT** = capacitor seleccionado por SW1, de pin 6 a GND.

El resistor fijo de 1 kΩ en serie con el potenciómetro no es opcional: con el pote en cero, el pin 7 descargaría el capacitor sin límite de corriente y el chip se destruye.

### Selección de rango — SW1

Conmutador rotativo de 3 posiciones, un polo:

| Posición | CT | Rango | Uso |
|---|---|---|---|
| 1 | 1 µF | 0,7 – 480 Hz | Depuración visual |
| 2 | 10 nF | 72 Hz – 48 kHz | Pruebas intermedias |
| 3 | 1 nF | 720 Hz – 480 kHz | Ejecución normal |

`f ≈ 1,44 / ((R1 + 2·R2) · CT)`

### Nota sobre ciclo de trabajo

La configuración da entre 50 % y 67 % según la posición del potenciómetro. Es irrelevante para lógica disparada por flanco. Si más adelante hiciera falta un 50 % exacto, se agrega un diodo 1N4148 en paralelo con R2, cátodo hacia el pin 6.

---

## 4. Antirrebote — U2

Configuración monoestable.

| Pin U2 | Conexión |
|---|---|
| 1 | GND |
| 2 | Nodo de disparo (ver abajo) |
| 3 | Salida → U3 pin 5 |
| 4 | +5 V |
| 5 | 10 nF a GND |
| 6 | Unido a pin 7 y a C2 |
| 7 | Unido a pin 6, y R3 a +5 V |
| 8 | +5 V |

- **R3** = 470 kΩ, **C2** = 1 µF → pulso de ≈ 517 ms.
- **RP** = 10 kΩ de +5 V al nodo de disparo (pull-up).
- **SW2** = pulsador momentáneo, del nodo de disparo a GND.
- **Filtro de disparo**: 10 kΩ en serie hacia el pin 2, con 100 nF del pin 2 a GND.

### Por qué el filtro RC en el disparo

Un pulsador mecánico rebota tanto al cerrar como al abrir. El rebote de cierre no molesta: el primer contacto dispara el monoestable y los siguientes caen dentro de los 517 ms de pulso ya activo. El rebote de **apertura** sí es peligroso — puede volver a llevar el pin 2 por debajo de 1/3 de VCC y generar un segundo flanco. El filtro RC lentifica ese retorno lo suficiente para que los rebotes no crucen el umbral.

---

## 5. Selección de modo y halt — U3, U4, U5

### Lógica

```
~SEL     = INV(SEL)                      ; U4a
N1       = NAND(ASTABLE, ~SEL)           ; U3a
N2       = NAND(MANUAL,  SEL)            ; U3b
CLK_RAW  = NAND(N1, N2)                  ; U3c
CLK_G    = NAND(CLK_RAW, HALT_SYNC)      ; U3d
CLK      = INV(CLK_G)                    ; U4b
CLK_n    = INV(CLK)                      ; U4c
```

Con `SEL` en bajo pasa el astable; en alto, el pulso manual.

### El problema del halt asincrónico

Si `HALT_n` se activa mientras `CLK` está en alto, la compuerta corta el reloj de inmediato y produce un **pulso enano**: un flanco de subida seguido de una bajada prematura. Los registros pueden capturar datos inestables, y el síntoma resultante — la máquina se corrompe de a ratos, sin patrón — es de los más difíciles de diagnosticar del proyecto.

### Solución: sincronizar HALT

`HALT_n` se muestrea con un flip-flop D en el **flanco de bajada** de `CLK_RAW`:

| Pin U5 | Conexión |
|---|---|
| 1 (1CLR_n) | +5 V |
| 2 (1D) | HALT_n (desde backplane, tras pull-up) |
| 3 (1CLK) | ~CLK_RAW (desde U4d) |
| 4 (1PRE_n) | +5 V |
| 5 (1Q) | HALT_SYNC → U3 pin 12 |

Así `HALT_SYNC` solo cambia cuando `CLK_RAW` ya está en bajo, y la compuerta nunca interrumpe un pulso a medias.

**La segunda mitad de U5 queda sin usar.** Sus entradas (pines 10, 11, 12, 13) van a +5 V, no al aire. Entrada CMOS flotante = oscilación, consumo y ruido.

### Pull-up de HALT_n

**R4 = 4k7 de `HALT_n` a +5 V, en esta tarjeta y en ninguna otra.**

---

## 6. Reset — U6

Reset por encendido y por pulsador, con Schmitt trigger.

- **R5** = 100 kΩ de +5 V al nodo RC.
- **C3** = 1 µF del nodo RC a GND.
- **D1** = 1N4148 en paralelo con R5, cátodo hacia +5 V (descarga C3 al apagar).
- **SW3** = pulsador momentáneo, del nodo RC a GND.
- Nodo RC → U6a → U6b → `RESET_n`.

Constante de tiempo ≈ 100 ms. Los dos inversores Schmitt en cascada dan una pendiente limpia sin importar cuán lento suba el RC.

Las cuatro compuertas restantes de U6 llevan sus entradas a GND.

### Detección de caída de tensión

Con fuente de laboratorio limitada por corriente, un corto no corta la alimentación: la hunde. El riel desciende de forma gradual y la lógica atraviesa la zona indefinida — ni uno ni cero — durante milisegundos. En ese tramo la máquina puede seguir generando pulsos de reloj y escribir basura en memoria antes de detenerse.

Detección con un TL431 como referencia y un comparador:

- **U8** = LM393 (comparador doble, colector abierto).
- **D2** = TL431 en configuración shunt: ánodo a GND, cátodo y referencia unidos al nodo `VREF`.
- **R11** = 1k5 de +5 V a `VREF` (polarización del TL431, ≈ 1,7 mA).
- Divisor de muestra: **R6 = 10 kΩ** de +5 V al nodo `VSENSE`, **R7 = 12 kΩ** de `VSENSE` a GND.
- `VSENSE` → entrada **no inversora** (U8 pin 3).
- `VREF` → entrada **inversora** (U8 pin 2).
- **R12** = 10 kΩ de la salida (U8 pin 1) a +5 V — el colector abierto lo necesita.
- **R8** = 470 kΩ de la salida a `VSENSE`, realimentación positiva para histéresis.
- Salida (U8 pin 1) unida al nodo RC del reset.

### Verificación de umbrales

| Estado del riel | VSENSE | Salida |
|---|---|---|
| 5,00 V (normal) | 2,75 V | Alta — no interviene |
| 4,54 V | 2,50 V | **Conmuta a baja — dispara reset** |
| Recuperando | — | Libera recién a 4,64 V |

La histéresis resultante es de unos 100 mV. Es deliberadamente angosta: si fuera mucho mayor, una caída normal bajo carga podría dejar la máquina en reset permanente.

**La polaridad importa.** `VSENSE` va a la entrada no inversora y `VREF` a la inversora, no al revés. Invertidas, el comparador mantiene el reset activo mientras el riel está sano y lo libera cuando se hunde — exactamente lo contrario de lo buscado.

Sin R8 el comparador oscila al rondar el umbral y genera una ráfaga de resets en lugar de uno solo.

La segunda mitad de U8 queda sin usar: entrada no inversora a +5 V, inversora a GND, salida al aire (es colector abierto, no requiere terminación).

---

## 7. Salida al backplane — U7

74HC244, con salidas en paralelo para aumentar la capacidad de corriente:

| Señal | Entradas | Salidas |
|---|---|---|
| CLK | 1A1–1A4 unidas | 1Y1–1Y4 unidas |
| CLK_n | 2A1–2A4 unidas | 2Y1–2Y4 unidas |

Ambos habilitadores (`1OE_n` pin 1, `2OE_n` pin 19) a GND: siempre activos.

Cuatro salidas en paralelo dan del orden de 24 mA, suficiente para el bus con todas las tarjetas conectadas. Una sola salida HC daría flancos demasiado lentos en el extremo alto del rango.

### Indicador

LED verde con resistor de 1 kΩ desde `CLK`. Solo es visible en el rango lento; a partir de unos cientos de hercios se ve como brillo continuo.

---

## 8. Pines usados del backplane

Esta tarjeta es la única que **emite** reloj y reset.

| Conector | Pin | Señal | Dirección |
|---|---|---|---|
| J2 | A2 | CLK | Salida |
| J2 | A4 | CLK_n | Salida |
| J2 | A6 | RESET_n | Salida |
| J2 | A7 | HALT_n | Entrada + pull-up |
| J1/J2 | varios | +5 V, GND | Alimentación |

Todos los demás pines de J1 y J2 quedan **sin conectar** en esta tarjeta.

---

## 9. Lista de materiales

| Cant. | Componente |
|---|---|
| 2 | TLC555 o LMC555 (o 1 × TLC556) |
| 1 | 74HC00 |
| 1 | 74HC04 |
| 1 | 74HC74 |
| 1 | 74HC14 |
| 1 | 74HC244 |
| 1 | LM393 |
| 1 | TL431 |
| 1 | Resistor 1k5 |
| 1 | Resistor 12 kΩ |
| 1 | Resistor 470 kΩ (histéresis) |
| 7 | Zócalo DIP (6 × 14–20 pines, 2 × 8 pines) |
| 7 | Capacitor cerámico 100 nF (desacople, uno por chip) |
| 1 | Capacitor electrolítico 47 µF |
| 2 | Capacitor cerámico 10 nF (pin 5 de cada 555) |
| 1 | Capacitor cerámico 100 nF (filtro de disparo) |
| 1 | Capacitor cerámico 1 nF |
| 1 | Capacitor cerámico 10 nF (rango medio) |
| 2 | Capacitor electrolítico 1 µF |
| 1 | Potenciómetro 1 MΩ lineal |
| 2 | Resistor 1 kΩ |
| 2 | Resistor 10 kΩ |
| 1 | Resistor 4k7 |
| 1 | Resistor 100 kΩ |
| 1 | Resistor 470 kΩ |
| 1 | Resistor 1 kΩ (LED) |
| 1 | Diodo 1N4148 |
| 1 | LED verde 5 mm |
| 1 | Conmutador rotativo 3 posiciones |
| 1 | Llave conmutadora 2 posiciones (SEL) |
| 2 | Pulsador momentáneo (paso, reset) |
| 2 | Tira de pines macho 2×28, paso 2,54 mm |

---

## 10. Procedimiento de prueba

Ejecutar **antes** de conectar la tarjeta al backplane.

**Paso 1 — Alimentación.** Sin integrados en los zócalos, aplicar 5 V. Verificar con multímetro que cada zócalo tenga 5 V y masa en los pines correctos. Confirmar que no haya continuidad entre 5 V y GND.

**Paso 2 — Consumo.** Colocar los integrados. El consumo total debe rondar los pocos miliamperios. Si supera 50 mA, apagar de inmediato: casi seguro hay una entrada flotante o un chip invertido.

**Paso 3 — Astable.** Rango lento, potenciómetro al máximo. El LED debe parpadear en torno a 1 Hz. Girar el potenciómetro y verificar que acelera de forma continua.

**Paso 4 — Rangos.** Con el analizador lógico en `CLK`, medir la frecuencia en las tres posiciones de SW1, en ambos extremos del potenciómetro. Anotar los seis valores.

**Paso 5 — Manual.** Pasar SEL a manual. Cada pulsación debe producir **exactamente un** flanco de subida. Verificar con el analizador lógico, no a ojo: el rebote es invisible a simple vista. Repetir veinte veces seguidas.

**Paso 6 — Halt.** Con el astable corriendo en rango lento, poner `HALT_n` a masa. El reloj debe detenerse **en bajo**, siempre. Repetir diez veces observando el último pulso en el analizador: si alguna vez se detiene en alto o aparece un pulso más corto que los demás, la sincronización de U5 está mal conectada.

**Paso 7 — Reset.** `RESET_n` debe estar en bajo unos 100 ms al encender y luego pasar a alto. El pulsador debe llevarlo a bajo mientras se mantiene apretado.

**Paso 8 — Caída de tensión.** Bajar lentamente la tensión de la fuente desde 5,0 V. `RESET_n` debe activarse en torno a 4,54 V y mantenerse activo por debajo. Volver a subir: debe liberarse cerca de 4,64 V, sin oscilar. Si en la transición aparecen múltiples resets, falta o está mal el resistor de histéresis. Si el reset nunca se libera, revisar la polaridad de las entradas del comparador.

**Paso 9 — Flancos.** Con el analizador en el rango rápido, verificar que `CLK` y `CLK_n` sean complementarios y sin escalones ni rebotes en las transiciones.

Ningún resultado se da por bueno sin haberlo visto en el analizador lógico.
