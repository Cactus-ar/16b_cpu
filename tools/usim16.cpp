// usim16 — Simulador a NIVEL DE MICROCICLO de la CPU16.
//
// A diferencia de emu16 (que implementa el ISA), usim16 no sabe que es una
// instruccion: modela el datapath fisico — PC, IR, MAR, TMPA/TMPB, banco de
// registros, ALU, banderas, IE, latch de IRQ, contador T — y en cada ciclo
// de reloj hace lo que digan LAS CINCO ROMs DE CONTROL REALES generadas por
// ucode16. Si su traza coincide con la de emu16, el microcodigo implementa
// el ISA. Es el puente de verificacion antes del esquema en Digital.
//
// Uso:  usim16 prog.bin [prog2.bin@0x8000 ...] [opciones]
//   --ucode DIR       carpeta con ucode0..4.bin (defecto: build)
//   --trace           una linea por instruccion (formato identico a emu16)
//   --utrace          ademas, una linea por MICROCICLO con las senales
//   --steps N         maximo de instrucciones
//   --irq-at N        activa IRQ_n al llegar al ciclo N (una vez)
//   --dump-data A N   al terminar, vuelca N palabras de datos desde A
//   --log ARCHIVO     traza y estado final al archivo (consola en pantalla)
//
// La salida (traza y estado final) es byte a byte comparable con
// `emu16 --trace --log`: verificar es un diff.

#include "isa16.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

using namespace isa16;

static FILE* g_out = stdout;

// ------------------------------------------------ senales (mapa de ucode16)
// ROM0 (activas en bajo)
enum { B_PC_OUT=1, B_RA_OUT=2, B_RB_OUT=4, B_ALU_OUT=8, B_RAM_OUT=16, B_ROM_OUT=32, B_IMM_OUT=64, B_IO_OUT=128 };
// ROM1
enum { L_PC_LD=1, L_REG_WE=2, L_MAR_LD=4, L_IR_LD=8, L_RAM_WE_n=16, L_FLAGS_LD=32, L_IO_LD=64, L_TMPA_LD=128 };
// ROM2
enum { C_TMPB_LD=1, C_PROG_WE_n=2, C_ALU_SRC=64, C_PC_INC=128 };  // ALU_OP en bits 2..5
// ROM3
enum { D_PC_AOUT_n=1 };  // IMM_SEL bits 1..3, RSA_SEL bits 4..5, RSB_SEL bits 6..7
// ROM4
enum { E_RSW_SEL=1, E_IE_LD=2, E_IE_VAL=4, E_HALT=8, E_UEND=16 };

struct Usim {
    // estado fisico
    uint16_t PC = RESET_ENTRY, IR = 0, MAR = 0, TMPA = 0, TMPB = 0;
    uint16_t R[8] = {0};
    bool Z = false, C = false, IE = false;
    bool irqLine = false, irqLatch = false;
    unsigned T = 0;
    bool halted = false;
    std::vector<uint16_t> prog = std::vector<uint16_t>(65536, 0);
    std::vector<uint16_t> data = std::vector<uint16_t>(65536, 0);
    std::vector<uint16_t> port = std::vector<uint16_t>(256, 0);
    unsigned long long cycles = 0, instrs = 0;
    // ROMs de control
    std::vector<uint8_t> rom[5];
};

static bool loadUcode(Usim& u, const std::string& dir) {
    for (int r = 0; r < 5; r++) {
        char name[512]; snprintf(name, sizeof name, "%s/ucode%d.bin", dir.c_str(), r);
        std::ifstream f(name, std::ios::binary);
        if (!f) { fprintf(stderr, "no puedo abrir %s (correr ucode16 primero)\n", name); return false; }
        u.rom[r].assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (u.rom[r].size() != 16384) { fprintf(stderr, "%s: tamano invalido\n", name); return false; }
    }
    return true;
}

// un ciclo de reloj completo: leer ROMs -> buses combinacionales -> flanco
static void clockCycle(Usim& u, bool trace, bool utrace) {
    // direccion de la ROM de control (01-spec §10)
    unsigned op = f_op(u.IR), fn = f_funct(u.IR);
    unsigned addr = (op << 10) | (fn << 7) | (u.T << 4)
                  | ((unsigned)u.Z << 3) | ((unsigned)u.C << 2)
                  | ((unsigned)u.irqLatch << 1) | (unsigned)u.IE;
    uint8_t r0 = u.rom[0][addr], r1 = u.rom[1][addr], r2 = u.rom[2][addr],
            r3 = u.rom[3][addr], r4 = u.rom[4][addr];

    // --- traza a nivel instruccion: el fetch identifica el limite
    bool isFetch = (r1 & L_IR_LD) != 0;
    if (isFetch && trace)
        fprintf(g_out, "%6llu  %04X  %04X  %s\n", u.instrs, u.PC, u.prog[u.PC], dis(u.prog[u.PC]).c_str());

    // --- bus de direcciones: PC o MAR (arbitrado por PC_AOUT_n)
    uint16_t abus = (r3 & D_PC_AOUT_n) ? u.MAR : u.PC;   // bit=0 (activo) => PC

    // --- generador de inmediatos (interno de control)
    unsigned immSel = (r3 >> 1) & 7;
    uint16_t immVal = 0;
    switch (immSel) {
    case 0: immVal = (uint16_t)sext6(f_imm6(u.IR)); break;
    case 1: immVal = (uint16_t)(f_imm8(u.IR) << 8); break;
    case 2: immVal = (uint16_t)sext12(f_imm12(u.IR)); break;
    case 3: immVal = (uint16_t)f_imm8(u.IR); break;
    case 4: immVal = IRQ_VECTOR; break;
    }

    // --- seleccion de registros (muxes de la tarjeta de control)
    unsigned rsaSel = (r3 >> 4) & 3, rsbSel = (r3 >> 6) & 3;
    unsigned rsa = (rsaSel == 0) ? f_rs(u.IR) : (rsaSel == 1) ? f_rd(u.IR) : 7;
    unsigned rsb = (rsbSel == 0) ? f_rt(u.IR) : (rsbSel == 1) ? f_rd(u.IR) : f_rs(u.IR);
    unsigned rsw = (r4 & E_RSW_SEL) ? 7 : f_rd(u.IR);

    // --- ALU combinacional (A=TMPA, B=TMPB)
    unsigned aluOp = (r2 & C_ALU_SRC) ? fn : ((r2 >> 2) & 7);
    uint16_t a = u.TMPA, b = u.TMPB, aluRes; bool aluC = false;
    switch (aluOp) {
    case FN_ADD: { uint32_t s = (uint32_t)a + b; aluRes = (uint16_t)s; aluC = s > 0xFFFF; break; }
    case FN_SUB: aluRes = (uint16_t)(a - b); aluC = a >= b; break;
    case FN_AND: aluRes = a & b; break;
    case FN_OR:  aluRes = a | b; break;
    case FN_XOR: aluRes = a ^ b; break;
    case FN_SHL: aluRes = (uint16_t)(a << 1); aluC = (a & 0x8000) != 0; break;
    case FN_SHR: aluRes = (uint16_t)(a >> 1); aluC = (a & 1) != 0; break;
    default:     aluRes = ((int16_t)a < (int16_t)b) ? 1 : 0; break;
    }

    // --- bus de datos: exactamente un emisor (garantizado por ucode16)
    uint16_t dbus = 0xFFFF; const char* src = "?";
    if      (!(r0 & B_PC_OUT))  { dbus = u.PC;          src = "PC";  }
    else if (!(r0 & B_RA_OUT))  { dbus = u.R[rsa];      src = "RA";  }
    else if (!(r0 & B_RB_OUT))  { dbus = u.R[rsb];      src = "RB";  }
    else if (!(r0 & B_ALU_OUT)) { dbus = aluRes;        src = "ALU"; }
    else if (!(r0 & B_RAM_OUT)) { dbus = u.data[abus];  src = "RAM"; }
    else if (!(r0 & B_ROM_OUT)) { dbus = u.prog[abus];  src = "ROM"; }
    else if (!(r0 & B_IMM_OUT)) { dbus = immVal;        src = "IMM"; }
    else if (!(r0 & B_IO_OUT))  {                       src = "IO";
        unsigned p = abus & 0xFF;
        dbus = u.port[p];
        if (p == 1) { int ch = getchar(); dbus = (ch == EOF) ? 0 : (uint16_t)ch; }  // consola provisoria
    }

    if (utrace)
        fprintf(g_out, "          c=%-8llu T%u  bus=%s:%04X  addr=%04X  alu=%04X\n",
                u.cycles, u.T, src, dbus, abus, aluRes);

    // --- flanco de subida: cargas y estado siguiente
    if (r1 & L_TMPA_LD) u.TMPA = dbus;
    if (r2 & C_TMPB_LD) u.TMPB = dbus;
    if (r1 & L_MAR_LD)  u.MAR  = dbus;
    if (r1 & L_IR_LD)   { u.IR = dbus; u.instrs++; }
    if (r1 & L_REG_WE)  { if (rsw) u.R[rsw] = dbus; }          // R0 cableado a cero
    if (!(r1 & L_RAM_WE_n))  u.data[abus] = dbus;
    if (!(r2 & C_PROG_WE_n)) u.prog[abus] = dbus;
    if (r1 & L_FLAGS_LD) { u.Z = (aluRes == 0); u.C = aluC; }  // registro en la tarjeta ALU
    if (r1 & L_IO_LD) {
        unsigned p = abus & 0xFF;
        u.port[p] = dbus;
        if (p == 0) { putchar(dbus & 0xFF); fflush(stdout); }  // consola provisoria
        if (p == 2) { printf("%04X\n", dbus); }
    }
    if (r1 & L_PC_LD)      u.PC = dbus;
    else if (r2 & C_PC_INC) u.PC = (uint16_t)(u.PC + 1);
    if (r4 & E_IE_LD) u.IE = (r4 & E_IE_VAL) != 0;
    if (r4 & E_HALT)  u.halted = true;

    // la atencion Tb consume la solicitud (el periferico emulado la suelta)
    if (immSel == 4 && (r1 & L_PC_LD)) {
        u.irqLine = false;
        if (trace) fprintf(g_out, "        --- IRQ atendida: R7=0x%04X, PC=0x%04X, IE=0\n", u.R[7], IRQ_VECTOR);
    }

    u.cycles++;

    // contador de microciclo + latch de IRQ en el limite de instruccion
    if (r4 & E_UEND) { u.T = 0; u.irqLatch = u.irqLine; }
    else             u.T = (u.T + 1) & 7;
}

static bool loadBin(Usim& u, const std::string& spec) {
    std::string path = spec; uint32_t at = 0;
    size_t amp = spec.find('@');
    if (amp != std::string::npos) { path = spec.substr(0, amp); at = (uint32_t)strtoul(spec.substr(amp + 1).c_str(), nullptr, 0); }
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "no puedo abrir %s\n", path.c_str()); return false; }
    std::vector<char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (size_t i = 0; i + 1 < buf.size(); i += 2)
        u.prog[at + i / 2] = (uint16_t)((uint8_t)buf[i] | ((uint8_t)buf[i + 1] << 8));
    printf("usim16: %s -> prog[0x%04X..0x%04X]\n", path.c_str(), at, (unsigned)(at + buf.size() / 2 - 1));
    return true;
}

int main(int argc, char** argv) {
    Usim u;
    bool trace = false, utrace = false, loaded = false, haveIrq = false;
    unsigned long long maxSteps = 10000000ULL, irqAt = 0;
    long dumpA = -1, dumpN = 0;
    std::string udir = "build";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if      (a == "--trace")  trace = true;
        else if (a == "--utrace") { utrace = true; trace = true; }
        else if (a == "--ucode"   && i + 1 < argc) udir = argv[++i];
        else if (a == "--steps"   && i + 1 < argc) maxSteps = strtoull(argv[++i], nullptr, 0);
        else if (a == "--irq-at"  && i + 1 < argc) { irqAt = strtoull(argv[++i], nullptr, 0); haveIrq = true; }
        else if (a == "--dump-data" && i + 2 < argc) { dumpA = strtol(argv[++i], nullptr, 0); dumpN = strtol(argv[++i], nullptr, 0); }
        else if (a == "--log" && i + 1 < argc) {
            g_out = fopen(argv[++i], "w");
            if (!g_out) { fprintf(stderr, "no puedo crear %s\n", argv[i]); return 1; }
        }
        else if (a[0] == '-') { fprintf(stderr, "opcion desconocida %s\n", a.c_str()); return 1; }
        else { if (!loadBin(u, a)) return 1; loaded = true; }
    }
    if (!loaded) { fprintf(stderr, "uso: usim16 prog.bin [prog2.bin@0x8000] [--ucode DIR] [--trace] [--utrace] [--steps N] [--irq-at C] [--dump-data A N] [--log ARCHIVO]\n"); return 1; }
    if (!loadUcode(u, udir)) return 1;

    while (!u.halted && u.instrs < maxSteps) {
        if (haveIrq && u.cycles >= irqAt) { u.irqLine = true; if (u.T == 0) u.irqLatch = true; haveIrq = false; }
        clockCycle(u, trace, utrace);
    }

    double secs = (double)u.cycles / (double)F_MAX_HZ;
    fprintf(g_out, "\n--- estado final ---\n");
    for (int i = 0; i < 8; i++) fprintf(g_out, "R%d=0x%04X%s", i, u.R[i], i == 3 ? "\n" : "  ");
    fprintf(g_out, "\nPC=0x%04X  Z=%d C=%d IE=%d  %s\n", u.PC, u.Z, u.C, u.IE, u.halted ? "HALT" : "(limite de pasos)");
    fprintf(g_out, "%llu instrucciones, %llu ciclos = %.3f s a 480 kHz\n", u.instrs, u.cycles, secs);
    if (dumpA >= 0) {
        fprintf(g_out, "--- datos [0x%04lX..0x%04lX] ---\n", dumpA, dumpA + dumpN - 1);
        for (long i = 0; i < dumpN; i++) {
            if (i % 8 == 0) fprintf(g_out, "%04lX:", dumpA + i);
            fprintf(g_out, " %04X", u.data[(uint16_t)(dumpA + i)]);
            if (i % 8 == 7 || i == dumpN - 1) fprintf(g_out, "\n");
        }
    }
    if (g_out != stdout) fclose(g_out);
    return u.halted ? 0 : 2;
}
