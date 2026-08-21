// ucode16 — Microensamblador de la CPU16.
//
// Genera las imagenes de las CINCO EEPROM de control (AT28C256) a partir
// del microcodigo normativo de docs/01-isa-spec.md §6.3 y §7, con el mapa
// de salidas y direccion fijado en §10:
//
//   direccion (14 bits): A13..A0 = op[3:0] · funct[2:0] · T[2:0] · Z · C · IRQ · IE
//   (A14 a masa; cada imagen ocupa 16K de los 32K del chip)
//
// Salidas:  build/ucode0.bin .. build/ucode4.bin  (16384 bytes cada una)
//           build/ucode.lst                        (listado legible por estado)
//
// El valor almacenado es el NIVEL ELECTRICO del pin: las senales activas
// en bajo se asierten escribiendo 0. Un chip borrado (FF) deja todas las
// habilitaciones de bus inactivas — a proposito.
//
// Verificacion estatica sobre las 16384 direcciones:
//   - exactamente UNA habilitacion de bus activa en todo estado (la regla
//     de oro del bus-spec; en los estados sin transferencia se mantiene
//     ALU_OUT_n como bus keeper)
//   - toda secuencia termina con uEND antes de T7
//
// Supuesto de hardware documentado en §10: IRQ_n llega a la direccion
// MUESTREADO en el limite de instruccion (flip-flop en la tarjeta de
// control). Por eso (T=0|T=1, IRQ=1, IE=1) es SIEMPRE la secuencia de
// atencion, y los estados de instruccion normal con esos bits son
// inalcanzables.

#include "isa16.hpp"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

using namespace isa16;

// ------------------------------------------------------------- senales
struct SigDef { int rom, bit; bool activeLow; const char* name; };

enum Sig {
    // ROM0 — habilitaciones de bus (activas en bajo)
    S_PC_OUT, S_RA_OUT, S_RB_OUT, S_ALU_OUT, S_RAM_OUT, S_ROM_OUT, S_IMM_OUT, S_IO_OUT,
    // ROM1 — cargas
    S_PC_LD, S_REG_WE, S_MAR_LD, S_IR_LD, S_RAM_WE, S_FLAGS_LD, S_IO_LD, S_TMPA_LD,
    // ROM2
    S_TMPB_LD, S_PROG_WE, S_ALU_OP0, S_ALU_OP1, S_ALU_OP2, S_ALU_OP3, S_ALU_SRC, S_PC_INC,
    // ROM3
    S_PC_AOUT, S_IMM_SEL0, S_IMM_SEL1, S_IMM_SEL2, S_RSA_SEL0, S_RSA_SEL1, S_RSB_SEL0, S_RSB_SEL1,
    // ROM4
    S_RSW_SEL, S_IE_LD, S_IE_VAL, S_HALT, S_UEND,
    S_COUNT
};

static const SigDef SIG[S_COUNT] = {
    {0,0,true, "PC_OUT_n"}, {0,1,true, "RA_OUT_n"}, {0,2,true, "RB_OUT_n"}, {0,3,true, "ALU_OUT_n"},
    {0,4,true, "RAM_OUT_n"},{0,5,true, "ROM_OUT_n"},{0,6,true, "IMM_OUT_n"},{0,7,true, "IO_OUT_n"},
    {1,0,false,"PC_LD"},    {1,1,false,"REG_WE"},   {1,2,false,"MAR_LD"},   {1,3,false,"IR_LD"},
    {1,4,true, "RAM_WE_n"}, {1,5,false,"FLAGS_LD"}, {1,6,false,"IO_LD"},    {1,7,false,"TMPA_LD"},
    {2,0,false,"TMPB_LD"},  {2,1,true, "PROG_WE_n"},{2,2,false,"ALU_OP0"},  {2,3,false,"ALU_OP1"},
    {2,4,false,"ALU_OP2"},  {2,5,false,"ALU_OP3"},  {2,6,false,"ALU_SRC"},  {2,7,false,"PC_INC"},
    {3,0,true, "PC_AOUT_n"},{3,1,false,"IMM_SEL0"}, {3,2,false,"IMM_SEL1"}, {3,3,false,"IMM_SEL2"},
    {3,4,false,"RSA_SEL0"}, {3,5,false,"RSA_SEL1"}, {3,6,false,"RSB_SEL0"}, {3,7,false,"RSB_SEL1"},
    {4,0,false,"RSW_SEL"},  {4,1,false,"IE_LD"},    {4,2,false,"IE_VAL"},   {4,3,false,"HALT_CTL"},
    {4,4,false,"uEND"},
};

// campos multibit
enum ImmMode { IMM6S = 0, IMM8H = 1, IMM12S = 2, IMM8Z = 3, IMMVEC = 4 };
enum RsaSel  { RSA_RS = 0, RSA_RD = 1, RSA_R7 = 2 };
enum RsbSel  { RSB_RT = 0, RSB_RD = 1, RSB_RS = 2 };
enum RswSel  { RSW_RD = 0, RSW_R7 = 1 };

// ------------------------------------------------------- microinstruccion
struct MW {
    uint8_t r[5];
    MW() {
        // todo desasertado: activas-en-bajo a 1, activas-en-alto a 0
        for (auto& b : r) b = 0;
        for (auto& s : SIG) if (s.activeLow) r[s.rom] |= (uint8_t)(1 << s.bit);
        r[4] |= 0xE0;   // salidas libres de ROM4 en alto (como chip borrado)
    }
    void set(Sig s) {
        const SigDef& d = SIG[s];
        if (d.activeLow) r[d.rom] &= (uint8_t)~(1 << d.bit);
        else             r[d.rom] |= (uint8_t)(1 << d.bit);
    }
    void aluOp(unsigned fn) {
        if (fn & 1) set(S_ALU_OP0);
        if (fn & 2) set(S_ALU_OP1);
        if (fn & 4) set(S_ALU_OP2);
        // ALU_OP3 reservado: queda en 0
    }
    void imm(ImmMode m) {
        if (m & 1) set(S_IMM_SEL0);
        if (m & 2) set(S_IMM_SEL1);
        if (m & 4) set(S_IMM_SEL2);
    }
    void rsa(RsaSel s) { if (s & 1) set(S_RSA_SEL0); if (s & 2) set(S_RSA_SEL1); }
    void rsb(RsbSel s) { if (s & 1) set(S_RSB_SEL0); if (s & 2) set(S_RSB_SEL1); }
    void rsw(RswSel s) { if (s) set(S_RSW_SEL); }
    bool operator==(const MW& o) const { return memcmp(r, o.r, 5) == 0; }
};

// estados frecuentes
static MW idleEnd() {           // sin transferencia: ALU como bus keeper + fin
    MW m; m.set(S_ALU_OUT); m.set(S_UEND); return m;
}
static MW fetch() {             // T0: PC -> direcciones, ROM -> IR, PC+1
    MW m; m.set(S_PC_AOUT); m.set(S_ROM_OUT); m.set(S_IR_LD); m.set(S_PC_INC); return m;
}

// microcodigo de (op, funct, T, Z) — 01-isa-spec.md §6.3
// devuelve false si el estado es inalcanzable (se rellena con idleEnd)
static bool micro(unsigned op, unsigned fn, unsigned T, unsigned Z, MW& m) {
    m = MW();
    switch (op) {
    case OP_RTYPE:
        if (T == 1) { m.set(S_RA_OUT); m.set(S_TMPA_LD); m.rsa(RSA_RS); return true; }
        if (T == 2) { m.set(S_RB_OUT); m.set(S_TMPB_LD); m.rsb(RSB_RT); return true; }
        if (T == 3) { m.set(S_ALU_OUT); m.set(S_REG_WE); m.set(S_FLAGS_LD);
                      m.set(S_ALU_SRC); m.rsw(RSW_RD); m.set(S_UEND); return true; }
        return false;
    case OP_ADDI:
        if (T == 1) { m.set(S_RA_OUT); m.set(S_TMPA_LD); m.rsa(RSA_RS); return true; }
        if (T == 2) { m.set(S_IMM_OUT); m.set(S_TMPB_LD); m.imm(IMM6S); return true; }
        if (T == 3) { m.set(S_ALU_OUT); m.set(S_REG_WE); m.set(S_FLAGS_LD);
                      m.aluOp(FN_ADD); m.rsw(RSW_RD); m.set(S_UEND); return true; }
        return false;
    case OP_LW: case OP_SW: case OP_SWP:
        if (T == 1) { m.set(S_RA_OUT); m.set(S_TMPA_LD); m.rsa(RSA_RS); return true; }
        if (T == 2) { m.set(S_IMM_OUT); m.set(S_TMPB_LD); m.imm(IMM6S); return true; }
        if (T == 3) { m.set(S_ALU_OUT); m.set(S_MAR_LD); m.aluOp(FN_ADD); return true; }
        if (T == 4) {
            if (op == OP_LW) { m.set(S_RAM_OUT); m.set(S_REG_WE); m.rsw(RSW_RD); }
            else {
                m.set(S_RB_OUT); m.rsb(RSB_RD);
                m.set(op == OP_SW ? S_RAM_WE : S_PROG_WE);
            }
            m.set(S_UEND); return true;
        }
        return false;
    case OP_BEQ: case OP_BNE: {
        if (T == 1) { m.set(S_RA_OUT); m.set(S_TMPA_LD); m.rsa(RSA_RD); return true; }
        if (T == 2) { m.set(S_RB_OUT); m.set(S_TMPB_LD); m.rsb(RSB_RS); return true; }
        if (T == 3) { m.set(S_ALU_OUT); m.set(S_FLAGS_LD); m.aluOp(FN_SUB); return true; }
        bool taken = (op == OP_BEQ) ? (Z == 1) : (Z == 0);
        if (!taken) { if (T == 4) { m = idleEnd(); return true; } return false; }
        if (T == 4) { m.set(S_PC_OUT); m.set(S_TMPA_LD); return true; }
        if (T == 5) { m.set(S_IMM_OUT); m.set(S_TMPB_LD); m.imm(IMM6S); return true; }
        if (T == 6) { m.set(S_ALU_OUT); m.set(S_PC_LD); m.aluOp(FN_ADD); m.set(S_UEND); return true; }
        return false; }
    case OP_LUI:
        if (T == 1) { m.set(S_IMM_OUT); m.set(S_REG_WE); m.imm(IMM8H); m.rsw(RSW_RD); m.set(S_UEND); return true; }
        return false;
    case OP_ORI:
        if (T == 1) { m.set(S_RA_OUT); m.set(S_TMPA_LD); m.rsa(RSA_RD); return true; }
        if (T == 2) { m.set(S_IMM_OUT); m.set(S_TMPB_LD); m.imm(IMM8Z); return true; }
        if (T == 3) { m.set(S_ALU_OUT); m.set(S_REG_WE); m.set(S_FLAGS_LD);
                      m.aluOp(FN_OR); m.rsw(RSW_RD); m.set(S_UEND); return true; }
        return false;
    case OP_JALR:
        if (T == 1) { m.set(S_PC_OUT); m.set(S_REG_WE); m.rsw(RSW_RD); return true; }
        if (T == 2) { m.set(S_RA_OUT); m.set(S_PC_LD); m.rsa(RSA_RS); m.set(S_UEND); return true; }
        return false;
    case OP_JMP:
        if (T == 1) { m.set(S_PC_OUT); m.set(S_TMPA_LD); return true; }
        if (T == 2) { m.set(S_IMM_OUT); m.set(S_TMPB_LD); m.imm(IMM12S); return true; }
        if (T == 3) { m.set(S_ALU_OUT); m.set(S_PC_LD); m.aluOp(FN_ADD); m.set(S_UEND); return true; }
        return false;
    case OP_IN: case OP_OUT:
        if (T == 1) { m.set(S_IMM_OUT); m.set(S_MAR_LD); m.imm(IMM8Z); return true; }
        if (T == 2) {
            if (op == OP_IN) { m.set(S_IO_OUT); m.set(S_REG_WE); m.rsw(RSW_RD); }
            else             { m.set(S_RA_OUT); m.set(S_IO_LD);  m.rsa(RSA_RD); }
            m.set(S_UEND); return true;
        }
        return false;
    case OP_HALT:
        if (T == 1) { m = idleEnd(); m.set(S_HALT); return true; }
        return false;
    case OP_SYS:
        if (T != 1) return false;
        if (fn == SYS_EI)  { m = idleEnd(); m.set(S_IE_LD); m.set(S_IE_VAL); return true; }
        if (fn == SYS_DI)  { m = idleEnd(); m.set(S_IE_LD); return true; }
        if (fn == SYS_RETI){ m.set(S_RA_OUT); m.set(S_PC_LD); m.rsa(RSA_R7);
                             m.set(S_IE_LD); m.set(S_IE_VAL); m.set(S_UEND); return true; }
        m = idleEnd(); return true;      // subcampos reservados: no-op
    case OP_PREFIX:
        if (T == 1) { m = idleEnd(); m.set(S_HALT); return true; }   // indefinido: frena
        return false;
    }
    return false;
}

// atencion de interrupcion (§7): (T0|T1) con IRQ muestreado y IE
static MW attention(unsigned T) {
    MW m;
    if (T == 0) { m.set(S_PC_OUT); m.set(S_REG_WE); m.rsw(RSW_R7); }        // Ta: R7 <- PC
    else        { m.set(S_IMM_OUT); m.set(S_PC_LD); m.imm(IMMVEC);          // Tb: PC <- vector
                  m.set(S_IE_LD); m.set(S_UEND); }                          //     IE <- 0
    return m;
}

// ------------------------------------------------------------- listado
static std::string describe(const MW& m) {
    std::string s;
    for (int i = 0; i < S_COUNT; i++) {
        const SigDef& d = SIG[i];
        bool on = d.activeLow ? !(m.r[d.rom] & (1 << d.bit)) : !!(m.r[d.rom] & (1 << d.bit));
        if (on) { if (!s.empty()) s += " + "; s += d.name; }
    }
    return s.empty() ? "(nada)" : s;
}

int main() {
    std::vector<std::vector<uint8_t>> rom(5, std::vector<uint8_t>(16384));
    unsigned defined = 0, filled = 0;
    int busErrors = 0;

    for (unsigned addr = 0; addr < 16384; addr++) {
        unsigned op  = (addr >> 10) & 0xF;
        unsigned fn  = (addr >> 7) & 0x7;
        unsigned T   = (addr >> 4) & 0x7;
        unsigned Z   = (addr >> 3) & 1;
        unsigned irq = (addr >> 1) & 1;
        unsigned ie  =  addr       & 1;

        MW m;
        if (irq && ie && (T == 0 || T == 1)) { m = attention(T); defined++; }
        else if (T == 0)                     { m = fetch(); defined++; }
        else {
            bool ok = micro(op, fn, T, Z, m);
            if (ok) defined++; else { m = idleEnd(); filled++; }
        }

        // verificacion: exactamente un emisor de bus (ROM0 tiene 8 activas-bajo)
        int drivers = 0;
        for (int b = 0; b < 8; b++) if (!(m.r[0] & (1 << b))) drivers++;
        if (drivers != 1) {
            fprintf(stderr, "ERROR: %d emisores en addr 0x%04X (op=%X fn=%u T=%u Z=%u IRQ=%u IE=%u)\n",
                    drivers, addr, op, fn, T, Z, irq, ie);
            busErrors++;
        }
        for (int r = 0; r < 5; r++) rom[r][addr] = m.r[r];
    }

    if (busErrors) { fprintf(stderr, "%d violaciones del bus — no se generan imagenes\n", busErrors); return 1; }

    // imagenes
    for (int r = 0; r < 5; r++) {
        char name[64]; snprintf(name, sizeof name, "build/ucode%d.bin", r);
        std::ofstream f(name, std::ios::binary);
        f.write((const char*)rom[r].data(), (std::streamsize)rom[r].size());
    }

    // listado legible: un renglon por estado unico de cada instruccion
    std::ofstream ls("build/ucode.lst");
    ls << "; ucode16 — microcodigo CPU16 (01-isa-spec.md §6.3/§7/§10)\n"
       << "; direccion: A13..A0 = op[3:0]·funct[2:0]·T[2:0]·Z·C·IRQ·IE\n\n";
    static const char* OPN[16] = {"R-type","ADDI","LW","SW","BEQ","BNE","LUI","ORI",
                                  "JALR","JMP","IN","OUT","HALT","SYS","SWP","PREFIJO"};
    ls << "FETCH (T0, comun):\n  T0: " << describe(fetch()) << "\n\n";
    ls << "ATENCION DE IRQ (IRQ muestreado=1, IE=1):\n"
       << "  Ta: " << describe(attention(0)) << "\n"
       << "  Tb: " << describe(attention(1)) << "\n\n";
    for (unsigned op = 0; op < 16; op++) {
        unsigned fmax = (op == OP_SYS) ? 8u : 1u;
        for (unsigned fn = 0; fn < fmax; fn++) {
            ls << OPN[op];
            if (op == OP_SYS) ls << " fn=" << fn;
            ls << ":\n";
            for (unsigned Z = 0; Z < ((op == OP_BEQ || op == OP_BNE) ? 2u : 1u); Z++) {
                if (op == OP_BEQ || op == OP_BNE) ls << " [Z=" << Z << "]\n";
                for (unsigned T = 1; T < 8; T++) {
                    MW m; if (!micro(op, fn, T, Z, m)) break;
                    ls << "  T" << T << ": " << describe(m) << "\n";
                }
            }
            ls << "\n";
        }
    }

    printf("ucode16: 5 imagenes de 16384 bytes en build/ucode0..4.bin\n"
           "  estados definidos: %u   rellenos (inalcanzables): %u\n"
           "  invariante de un emisor: OK en las 16384 direcciones\n"
           "  listado: build/ucode.lst\n", defined, filled);
    return 0;
}
