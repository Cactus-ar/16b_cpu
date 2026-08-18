# CPU de 16 bits en lógica discreta

Diseño y construcción física de una CPU de 16 bits usando exclusivamente lógica discreta de la familia **74HC**. Sin microcontroladores, sin FPGA, sin ningún procesador escondido adentro: cada compuerta, cada registro y cada bit del datapath está hecho de chips que se pueden tocar.

El objetivo es una máquina capaz de ejecutar programas reales —bucles, condicionales, acceso a memoria, entrada y salida— construida de forma que cada decisión de diseño sea visible y verificable con un osciloscopio.

> **Estado actual:** en construcción. El esquemático del módulo de reloj está terminado y verificado; el resto de las tarjetas está en diseño. Ver [Estado del proyecto](#estado-del-proyecto).

---

## Qué es esto

Una CPU **multiciclo microprogramada** de 16 bits, repartida en nueve tarjetas que se conectan a un **backplane** pasivo. Los módulos se comunican por un bus compartido de 16 bits con salidas **tri-state**: en cada ciclo, exactamente un módulo escribe en el bus y el resto escucha.

Números aproximados del sistema completo:

| | |
|---|---|
| Integrados | ~130 |
| Ancho de datos | 16 bits |
| Registros | 8 de propósito general (R0 cableado a cero) |
| Frecuencia | 1 Hz – 480 kHz, ajustable |
| Consumo | 0,7 – 1,0 A a 5 V |
| Tarjetas | 9 (más una opcional) |
| Formato de tarjeta | 100 × 160 mm |

### Por qué multiciclo y no single-cycle

Un diseño *single-cycle* es más simple de entender en el pizarrón, pero su datapath es punto a punto: la salida de la ALU va únicamente a la memoria, cada operando tiene su propio camino. Montado sobre un backplane, eso no es un bus — es un arnés de cables con slots no intercambiables.

El bus compartido con tri-state cambia la ecuación: los multiplexores de 16 bits desaparecen y se reemplazan por buffers 74HC245. Se paga con una unidad de control más compleja, pero esa unidad **es una ROM**, o sea que es gratis en cantidad de chips. La cuenta cierra claramente a favor del bus.

---

## Arquitectura

```
                    ┌─────────────────────────────┐
                    │      BUS DE DATOS (16)      │
                    └──┬────┬────┬────┬────┬───┬──┘
                       │    │    │    │    │   │
   ┌────────┐  ┌───────┴┐ ┌─┴──┐ ┌┴───┐ ┌┴──┐ ┌┴─────┐
   │ Reloj  │  │Registros│ │ALU │ │RAM │ │ROM│ │ E/S  │
   └───┬────┘  └────────┘ └────┘ └─┬──┘ └─┬─┘ └──────┘
       │ CLK                       │      │
       ▼                     ┌─────┴──────┴────┐
   ┌────────────────┐        │ BUS DE DIRECC.  │
   │ Unidad de      │        └─────────────────┘
   │ control (µcode)│───── señales de control ─────►
   └────────────────┘
```

**Harvard modificada:** memoria de programa y de datos separadas, pero ambas direccionadas por el mismo bus. El PC maneja el bus de direcciones durante el *fetch*; el MAR lo maneja el resto del tiempo, arbitrados por una sola señal.

**Sin banderas globales visibles al bus salvo Z y C**, que viajan de la ALU a la unidad de control para resolver saltos condicionales.

---

## El ISA en una página

Instrucciones de **ancho fijo de 16 bits**, arquitectura **load/store**. Cuatro formatos, deliberadamente pocos: cada formato adicional es lógica de decodificación que hay que dibujar a mano.

| Formato | Campos (bits) |
|---|---|
| **R** | op(4) · rd(3) · rs(3) · rt(3) · funct(3) |
| **I** | op(4) · rd(3) · rs(3) · imm(6) |
| **L** | op(4) · rd(3) · —(1) · imm8(8) |
| **J** | op(4) · imm(12) |

### Instrucciones

| Opcode | Instrucción | Operación |
|---|---|---|
| `0000` | R-type | Según `funct`: ADD, SUB, AND, OR, XOR, SHL, SHR, SLT |
| `0001` | `ADDI rd, rs, imm` | rd ← rs + imm |
| `0010` | `LW rd, imm(rs)` | rd ← mem[rs + imm] |
| `0011` | `SW rd, imm(rs)` | mem[rs + imm] ← rd |
| `0100` | `BEQ rd, rs, imm` | Salta si rd = rs |
| `0101` | `BNE rd, rs, imm` | Salta si rd ≠ rs |
| `0110` | `LUI rd, imm` | rd ← imm8 << 8 |
| `0111` | `ORI rd, imm` | rd ← rd \| imm8 |
| `1000` | `JALR rd, rs` | rd ← PC+1; PC ← rs |
| `1001` | `JMP imm` | PC ← PC + imm |
| `1010` | `IN rd, puerto` | rd ← puerto |
| `1011` | `OUT rs, puerto` | puerto ← rs |
| `1100` | `HALT` | Detiene el reloj |
| `1101` | Sistema | EI / DI / RETI (interrupciones) |
| `1110` | `SWP rd, imm(rs)` | prog[rs + imm] ← rd (carga de programas) |
| `1111` | — | Prefijo de expansión (reservado) |

Los 3 bits de `funct` van **directo** al selector de la ALU, sin traducción intermedia. Eso elimina un decodificador entero.

**R0 está cableado a cero**, lo que habilita pseudo-instrucciones que el ensamblador traduce y el hardware no necesita implementar: `MOV rd, rs` es `ADDI rd, rs, 0`, y `NOP` es `ADDI R0, R0, 0`.

Detalle completo del encoding, semántica y microcódigo: [`docs/01-isa-spec.md`](docs/01-isa-spec.md).

---

## Los módulos

Son **nueve tarjetas**: el registro de instrucción se fusionó con la unidad de control (las señales de selección de registros derivan del IR y las emite el control — separarlos costaría nueve pines de backplane). El número 4 queda vacante para no invalidar referencias existentes.

| # | Módulo | Función | Estado |
|---|---|---|---|
| 1 | **Reloj** | Oscilador, paso a paso, halt sincrónico, reset, brownout | Esquemático listo |
| 2 | **Pruebas de bus** | 16 LEDs + 16 switches para inspeccionar y forzar el bus | Pendiente |
| 3 | Contador de programa | PC con incremento y carga | Pendiente |
| 4 | — | *Fusionado con el 9* | |
| 5 | Banco de registros | 8 × 16 bits, doble puerto de lectura | Pendiente |
| 6 | ALU | Aritmética, lógica, corrimientos, banderas | Pendiente |
| 7 | Memoria de datos + MAR | RAM y registro de direcciones | Pendiente |
| 8 | Memoria de programa | ROM/EEPROM con carga desde el bus | Pendiente |
| 9 | Unidad de control | IR, secuenciador y ROM de microcódigo | Pendiente |
| 10 | E/S | LCD HD44780 + teclado hexadecimal | Pendiente |
| 11 | Teclado PS/2 | Opcional, posterior a la máquina funcionando | Pendiente |

### Sobre el módulo 2

La tarjeta de pruebas de bus **no es opcional y va segunda**. Cuando construyas el contador de programa no vas a tener forma de saber si funciona: nada le da datos y nada muestra lo que saca. Esta tarjeta es el instrumento que se usa en cada depuración del resto del proyecto.

### Sobre el banco de registros

Construirlo con flip-flops requeriría unos treinta integrados. La alternativa: **dos SRAM idénticas escribiendo siempre lo mismo en ambas**. Como cada una tiene su propio bus de direcciones, se obtienen dos puertos de lectura independientes con dos chips en lugar de treinta.

---

## Convenciones del proyecto

Estas reglas rigen las nueve tarjetas y no se negocian por tarjeta.

**Familia 74HC a 5 V.** Consume mucho menos que la 74LS —relevante con 130 chips— y se consigue mejor.

**Ninguna entrada CMOS flotante, nunca.** Toda entrada sin usar va a VCC o a GND con un cable real. Una entrada al aire oscila, calienta el chip y mete ruido en toda la placa. Las salidas sin usar sí pueden quedar al aire.

**Un capacitor de 100 nF por integrado**, entre VCC y GND, físicamente pegado al chip. Es el consejo que todo el mundo saltea y la causa número uno de circuitos que "andan a veces".

**Sufijo `_n` para señales activas en bajo.** Con cuarenta señales de control cruzando la máquina, esta convención evita horas de confusión.

**Exactamente un emisor por vez en el bus.** Toda habilitación de salida es activa en bajo y, en reset, debe estar desactivada. Si el registro de microinstrucción arrancara en ceros, los ocho módulos manejarían el bus simultáneamente.

**A PCB va solo lo que ya funcionó al menos una vez.** Lo que tenga lógica sin validar se arma primero en protoboard o placa universal.

---

## Herramientas

| Herramienta | Uso |
|---|---|
| [KiCad](https://www.kicad.org) 10.x | Esquemáticos y PCB |
| [Digital](https://github.com/hneemann/Digital) | Simulación lógica de la arquitectura |
| Osciloscopio (2 canales) | Verificación de flancos y temporización |
| Fuente regulable con límite de corriente | Alimentación y protección al energizar |

**Sobre el límite de corriente:** antes de energizar una tarjeta nueva se ajusta al consumo esperado más 50 mA. Si hay un corto o un chip invertido, la fuente entra en corriente constante y la tensión se hunde en lugar de destruir el integrado.

---

## Estructura del repositorio

```
├── docs/
│   ├── 00-bus-spec.md             ← Especificación normativa del bus
│   ├── 01-isa-spec.md             ← ISA y microcódigo (precedencia máxima)
│   ├── componentes.md             ← Lista maestra y disponibilidad
│   └── modulos/
│       ├── 01-reloj.md            ← Diseño y pruebas del reloj
│       └── 01-reloj-kicad.md      ← Netlist e implementación en KiCad
├── kicad/
│   ├── comun/
│   │   └── conectores.kicad_sch   ← Hoja compartida, fuente de verdad
│   └── Modulo_01_Reloj/
├── tools/
│   ├── isa16.hpp                  ← Tabla única del ISA (C++)
│   ├── asm16.cpp / emu16.cpp      ← Ensamblador y emulador
│   ├── cpu16-tools.sln            ← Solución de Visual Studio
│   └── tests/                     ← Programas de prueba
└── PENDIENTES.md                  ← Discrepancias y huecos abiertos
```

Los documentos normativos se leen en orden: `00-bus-spec.md` (pinout, formato físico, señales), `01-isa-spec.md` (instrucciones y microcódigo — ante conflicto, este manda), `02-io-spec.md` (E/S — pendiente de escribir).

`kicad/comun/conectores.kicad_sch` se copia sin modificar a cada proyecto nuevo. Contiene los dos conectores con etiquetas globales, ya verificado. Los cambios se hacen en `comun/` y se recopian.

---

## Cómo replicarlo

1. Leer `docs/00-bus-spec.md` completo antes de tocar nada.
2. Verificar disponibilidad local de: tiras de pines 2×28, TLC555, TL431 en TO-92.
3. Construir el **módulo 1 (reloj)** en protoboard y validarlo con el procedimiento de prueba del documento.
4. Construir el **módulo 2 (pruebas de bus)**.
5. Recién entonces, el resto de las tarjetas.

El orden importa: los módulos 1 y 2 son las herramientas con las que se depuran todos los demás.

---

## Escala del proyecto

Es un proyecto de meses, no de semanas. Una CPU de 8 bits en lógica discreta ronda los 40 integrados; esta llega a unos 130 por el ancho de 16 bits y el banco de registros con doble puerto.

Es perfectamente factible, pero conviene saberlo desde el principio: la mayoría de los proyectos de este tipo no fracasan por dificultad técnica sino por el tiempo que pasa hasta ver algo funcionando. De ahí el orden de construcción, pensado para tener piezas verificables desde el primer mes.
