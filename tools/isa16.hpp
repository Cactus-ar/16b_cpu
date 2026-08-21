// isa16.hpp — Tabla única del ISA CPU16.
//
// ÚNICA definición del encoding para todas las herramientas (asm16, emu16 y,
// a futuro, el microensamblador). Derivada de los documentos normativos:
//   docs/01-isa-spec.md   (v0.3 — instrucciones, encoding, microcódigo)
//   docs/03-memoria-spec.md (v0.1 — espacios, direcciones fijas, ABI)
// Si algo difiere del spec, el spec manda y esto es un bug.

#pragma once
#include <cstdint>

namespace isa16 {

// ---------------------------------------------------------------- opcodes §3
enum Op : unsigned {
    OP_RTYPE = 0x0,  // rd <- rs funct rt
    OP_ADDI  = 0x1,  // rd <- rs + sext(imm6)
    OP_LW    = 0x2,  // rd <- data[rs + sext(imm6)]
    OP_SW    = 0x3,  // data[rs + sext(imm6)] <- rd
    OP_BEQ   = 0x4,  // si rd == rs: PC <- PC+1 + sext(imm6)
    OP_BNE   = 0x5,  // si rd != rs: idem
    OP_LUI   = 0x6,  // rd <- imm8 << 8
    OP_ORI   = 0x7,  // rd <- rd | imm8
    OP_JALR  = 0x8,  // rd <- PC+1; PC <- rs   (restriccion: rd != rs)
    OP_JMP   = 0x9,  // PC <- PC+1 + sext(imm12)
    OP_IN    = 0xA,  // rd <- port[imm8]
    OP_OUT   = 0xB,  // port[imm8] <- rd
    OP_HALT  = 0xC,  // detiene el reloj
    OP_SYS   = 0xD,  // EI / DI / RETI segun bits [2:0]
    OP_SWP   = 0xE,  // prog[rs + sext(imm6)] <- rd  (solo banco no ejecutante)
    OP_PREFIX= 0xF,  // reservado — ejecutarlo es comportamiento indefinido
};

// ------------------------------------------------------------------ funct §4
enum Funct : unsigned {
    FN_ADD = 0, FN_SUB = 1, FN_AND = 2, FN_OR  = 3,
    FN_XOR = 4, FN_SHL = 5, FN_SHR = 6, FN_SLT = 7,   // SLT: CON signo
};

// ------------------------------------------------------- subcampo sistema §7
enum Sys : unsigned { SYS_EI = 0, SYS_DI = 1, SYS_RETI = 2 };

// ------------------------------------------------------- extraccion de campos
inline unsigned f_op   (uint16_t w) { return (w >> 12) & 0xF; }
inline unsigned f_rd   (uint16_t w) { return (w >>  9) & 0x7; }
inline unsigned f_rs   (uint16_t w) { return (w >>  6) & 0x7; }
inline unsigned f_rt   (uint16_t w) { return (w >>  3) & 0x7; }
inline unsigned f_funct(uint16_t w) { return  w        & 0x7; }
inline unsigned f_imm6 (uint16_t w) { return  w        & 0x3F; }
inline unsigned f_imm8 (uint16_t w) { return  w        & 0xFF; }
inline unsigned f_imm12(uint16_t w) { return  w        & 0xFFF; }

// ------------------------------------------------------- extension de signo
inline int16_t sext6 (unsigned v) { return (int16_t)((v & 0x20)  ? (v | 0xFFC0) : v); }
inline int16_t sext12(unsigned v) { return (int16_t)((v & 0x800) ? (v | 0xF000) : v); }

// -------------------------------------------------------------- construccion
inline uint16_t encR(unsigned rd, unsigned rs, unsigned rt, unsigned fn) {
    return (uint16_t)((OP_RTYPE << 12) | (rd << 9) | (rs << 6) | (rt << 3) | fn);
}
inline uint16_t encI(unsigned op, unsigned rd, unsigned rs, unsigned imm6) {
    return (uint16_t)((op << 12) | (rd << 9) | (rs << 6) | (imm6 & 0x3F));
}
inline uint16_t encL(unsigned op, unsigned rd, unsigned imm8) {
    return (uint16_t)((op << 12) | (rd << 9) | (imm8 & 0xFF));   // bit 8 en cero
}
inline uint16_t encJ(unsigned op, unsigned imm12) {
    return (uint16_t)((op << 12) | (imm12 & 0xFFF));
}
inline uint16_t encSys(unsigned sub) { return (uint16_t)((OP_SYS << 12) | (sub & 0x7)); }

// ----------------------------------------------- mapa de memoria (03-spec §3)
constexpr uint16_t RESET_ENTRY  = 0x0000;
constexpr uint16_t IRQ_VECTOR   = 0x0004;
constexpr uint16_t MONITOR_BASE = 0x0008;
constexpr uint16_t USER_BASE    = 0x8000;  // banco alto (A15=1) — escribible con SWP
constexpr uint16_t MON_VARS     = 0x0000;  // espacio de datos
constexpr uint16_t USER_DATA    = 0x0100;
constexpr uint16_t STACK_TOP    = 0x7FFF;
constexpr uint16_t DATA_POP_END = 0x7FFF;  // ultima palabra de RAM poblada

// banco de programa de una direccion (A15)
inline unsigned bank(uint16_t addr) { return addr >> 15; }

// ------------------------------------------- temporizacion (01-spec §6.3)
// Ciclos de reloj por instruccion, T0 (fetch) incluido.
// BEQ/BNE: la ROM de control lee Z recien en T4 (un ciclo despues de
// FLAGS_LD), asi que el caso no tomado tambien paga ese ciclo de decision.
constexpr unsigned CYC_RTYPE     = 4;
constexpr unsigned CYC_ADDI      = 4;
constexpr unsigned CYC_LW        = 5;
constexpr unsigned CYC_SW        = 5;
constexpr unsigned CYC_SWP       = 5;
constexpr unsigned CYC_BR_NTAKEN = 5;
constexpr unsigned CYC_BR_TAKEN  = 7;   // la secuencia mas larga de la maquina
constexpr unsigned CYC_LUI       = 2;
constexpr unsigned CYC_ORI       = 4;
constexpr unsigned CYC_JALR      = 3;
constexpr unsigned CYC_JMP       = 4;
constexpr unsigned CYC_IN        = 3;
constexpr unsigned CYC_OUT       = 3;
constexpr unsigned CYC_HALT      = 2;
constexpr unsigned CYC_SYS       = 2;
constexpr unsigned CYC_IRQ       = 2;   // Ta + Tb (el chequeo no paga ciclo: T0 se convierte en Ta)

// Frecuencia maxima real del reloj (modulo 01) y ciclo de escritura EEPROM.
constexpr unsigned long F_MAX_HZ        = 480000;
constexpr unsigned long EEPROM_WC_CYCLES = 4800;  // 10 ms a 480 kHz (03-spec §6)

} // namespace isa16

// ------------------------------------------------------------- desensamblador
// (fuera del namespace de constantes por comodidad; compartido por emu16/usim16)
#include <cstdio>
#include <string>
namespace isa16 {

inline const char* functName(unsigned fn) {
    static const char* N[8] = {"ADD","SUB","AND","OR","XOR","SHL","SHR","SLT"};
    return N[fn & 7];
}

inline std::string dis(uint16_t w) {
    char b[64];
    unsigned rd = f_rd(w), rs = f_rs(w), rt = f_rt(w);
    switch (f_op(w)) {
    case OP_RTYPE: {
        unsigned fn = f_funct(w);
        if (fn == FN_SHL || fn == FN_SHR) snprintf(b, sizeof b, "%s R%u, R%u", functName(fn), rd, rs);
        else snprintf(b, sizeof b, "%s R%u, R%u, R%u", functName(fn), rd, rs, rt);
        break; }
    case OP_ADDI: snprintf(b, sizeof b, "ADDI R%u, R%u, %d", rd, rs, sext6(f_imm6(w))); break;
    case OP_LW:   snprintf(b, sizeof b, "LW R%u, %d(R%u)",  rd, sext6(f_imm6(w)), rs); break;
    case OP_SW:   snprintf(b, sizeof b, "SW R%u, %d(R%u)",  rd, sext6(f_imm6(w)), rs); break;
    case OP_SWP:  snprintf(b, sizeof b, "SWP R%u, %d(R%u)", rd, sext6(f_imm6(w)), rs); break;
    case OP_BEQ:  snprintf(b, sizeof b, "BEQ R%u, R%u, %+d", rd, rs, sext6(f_imm6(w))); break;
    case OP_BNE:  snprintf(b, sizeof b, "BNE R%u, R%u, %+d", rd, rs, sext6(f_imm6(w))); break;
    case OP_LUI:  snprintf(b, sizeof b, "LUI R%u, 0x%02X", rd, f_imm8(w)); break;
    case OP_ORI:  snprintf(b, sizeof b, "ORI R%u, 0x%02X", rd, f_imm8(w)); break;
    case OP_JALR: snprintf(b, sizeof b, "JALR R%u, R%u", rd, rs); break;
    case OP_JMP:  snprintf(b, sizeof b, "JMP %+d", sext12(f_imm12(w))); break;
    case OP_IN:   snprintf(b, sizeof b, "IN R%u, 0x%02X",  rd, f_imm8(w)); break;
    case OP_OUT:  snprintf(b, sizeof b, "OUT R%u, 0x%02X", rd, f_imm8(w)); break;
    case OP_HALT: snprintf(b, sizeof b, "HALT"); break;
    case OP_SYS:
        switch (f_funct(w)) {
        case SYS_EI:   snprintf(b, sizeof b, "EI");   break;
        case SYS_DI:   snprintf(b, sizeof b, "DI");   break;
        case SYS_RETI: snprintf(b, sizeof b, "RETI"); break;
        default:       snprintf(b, sizeof b, "SYS? %u", f_funct(w));
        }
        break;
    case OP_PREFIX: snprintf(b, sizeof b, "PREFIJO! (indefinido)"); break;
    default: snprintf(b, sizeof b, "???");
    }
    return b;
}

} // namespace isa16
