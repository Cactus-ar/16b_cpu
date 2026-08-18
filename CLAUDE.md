# CLAUDE.md

Contexto del proyecto para asistentes de IA. Leer antes de modificar nada.

---

## Qué es este proyecto

CPU de 16 bits construida físicamente con lógica discreta 74HC. Sin microcontroladores, sin FPGA, sin ningún procesador escondido: cada compuerta y cada registro es un chip real.

**Objetivo declarado por el autor:** reproducir la experiencia de una computadora doméstica de 1982 (referencia: TI-99/4A), salvando distancias. No se busca compatibilidad ni salida de video. Se busca poder cargar, guardar y ejecutar programas pequeños — incluido algo como un Pong rudimentario — en una máquina construida con tecnología comprensible.

**Criterio de diseño explícito del autor:** el costo y el esfuerzo físico **no son restricción**. La variable a minimizar es el riesgo de tener que rehacer trabajo. Ante la duda entre "suficiente" y "con margen", elegir margen.

**Requisito transversal:** el diseño debe ser expandible. Está previsto agregar más adelante BIOS, joystick analógico por conversión A/D, LCD gráfico y almacenamiento en flash. Ninguna decisión actual debe cerrar esas puertas.

---

## Qué cuenta como "espíritu del proyecto"

Esta distinción se discutió explícitamente y guía las decisiones de implementación:

**Aceptable:** memoria pasiva. ROM de microcódigo, tablas de búsqueda en EEPROM, generadores por tabla. Es cómo se construían las minicomputadoras de los setenta y la propia TI-99/4A tenía su BASIC en ROM. Una EEPROM no decide nada: es una tabla de verdad congelada.

**No aceptable:** cualquier cosa que ejecute o decida. Microcontroladores, FPGA, CPLD, lógica programable. Eso sería simular una CPU en vez de construirla.

**Zona gris a discutir con el autor:** reemplazar lógica combinacional simple y comprensible por tablas solo para ahorrar chips. Ejemplo concreto ya señalado: el generador de inmediatos con EEPROM ahorra siete chips pero oculta cuatro modos de extensión de signo que son fáciles de ver en compuertas.

---

## Estado actual

| | |
|---|---|
| Módulo 01 (reloj) | Esquemático terminado y verificado, ERC limpio |
| Resto de las tarjetas | En diseño, sin esquemático |
| Spec del ISA | **Reconstruido y validado por completo** (v0.3, ago 2026) — normativo, incluido el microcódigo |
| Construcción física | No iniciada — esperando componentes |
| Ensamblador, emulador, microcódigo | No iniciados |

**Nada está fabricado.** Todas las decisiones de diseño siguen siendo reversibles, pero eso deja de ser cierto en cuanto se mande a hacer el backplane.

---

## Documentos normativos

Se leen en este orden. Los tres primeros gobiernan todo lo demás.

| Documento | Contenido |
|---|---|
| `docs/00-bus-spec.md` | Pinout del backplane, formato físico, señales de control |
| `docs/01-isa-spec.md` | Instrucciones, encoding, microcódigo de cada una |
| `docs/02-io-spec.md` | **No existe todavía** — puertos de E/S y periféricos |
| `docs/03-memoria-spec.md` | Mapa de memoria: tres espacios, direcciones fijas, ABI, carga con `SWP` |

Ante conflicto entre documentos, **`01-isa-spec.md` tiene precedencia**: fue validado completo por el autor (ago 2026) y de él deriva el microcódigo.

---

## Convenciones de hardware — no negociables

Estas reglas rigen las nueve tarjetas por igual:

- **Familia 74HC a 5 V.** No mezclar con 74HCT.
- **Ninguna entrada CMOS flotante.** Toda entrada sin usar va a VCC o GND con cable real. Las salidas sin usar sí pueden quedar al aire, marcadas con símbolo de no conectado.
- **Un capacitor de 100 nF por integrado**, entre VCC y GND, pegado al chip.
- **Sufijo `_n` para señales activas en bajo.**
- **Exactamente un emisor por vez en el bus.** Toda habilitación de salida es activa en bajo y desactivada en reset.
- **A PCB va solo lo que ya funcionó al menos una vez.**
- **Toda la memoria usa la huella JEDEC de 28 pines, 32K × 8.** SRAM y EEPROM comparten pinout: un solo tipo de zócalo, un solo bloque de layout, y cualquier reemplazo JEDEC entra.

---

## Convenciones de documentación

- **Español**, con términos técnicos en inglés cuando corresponde (bus, tri-state, backplane, fetch, datapath).
- Los documentos explican **por qué**, no solo qué. El registro de razones es lo que hace replicable el proyecto.
- Cada módulo lleva su procedimiento de prueba escrito, con criterios verificables.
- No prometer estado que no existe. Si algo está pendiente, decirlo.

---

## Decisiones tomadas que conviene no revisar sin motivo

Cada una tiene razones discutidas en detalle. Cambiarlas es posible pero debe ser deliberado.

| Decisión | Razón breve |
|---|---|
| Multiciclo microprogramado, no single-cycle | Single-cycle sobre backplane no es un bus, es un arnés de cables |
| Bus compartido tri-state | Elimina los multiplexores de 16 bits; la complejidad se muda a una ROM, que es gratis en chips |
| ISA propio, no RV32I | RV32I duplicaría el ancho y llevaría de ~130 a ~220 chips |
| Banderas Z y C propias | RISC-V no las tiene porque optimiza para pipeline; esta máquina no tiene pipeline |
| Tiras de pines 2×28, no DIN 41612 | Disponibilidad en Argentina |
| Direccionamiento por palabra, sin acceso a bytes | Elimina toda la lógica de selección de carriles |
| E/S con espacio de puertos propio | El número de puerto viaja por el bus de direcciones vía MAR: cero señales nuevas |
| IR fusionado en la tarjeta de control | `RSA`/`RSB`/`RSW` derivan del IR y las emite el control; separarlos costaría nueve pines |
| Interrupciones **sí** en la v1 | El costo real es un flip-flop y dos bits más en la ROM; postergarlas obliga a reprogramar cuatro EEPROM |

---

## Riesgos abiertos

**Disponibilidad de componentes.** Es el riesgo principal. La compra se hace mayormente por marketplaces, lo que da acceso a partes obsoletas pero sin trazabilidad. Ver `docs/componentes.md`. Regla: comprar el doble y verificar cada chip de memoria con el programador antes de soldarlo.

**Tamaño físico de la tarjeta.** El formato es 100 × 160 mm y la tarjeta de control lleva ~18 integrados. **Conviene hacer el layout de esa tarjeta antes que el de ninguna otra**: si no entrara, habría que cambiar el formato de las nueve.

**El 74C922** — riesgo cerrado (ago 2026): se descartó definitivamente; el codificador de teclado del módulo 10 se hace con cuatro chips de lógica común (74HC161 + 74HC148 + 74HC74 + RC). Ver `docs/componentes.md` §4.

**El Pong** exige refresco fluido, que un LCD de caracteres no da. Si se mantiene como objetivo, la tarjeta de E/S necesita repensarse hacia matriz de LEDs o LCD gráfico.

---

## Cómo trabajar en este repo

1. Ante un cambio de diseño, actualizar **primero** el documento normativo y después el resto.
2. Los `.kicad_sch` son texto plano y se pueden verificar por script: pines sin conectar, unidades faltantes, valores inconsistentes. Ya se hizo manualmente; conviene automatizarlo en `tools/`.
3. `kicad/comun/conectores.kicad_sch` es la fuente de verdad de la hoja de conectores. Cada proyecto tiene su copia porque KiCad la necesita al lado, pero los cambios se hacen en `comun/` y se recopian.
4. No inventar valores de componentes ni números de parte. Si un dato no está verificado, decirlo.
