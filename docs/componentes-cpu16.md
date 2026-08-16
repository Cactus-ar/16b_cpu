# Componentes — Lista maestra y riesgo de disponibilidad

**Versión 0.1** · Verificar disponibilidad **antes** de continuar el diseño

Este documento existe porque no tiene sentido diseñar sobre componentes que no se van a conseguir. Las cantidades cubren el proyecto completo, incluidas las tarjetas todavía no diseñadas.

---

## 1. Memoria — Prioridad de compra máxima

**Comprar toda la memoria del proyecto de una vez**, aunque las tarjetas tarden meses en existir. Es el componente con mayor riesgo de discontinuación y el más difícil de sustituir a mitad de camino.

### Decisión de diseño: huella única

El **62256 (SRAM) y el 28C256 (EEPROM) comparten el pinout JEDEC de 28 pines**: ambos son 32K × 8, con las mismas líneas de dirección y datos en las mismas posiciones.

Toda la memoria del proyecto usa esa única huella. Ventajas:

- Un solo tipo de zócalo en todo el sistema.
- Un solo bloque de layout, reutilizado en tres tarjetas.
- Ante una discontinuación, cualquier memoria JEDEC de 32K × 8 en DIP-28 entra en el mismo lugar.

**No dependemos de un fabricante sino de un estándar.**

### Lista

| Uso | Chip | Cant. | Comprar |
|---|---|---|---|
| ROM de microcódigo | AT28C256-15PU | 4 | 10 |
| Generador de inmediatos | AT28C256-15PU | 2 | 5 |
| Memoria de programa | AT28C256-15PU | 2 | 5 |
| RAM de datos | AS6C62256-55PCN | 2 | 6 |
| **Total** | | **10** | **26** |

Las cantidades a comprar contemplan el descarte por unidades defectuosas, que con proveedores de excedentes es esperable.

### Estado de mercado (agosto 2026)

| Parte | Estado |
|---|---|
| **AT28C256-15PU** | Activo. Microchip calificó un nuevo sitio de prueba en 2025 — señal de inversión sostenida. Stock en distribuidores oficiales. |
| **AS6C62256-55PCN** | Activo en DIP-28. |
| ~~AS6C62256A-70PIN~~ | **Obsoleto** — no pedir este código. |
| ~~KM62256, HM62256, TC55257~~ | Discontinuados (los originales de los ochenta). |

**Pedir por código completo, no por familia.** La obsolescencia en esta categoría es por variante, no por línea de producto.

### Alternativas si falta stock

Cualquier SRAM asíncrona de 32K × 8, DIP-28, 5 V, pinout JEDEC:

- AS6C62256 (cualquier sufijo de velocidad activo)
- CY62256 en versión DIP
- UM62256, IS62C256

Para la EEPROM: AT28C64B (8K, alcanza pero sin margen) o cualquier 28C256 de segunda fuente.

**Requisito de velocidad: 150 ns o mejor.** A 480 kHz el ciclo es de 2 µs, así que sobra margen. Cualquier velocidad comercial sirve.

---

## 2. Lógica 74HC

Riesgo bajo: la familia se sigue fabricando activamente en DIP por Texas Instruments, Nexperia y onsemi. Aun así conviene comprar en cantidad — son unos 130 chips.

| Chip | Función | Estimado | Comprar |
|---|---|---|---|
| 74HC00 | NAND cuádruple | 4 | 10 |
| 74HC04 | Inversor séxtuple | 6 | 15 |
| 74HC08 | AND cuádruple | 4 | 10 |
| 74HC14 | Schmitt trigger séxtuple | 3 | 8 |
| 74HC32 | OR cuádruple | 4 | 10 |
| 74HC74 | Flip-flop D doble | 6 | 12 |
| 74HC86 | XOR cuádruple | 8 | 15 |
| 74HC138 | Decodificador 3 a 8 | 6 | 12 |
| 74HC139 | Decodificador 2 a 4 doble | 3 | 8 |
| 74HC153 | Mux 4 a 1 doble | 4 | 8 |
| 74HC157 | Mux 2 a 1 cuádruple | 8 | 15 |
| 74HC161 | Contador binario 4 bits | 6 | 12 |
| 74HC163 | Contador sincrónico 4 bits | 4 | 8 |
| 74HC244 | Buffer óctuple tri-state | 12 | 20 |
| 74HC245 | Transceptor óctuple | 10 | 20 |
| 74HC283 | Sumador completo 4 bits | 8 | 15 |
| 74HC574 | Registro óctuple tri-state | 20 | 30 |
| 74HC595 | Registro de desplazamiento | 2 | 5 |

**Total aproximado: 118 en el diseño, ~230 a comprar.**

Las cantidades son estimadas: las tarjetas 3 a 10 todavía no están diseñadas y los números pueden moverse. Conviene comprar los de uso masivo (74HC574, 74HC245, 74HC244, 74HC283) con holgura y el resto ajustado.

**Verificar que sean 74HC y no 74HCT.** El HCT tiene umbrales de entrada compatibles con TTL y en un sistema íntegramente CMOS no aporta nada; mezclar familias complica el análisis de márgenes de ruido.

---

## 3. Componentes específicos ya definidos (Módulo 01)

| Componente | Cant. | Riesgo |
|---|---|---|
| TLC555 o LMC555 (DIP-8) | 2 | Bajo — versión CMOS del 555 |
| LM393 (DIP-8) | 1 | Muy bajo |
| TL431 (TO-92) | 1 | Muy bajo. Alternativas: LM431, KA431 |

**Sobre el TLC555:** si solo se consigue NE555, funciona, pero inyecta un pico de corriente en la alimentación que la versión CMOS no tiene, y da flancos más sucios. Vale la pena insistir con la variante CMOS.

**Pinout del TL431 en TO-92:** mirando la cara plana con las patas hacia abajo, el orden es **REF, A, K**. No es el orden intuitivo y no coincide con otros encapsulados — verificar contra la hoja de datos del fabricante recibido.

---

## 4. Periféricos

| Componente | Uso | Riesgo |
|---|---|---|
| LCD HD44780 16×2 o 20×4 | Salida v1 | Muy bajo — se fabrica masivamente |
| 74C922 o MM74C922 | Codificador de teclado hexadecimal | **Alto** — obsoleto |
| Teclado matricial 4×4 | Entrada v1 | Muy bajo |
| ADC0804 o ADC0808 | Joystick analógico (futuro) | Medio |
| Teclado PS/2 | Entrada v2 | **Alto** — ver nota |

### Nota sobre el 74C922

Está descontinuado hace tiempo y solo se consigue en excedentes. **Si no aparece, se construye con lógica discreta:** un 74HC161 para barrer las filas, un 74HC148 codificador de prioridad para las columnas, y un 74HC74 más un RC para el antirrebote. Unos cuatro chips.

Vale la pena decidir esto antes de diseñar el módulo 10.

### Nota sobre el teclado PS/2

La mayoría de los teclados USB modernos ya no incluyen el modo de compatibilidad, por lo que **los adaptadores pasivos USB a PS/2 no funcionan**. Hace falta un teclado con conector PS/2 nativo, que hoy se consigue mayormente usado.

Por eso el PS/2 es el módulo 11, opcional y posterior a la máquina funcionando.

---

## 5. Conectores y mecánica

| Componente | Cant. | Nota |
|---|---|---|
| Tira de pines macho 2×28 | 20 | Dos por tarjeta, 9 tarjetas + repuestos |
| Tira de pines hembra 2×28 | 20 | Para el backplane |
| Zócalo DIP-14 | 60 | |
| Zócalo DIP-16 | 40 | |
| Zócalo DIP-20 | 40 | |
| Zócalo DIP-28 (600 mil) | 15 | Memoria — **verificar el ancho** |
| Guías laterales para tarjeta | 9 | Perfil en U, riel o impresión 3D |

**Las tiras de pines se venden en tiras largas que se cortan a medida.** Comprar tiras de 2×40 y cortarlas sale más barato y flexible que buscar el largo exacto.

**Zócalos DIP-28 de 600 mil**, no de 300. La memoria usa el encapsulado ancho; los zócalos angostos son para otros chips y no entran.

---

## 6. Pasivos

| Componente | Cant. estimada |
|---|---|
| Capacitor cerámico 100 nF | **200** |
| Capacitor electrolítico 47 µF | 15 |
| Capacitor electrolítico 100 µF | 5 |
| Capacitor electrolítico 1000 µF | 2 |
| Resistores varios ¼ W | Surtido |
| Redes de resistores 8× 4k7 (bussed) | 10 |
| LED alta eficiencia | 50 |

**Los 100 nF son el componente más usado del proyecto:** uno por integrado, 130 como mínimo. Comprar 200 de una.

**Redes de resistores en lugar de individuales** donde haya pull-ups múltiples (líneas de bus, teclado). Un encapsulado de 8 resistores ocupa el lugar de uno solo.

**LED de alta eficiencia con resistores de 2k2 a 4k7**, no los 330 Ω habituales. Los LEDs indicadores son el mayor consumo de la máquina: 200 a 500 mA de un total de 0,7 a 1,0 A. Con LEDs eficientes se ven igual consumiendo un tercio.

---

## 7. Herramientas necesarias

| Herramienta | Necesidad | Nota |
|---|---|---|
| Programador de EEPROM | **Imprescindible** | Debe soportar 28C256 en DIP-28 paralelo |
| Osciloscopio 2 canales | **Imprescindible** | Ya disponible |
| Fuente regulable con límite de corriente | **Imprescindible** | Ya disponible |
| Multímetro | Imprescindible | |
| Analizador lógico 8-16 canales | Muy recomendable | Se vuelve difícil de reemplazar al llegar al bus de 16 bits |

**El programador de EEPROM no es opcional** y conviene conseguirlo temprano: además de programar el microcódigo, es la forma de verificar que los chips de memoria comprados en excedentes realmente funcionan. Escribir y verificar cada chip antes de soldarlo descarta los muertos.

Verificar que el modelo elegido soporte **28C256 paralelo en DIP-28**. Muchos programadores económicos priorizan memorias serie SPI/I²C y no manejan las paralelas.

---

## 8. Riesgo por comprar en marketplaces

Comprar en Amazon o similares da acceso a partes que los distribuidores oficiales ya marcaron obsoletas — una ventaja real para este proyecto. El costo es la trazabilidad.

La memoria y la lógica antigua están entre las categorías con más remarcado del mercado: chips recuperados de placas viejas, lijados y serigrafiados de nuevo. Fallan al mes, o solo a cierta temperatura, o de forma intermitente. **En una máquina de 130 chips, un integrado intermitente es de los problemas más difíciles de localizar.**

Tres medidas:

1. **Comprar el doble.** No como repuesto ante rotura, sino porque una fracción va a descartarse.
2. **Verificar cada chip de memoria antes de soldarlo**, escribiendo y releyendo con el programador.
3. **Zócalos en todo.** Ya está en el spec; acá cobra otro sentido.

Para los 74HC, la tarjeta de pruebas de bus (módulo 2) sirve como banco de pruebas improvisado.

---

## 9. Orden de compra sugerido

| Prioridad | Qué | Por qué |
|---|---|---|
| **1** | Memoria completa (26 chips) | Mayor riesgo de discontinuación; define las huellas |
| **1** | Programador de EEPROM | Necesario para verificar la memoria comprada |
| **2** | Componentes del módulo 01 | Para empezar a construir ya |
| **2** | Capacitores 100 nF (200) y zócalos | Uso masivo, nunca sobran |
| **3** | 74HC de uso masivo (574, 245, 244, 283) | Cantidades altas, precio unitario bajo |
| **4** | Resto de 74HC | Ajustar según diseño de cada tarjeta |
| **5** | Periféricos | Recién al llegar al módulo 10 |

**Antes de continuar el diseño de las tarjetas 3 a 10, confirmar disponibilidad real de los ítems de prioridad 1.** El resto del diseño depende de esas decisiones.
