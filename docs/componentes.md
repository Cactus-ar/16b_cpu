# Componentes — Lista maestra y riesgo de disponibilidad

**Versión 0.2** · **Última actualización: 2026-08-16** · Verificar disponibilidad **antes** de continuar el diseño

Estados posibles de cada ítem: **por verificar** (no se confirmó disponibilidad ni precio) → **confirmado** (hay stock localizable) → **recibido** (en mano, y verificado si es memoria) → **descartado** (se reemplazó por alternativa). El valor de este documento está en mantenerse al día: actualizar el estado y la fecha del encabezado con cada novedad.

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

| Uso | Chip | Cant. | Comprar | Estado |
|---|---|---|---|---|
| ROM de microcódigo | AT28C256-15PU | 4 | 10 | Por verificar |
| Generador de inmediatos | AT28C256-15PU | 2 | 5 | Por verificar |
| Memoria de programa | AT28C256-15PU | 2 | 5 | Por verificar |
| RAM de datos | AS6C62256-55PCN | 2 | 6 | Por verificar |
| **Banco de registros** | AS6C62256-55PCN | 4 | 8 | Por verificar |
| **Total** | | **14** | **34** | |

**Por qué el banco de registros lleva 4 SRAM:** el diseño usa dos copias idénticas que escriben siempre lo mismo, cada una con su propio bus de direcciones — dos puertos de lectura con memoria común (ver README). Con datos de 16 bits y chips de 8, cada copia son dos chips en paralelo: 2 copias × 2 chips = 4. *(Agregado el 16-ago-2026: faltaba en la lista original.)*

> Nota: el generador de inmediatos está resuelto en compuertas dentro de la tarjeta de control (`01-isa-spec.md` §5); sus EEPROM quedan en la lista como reserva deliberada (confirmado por el autor, ago 2026): si la zona gris se revisara, los chips ya están — y mientras tanto son repuesto universal, porque toda la memoria del proyecto usa el mismo AT28C256.

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

| Chip | Función | Estimado | Comprar | Estado |
|---|---|---|---|---|
| 74HC00 | NAND cuádruple | 4 | 10 | Por verificar |
| 74HC04 | Inversor séxtuple | 6 | 15 | Por verificar |
| 74HC08 | AND cuádruple | 4 | 10 | Por verificar |
| 74HC14 | Schmitt trigger séxtuple | 3 | 8 | Por verificar |
| 74HC32 | OR cuádruple | 4 | 10 | Por verificar |
| 74HC74 | Flip-flop D doble | 6 | 12 | Por verificar |
| 74HC86 | XOR cuádruple | 8 | 15 | Por verificar |
| 74HC138 | Decodificador 3 a 8 | 6 | 12 | Por verificar |
| 74HC139 | Decodificador 2 a 4 doble | 3 | 8 | Por verificar |
| 74HC148 | Codificador de prioridad 8 a 3 — teclado hexadecimal (§4) | 1 | 3 | Por verificar |
| 74HC153 | Mux 4 a 1 doble | 4 | 8 | Por verificar |
| 74HC157 | Mux 2 a 1 cuádruple | 8 | 15 | Por verificar |
| 74HC161 | Contador binario 4 bits | 6 | 12 | Por verificar |
| 74HC163 | Contador sincrónico 4 bits | 4 | 8 | Por verificar |
| 74HC244 | Buffer óctuple tri-state | 12 | 20 | Por verificar |
| 74HC245 | Transceptor óctuple | 10 | 20 | Por verificar |
| 74HC283 | Sumador completo 4 bits | 8 | 15 | Por verificar |
| 74HC574 | Registro óctuple tri-state | 20 | 30 | Por verificar |
| 74HC595 | Registro de desplazamiento | 2 | 5 | Por verificar |

**Total aproximado: 118 en el diseño, ~230 a comprar.**

Las cantidades son estimadas: las tarjetas 3 a 10 todavía no están diseñadas y los números pueden moverse. Conviene comprar los de uso masivo (74HC574, 74HC245, 74HC244, 74HC283) con holgura y el resto ajustado.

**Verificar que sean 74HC y no 74HCT.** El HCT tiene umbrales de entrada compatibles con TTL y en un sistema íntegramente CMOS no aporta nada; mezclar familias complica el análisis de márgenes de ruido.

---

## 3. Componentes específicos ya definidos (Módulo 01)

| Componente | Cant. | Riesgo | Estado |
|---|---|---|---|
| TLC555 o LMC555 (DIP-8) | 2 | Bajo — versión CMOS del 555 | Por verificar |
| LM393 (DIP-8) | 1 | Muy bajo | Por verificar |
| TL431 (TO-92) | 1 | Muy bajo. Alternativas: LM431, KA431 | Por verificar |
| Trimpot multivuelta 20 kΩ | 1 | Bajo — umbral de caída de tensión (R7) | Por verificar |
| Potenciómetro 1 MΩ lineal | 1 | Bajo — ajuste de frecuencia | Por verificar |
| Conmutador rotativo 3 posiciones, 1 polo | 1 | **Medio** — no está en cualquier casa de electrónica; verificar antes del pedido | Por verificar |
| Llave conmutadora SPDT | 1 | Muy bajo — selección astable/manual | Por verificar |
| Pulsador momentáneo | 2 | Muy bajo — paso y reset | Por verificar |

*(Electromecánicos agregados el 16-ago-2026: estaban en la BOM de `modulos/01-reloj.md` pero faltaban en esta lista maestra, que es de donde se arma el pedido.)*

**Sobre el TLC555:** si solo se consigue NE555, funciona, pero inyecta un pico de corriente en la alimentación que la versión CMOS no tiene, y da flancos más sucios. Vale la pena insistir con la variante CMOS.

**Pinout del TL431 en TO-92:** mirando la cara plana con las patas hacia abajo, el orden es **REF, A, K**. No es el orden intuitivo y no coincide con otros encapsulados — verificar contra la hoja de datos del fabricante recibido.

---

## 4. Periféricos

| Componente | Uso | Riesgo | Estado |
|---|---|---|---|
| LCD HD44780 16×2 o 20×4 | Salida v1 | Muy bajo — se fabrica masivamente | Por verificar |
| ~~74C922 o MM74C922~~ | Codificador de teclado hexadecimal | — | **Descartado** (ago 2026) — ver nota |
| Teclado matricial 4×4 | Entrada v1 | Muy bajo | Por verificar |
| ADC0804 o ADC0808 | Joystick analógico (futuro) | Medio | Por verificar |
| Teclado PS/2 | Entrada v2 | **Alto** — ver nota | Por verificar |

### Nota sobre el 74C922 — descartado

**Decisión del autor (16-ago-2026): el codificador de teclado se construye con lógica discreta, definitivamente.** Un 74HC161 para barrer las filas, un 74HC148 codificador de prioridad para las columnas, y un 74HC74 más un RC para el antirrebote — unos cuatro chips, todos activos y ya en la lista de la sección 2.

Razones: elimina la única dependencia de un chip obsoleto de excedentes del pedido, y es más fiel al espíritu del proyecto — la lógica queda visible en vez de dentro de un encapsulado discontinuado. El módulo 10 se diseña directamente así.

### Nota sobre el teclado PS/2

La mayoría de los teclados USB modernos ya no incluyen el modo de compatibilidad, por lo que **los adaptadores pasivos USB a PS/2 no funcionan**. Hace falta un teclado con conector PS/2 nativo, que hoy se consigue mayormente usado.

Por eso el PS/2 es el módulo 11, opcional y posterior a la máquina funcionando.

---

## 5. Conectores y mecánica

| Componente | Cant. | Nota | Estado |
|---|---|---|---|
| Tira de pines macho 2×28 | 20 | Dos por tarjeta, 9 tarjetas + repuestos | Por verificar |
| Tira de pines hembra 2×28 | 20 | Para el backplane | Por verificar |
| Zócalo DIP-8 | 10 | Módulo 01: 2× 555 + LM393 | Por verificar |
| Zócalo DIP-14 | 60 | | Por verificar |
| Zócalo DIP-16 | 40 | | Por verificar |
| Zócalo DIP-20 | 40 | | Por verificar |
| Zócalo DIP-28 (600 mil) | 20 | Memoria (14 en diseño + repuestos) — **verificar el ancho** | Por verificar |
| Guías laterales para tarjeta | 9 | Perfil en U, riel o impresión 3D | Por verificar |

**Las tiras de pines se venden en tiras largas que se cortan a medida.** Comprar tiras de 2×40 y cortarlas sale más barato y flexible que buscar el largo exacto.

**Zócalos DIP-28 de 600 mil**, no de 300. La memoria usa el encapsulado ancho; los zócalos angostos son para otros chips y no entran.

---

## 6. Pasivos

| Componente | Cant. estimada | Estado |
|---|---|---|
| Capacitor cerámico 100 nF | **200** | Por verificar |
| Capacitor electrolítico 47 µF | 15 | Por verificar |
| Capacitor electrolítico 100 µF | 5 | Por verificar |
| Capacitor electrolítico 1000 µF | 2 | Por verificar |
| Resistores varios ¼ W | Surtido | Por verificar |
| Redes de resistores 8× 4k7 (bussed) | 10 | Por verificar |
| Diodo 1N4148 | 20 | Por verificar |
| LED alta eficiencia | 50 | Por verificar |

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
| **1** | Memoria completa (34 chips) | Mayor riesgo de discontinuación; define las huellas |
| **1** | Programador de EEPROM | Necesario para verificar la memoria comprada |
| **2** | Componentes del módulo 01 | Para empezar a construir ya |
| **2** | Capacitores 100 nF (200) y zócalos | Uso masivo, nunca sobran |
| **3** | 74HC de uso masivo (574, 245, 244, 283) | Cantidades altas, precio unitario bajo |
| **4** | Resto de 74HC | Ajustar según diseño de cada tarjeta |
| **5** | Periféricos | Recién al llegar al módulo 10 |

**Antes de continuar el diseño de las tarjetas 3 a 10, confirmar disponibilidad real de los ítems de prioridad 1.** El resto del diseño depende de esas decisiones.
