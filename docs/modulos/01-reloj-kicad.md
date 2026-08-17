# KiCad — Módulo 01 (Reloj)

Guía de implementación y netlist completo. Complemento de `01-reloj.md`.

---

## 1. Estrategia: plantilla antes que tarjeta

Van a existir diez tarjetas con el mismo formato, los mismos conectores y las mismas reglas. **No dibujar esta como si fuera única.**

Crear primero un proyecto `plantilla-tarjeta` con:

- Contorno de placa de 160 × 100 mm en `Edge.Cuts`.
- J1 y J2 ya colocados y con su numeración de pines definitiva.
- Agujeros de montaje en las cuatro esquinas, a 4 mm de cada borde.
- Netclasses y reglas de diseño ya configuradas.
- Serigrafía con nombre de proyecto, versión y fecha en el borde superior.

Cada módulo nuevo se hace copiando esa carpeta y renombrando. Ahorra horas y garantiza que las diez tarjetas encajen en el mismo backplane.

### Ubicación de conectores

Con origen en la esquina inferior izquierda de la placa:

| Conector | Pin A1 (x, y) | Orientación |
|---|---|---|
| J1 | 6 mm, 4 mm | Fila A hacia el borde inferior |
| J2 | 86 mm, 4 mm | Fila A hacia el borde inferior |

Cada tira de 2×28 mide 71,1 mm. J1 ocupa de 6 a 77,1 mm; J2 de 86 a 157,1 mm. Quedan casi 9 mm de separación entre ambas y 3 mm de margen al borde derecho.

**Fijar estas coordenadas y no tocarlas nunca más.** Un milímetro de diferencia entre tarjetas y no entran en el backplane.

---

## 2. Netclasses

Configurar en *Archivo → Configuración de placa → Reglas de diseño → Netclasses*.

| Netclass | Ancho de pista | Separación | Nets |
|---|---|---|---|
| `Power` | 1,5 mm | 0,25 mm | +5V |
| `Clock` | 0,4 mm | 0,4 mm | CLK, CLK_N, CLK_BUF, CLK_N_BUF, CLK_RAW |
| `Default` | 0,3 mm | 0,2 mm | Todo lo demás |

La separación mayor en la clase `Clock` no es capricho: los flancos rápidos acoplan a las pistas vecinas. En esta tarjeta importa poco, pero la plantilla se hereda a las que sí van a tener buses paralelos.

GND va por vertido de cobre en la cara inferior, sin pistas dedicadas.

---

## 3. Símbolos necesarios

Todos están en las bibliotecas que trae KiCad:

| Componente | Biblioteca : Símbolo |
|---|---|
| TLC555 | `Timer:TLC555` |
| 74HC00 | `74xx:74HC00` |
| 74HC04 | `74xx:74HC04` |
| 74HC74 | `74xx:74HC74` |
| 74HC14 | `74xx:74HC14` |
| 74HC244 | `74xx:74HC244` |
| LM393 | `Comparator:LM393` |
| TL431 | `Reference_Voltage:TL431LI` |
| Conector 2×28 | `Connector_Generic:Conn_02x28_Odd_Even` |

**Atención con la numeración del conector.** El símbolo `Odd_Even` numera 1, 3, 5… en una fila y 2, 4, 6… en la otra. La especificación usa A1–A28 y B1–B28. La correspondencia es:

- **A*n*** → pin impar `2n − 1`
- **B*n*** → pin par `2n`

Ejemplo: A2 (que lleva CLK en J2) es el pin 3 del símbolo. B2 es el pin 4.

Conviene renombrar los pines del símbolo a la nomenclatura A/B en una copia local de la biblioteca. Se hace una vez, en la plantilla, y evita errores en las diez tarjetas.

---

## 4. Netlist completo

### Alimentación y desacople

| Net | Conexiones |
|---|---|
| `+5V` | U1.8, U1.4, U2.8, U2.4, U3.14, U4.14, U5.14, U5.1, U5.4, U5.10, U5.13, U6.14, U7.20, U8.8, J1.A23–A25, J1.B23–B25, J2.A26, J2.A27, J2.B26 |
| `GND` | U1.1, U2.1, U3.7, U4.7, U5.7, U6.7, U7.10, U7.1, U7.19, U8.4, y todos los pines GND de J1/J2 |

Desacople: **C10–C17**, 100 nF cerámico de +5 V a GND, uno junto a cada integrado.
Bulk: **C18**, 47 µF electrolítico en la entrada de alimentación de la tarjeta.

### Oscilador (U1)

| Net | Conexiones |
|---|---|
| `OSC_D` | U1.7, R1, R2a |
| `OSC_C` | U1.2, U1.6, RV1.2, RV1.3, SW1 común |
| `ASTABLE` | U1.3, U3.1 |
| `CTRL1` | U1.5, C1 |

| Componente | Valor | De | A |
|---|---|---|---|
| R1 | 1 kΩ | +5V | OSC_D |
| R2a | 1 kΩ | OSC_D | RV1.1 |
| RV1 | 1 MΩ | RV1.1 | OSC_C (pines 2 y 3 unidos) |
| C1 | 10 nF | CTRL1 | GND |
| C4 | 1 µF | SW1 pos. 1 | GND |
| C5 | 10 nF | SW1 pos. 2 | GND |
| C6 | 1 nF | SW1 pos. 3 | GND |

### Antirrebote (U2)

| Net | Conexiones |
|---|---|
| `TRIG_RAW` | RP, SW2, R9 |
| `TRIG_F` | U2.2, R9, C8 |
| `MONO_C` | U2.6, U2.7, R3, C7 |
| `MANUAL` | U2.3, U3.5 |
| `CTRL2` | U2.5, C2 |

| Componente | Valor | De | A |
|---|---|---|---|
| RP | 10 kΩ | +5V | TRIG_RAW |
| SW2 | Pulsador | TRIG_RAW | GND |
| R9 | 10 kΩ | TRIG_RAW | TRIG_F |
| C8 | 100 nF | TRIG_F | GND |
| R3 | 470 kΩ | +5V | MONO_C |
| C7 | 1 µF | MONO_C | GND |
| C2 | 10 nF | CTRL2 | GND |

### Selección de modo y compuerta (U3, U4)

| Pin | Net |
|---|---|
| U3.1 | ASTABLE |
| U3.2 | SEL_N |
| U3.3 | N1 |
| U3.4 | MANUAL |
| U3.5 | SEL |
| U3.6 | N2 |
| U3.9 | N1 |
| U3.10 | N2 |
| U3.8 | CLK_RAW |
| U3.12 | CLK_RAW |
| U3.13 | HALT_SYNC |
| U3.11 | CLK_G |

| Pin | Net | Función |
|---|---|---|
| U4.1 → U4.2 | SEL → SEL_N | Inversión del selector |
| U4.3 → U4.4 | CLK_G → CLK | Reloj final |
| U4.5 → U4.6 | CLK → CLK_N | Reloj invertido |
| U4.9 → U4.8 | CLK_RAW → CLK_RAW_N | Flanco de muestreo de HALT |
| U4.11 → U4.10 | CLK_N → LED_A | Indicador |
| U4.13 → U4.12 | GND → *(sin usar)* | Entrada terminada |

| Componente | De | A |
|---|---|---|
| SW3 (conmutador) | común = SEL | pos. 1 = +5V, pos. 2 = GND |
| R13 = 2k2 | LED_A | LED1 ánodo |
| LED1 | cátodo | GND |

Usar conmutador de dos posiciones, no pulsador con pull-down: la entrada nunca queda flotando.

### Sincronización de HALT (U5)

| Pin | Net |
|---|---|
| U5.1 (1CLR_n) | +5V |
| U5.2 (1D) | HALT_N |
| U5.3 (1CLK) | CLK_RAW_N |
| U5.4 (1PRE_n) | +5V |
| U5.5 (1Q) | HALT_SYNC |
| U5.6 (1Q_n) | *(sin conectar — es salida)* |
| U5.10, U5.13 | +5V |
| U5.11, U5.12 | GND |
| U5.8, U5.9 | *(sin conectar — son salidas)* |

| Componente | Valor | De | A |
|---|---|---|---|
| R4 | 4k7 | +5V | HALT_N |

**R4 es el único pull-up de `HALT_n` de todo el sistema.** Ninguna otra tarjeta lleva uno.

### Reset y detección de caída (U6, U8, D2)

| Pin | Net |
|---|---|
| U6.1 | RST_RC |
| U6.2 | RST_A |
| U6.3 | RST_A |
| U6.4 | RESET_N |
| U6.5, U6.9, U6.11, U6.13 | GND |
| U8.2 (IN−) | VREF |
| U8.3 (IN+) | VSENSE |
| U8.1 (OUT) | RST_RC |
| U8.5 (IN+2) | +5V |
| U8.6 (IN−2) | GND |
| U8.7 (OUT2) | *(sin conectar)* |

| Componente | Valor | De | A |
|---|---|---|---|
| R5 | 100 kΩ | +5V | RST_RC |
| C3 | 1 µF | RST_RC | GND |
| D1 | 1N4148 | ánodo = RST_RC | cátodo = +5V |
| SW4 | Pulsador | RST_RC | GND |
| R11 | 1k5 | +5V | VREF |
| D2 | TL431 | cátodo y ref = VREF | ánodo = GND |
| R6 | 10 kΩ | +5V | VSENSE |
| R7 | 12 kΩ | VSENSE | GND |
| R12 | 10 kΩ | +5V | RST_RC |
| R8 | 470 kΩ | RST_RC | VSENSE |

D1 descarga C3 al apagar. Sin él, un ciclo rápido de apagado y encendido deja el capacitor cargado y la tarjeta arranca sin pulso de reset.

### Buffer de salida (U7)

| Pines | Net |
|---|---|
| U7.2, U7.4, U7.6, U7.8 | CLK |
| U7.18, U7.16, U7.14, U7.12 | CLK_BUF |
| U7.11, U7.13, U7.15, U7.17 | CLK_N |
| U7.9, U7.7, U7.5, U7.3 | CLK_N_BUF |
| U7.1, U7.19 | GND |

### Conexión al backplane

| Net | Pin |
|---|---|
| CLK_BUF | J2.A2 |
| CLK_N_BUF | J2.A4 |
| RESET_N | J2.A6 |
| HALT_N | J2.A7 |
| +5V | J1.A23–A25, J1.B23–B25, J2.A26, J2.A27, J2.B26 |
| GND | J1.A1, A10, A19, A22, A26, A27; J1.B1, B10, B19, B26–B28; J2.A1, A3, A5, A9, A18, A25; J2.B1, B11, B21, B27, B28 |

Todos los demás pines de J1 y J2 quedan **sin conectar** en esta tarjeta. Marcarlos con el símbolo de *no conectado* para que el ERC no proteste.

**J1.A28 y J2.A28 no se sueldan:** son los pines de polarización, se retiran de la tira macho.

---

## 5. Verificación antes de fabricar

1. **ERC sin errores.** Las advertencias por pines sin conectar se silencian con el símbolo correspondiente, no ignorándolas.
2. **DRC sin errores**, con las netclasses aplicadas.
3. **Un capacitor de 100 nF por integrado**, verificado uno por uno en el layout, no en el esquemático. Es el error que más se cuela.
4. **Ninguna entrada CMOS sin conectar.** Recorrer U3, U4, U5, U6 pin por pin.
5. **Distancia de los conectores medida en el visor 3D**, comparada contra las coordenadas de la sección 1.
6. **Serigrafía legible**: designadores visibles con los componentes montados, y marca del pin 1 en cada zócalo.
7. **Imprimir el layout en papel a escala 1:1** y apoyar los componentes físicos encima. Detecta huellas equivocadas antes de gastar en fabricación — sobre todo el potenciómetro, los pulsadores y el conmutador rotativo, que son los que más varían entre fabricantes.

El paso 7 parece exagerado y es el que más dinero ahorra.
