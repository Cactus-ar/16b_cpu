// emu16 — Emulador a nivel ISA de la CPU16.
//
// Uso:  emu16 prog.bin [prog2.bin@0x8000 ...] [opciones]
//   archivo[@dir]     imagen de palabras LE cargada en el espacio de PROGRAMA
//   --trace           imprime cada instruccion ejecutada
//   --steps N         maximo de instrucciones (defecto 10 millones)
//   --irq-at N        activa IRQ_n al llegar al ciclo N (una vez)
//   --dump-data A N   al terminar, vuelca N palabras de datos desde A
//   --log ARCHIVO     traza, estado final y volcados van al archivo en vez
//                     de stdout; la consola del programa (puerto 0) queda
//                     en pantalla y los avisos en stderr
//
// Fiel al ISA v0.3 hasta en los detalles feos: R0 clavado en cero, BEQ/BNE
// pisan Z y C, JALR con rd=rs cae al fall-through (con aviso), SLT con signo,
// C = bit expulsado en corrimientos. Cuenta ciclos con las duraciones reales
// del microcodigo (01-spec §6.3) y modela la EEPROM ocupada tras SWP
// (03-spec §6): leer o ejecutar del banco en escritura es aviso fuerte.
//
// E/S provisoria hasta que exista 02-io-spec:
//   OUT puerto 0 -> caracter a stdout      IN puerto 1 -> caracter de stdin
//   OUT puerto 2 -> palabra en hex a stdout (util para tests)

#include "isa16.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

using namespace isa16;

static FILE* g_out = stdout;   // destino de traza/estado (--log lo redirige)

struct Machine {
    uint16_t R[8] = {0};
    uint16_t PC = RESET_ENTRY;
    bool Z = false, C = false, IE = false;
    bool halted = false;
    std::vector<uint16_t> prog = std::vector<uint16_t>(65536, 0);
    std::vector<uint16_t> data = std::vector<uint16_t>(65536, 0);
    std::vector<uint16_t> port = std::vector<uint16_t>(256, 0);
    unsigned long long cycles = 0, instrs = 0;

    // modelo de EEPROM ocupada (un ciclo de escritura por vez)
    int      eBusyBank = -1;
    unsigned long long eBusyUntil = 0;

    bool irqLine = false;    // nivel de IRQ_n (activo = true)
    int  warnJalr = 0, warnBusy = 0, warnData = 0;
};

static void setReg(Machine& m, unsigned r, uint16_t v) { if (r) m.R[r] = v; }  // R0 cableado a cero

static const char* FN_NAME[8] = {"ADD","SUB","AND","OR","XOR","SHL","SHR","SLT"};

static std::string dis(uint16_t w) {
    char b[64];
    unsigned rd = f_rd(w), rs = f_rs(w), rt = f_rt(w);
    switch (f_op(w)) {
    case OP_RTYPE: {
        unsigned fn = f_funct(w);
        if (fn == FN_SHL || fn == FN_SHR) snprintf(b, sizeof b, "%s R%u, R%u", FN_NAME[fn], rd, rs);
        else snprintf(b, sizeof b, "%s R%u, R%u, R%u", FN_NAME[fn], rd, rs, rt);
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

// banderas de la ALU sobre un resultado (Z siempre; C segun operacion)
static void flagsZ(Machine& m, uint16_t res, bool c) { m.Z = (res == 0); m.C = c; }

static void checkEepromRead(Machine& m, uint16_t addr, const char* what) {
    if (m.eBusyBank >= 0 && m.cycles < m.eBusyUntil && (int)bank(addr) == m.eBusyBank) {
        if (m.warnBusy++ < 8)
            fprintf(stderr, "[AVISO c=%llu] %s 0x%04X: banco %d de EEPROM en ciclo de escritura "
                            "— en hardware esto lee BASURA (03-spec §6)\n",
                    m.cycles, what, addr, m.eBusyBank);
    }
}

static bool step(Machine& m, bool trace) {
    // atencion de interrupcion: se evalua en T0, antes del fetch (01-spec §7)
    if (m.irqLine && m.IE) {
        setReg(m, 7, m.PC);
        m.PC = IRQ_VECTOR;
        m.IE = false;
        m.irqLine = false;         // el periferico emulado suelta la linea al ser atendido
        m.cycles += CYC_IRQ;
        if (trace) fprintf(g_out, "        --- IRQ atendida: R7=0x%04X, PC=0x%04X, IE=0\n", m.R[7], m.PC);
    }

    checkEepromRead(m, m.PC, "fetch en");
    uint16_t w = m.prog[m.PC];
    uint16_t pc1 = (uint16_t)(m.PC + 1);   // "PC+1": base de saltos y enlace
    if (trace) fprintf(g_out, "%6llu  %04X  %04X  %s\n", m.instrs, m.PC, w, dis(w).c_str());

    unsigned rd = f_rd(w), rs = f_rs(w);
    m.PC = pc1;
    m.instrs++;

    switch (f_op(w)) {
    case OP_RTYPE: {
        uint16_t a = m.R[rs], b2 = m.R[f_rt(w)];
        uint32_t r32; uint16_t res; bool c = false;
        switch (f_funct(w)) {
        case FN_ADD: r32 = (uint32_t)a + b2; res = (uint16_t)r32; c = r32 > 0xFFFF; break;
        case FN_SUB: res = (uint16_t)(a - b2); c = a >= b2; break;   // borrow invertido
        case FN_AND: res = a & b2; break;
        case FN_OR:  res = a | b2; break;
        case FN_XOR: res = a ^ b2; break;
        case FN_SHL: res = (uint16_t)(a << 1); c = (a & 0x8000) != 0; break;
        case FN_SHR: res = (uint16_t)(a >> 1); c = (a & 1) != 0; break;
        default:     res = ((int16_t)a < (int16_t)b2) ? 1 : 0; break; // SLT con signo
        }
        setReg(m, rd, res); flagsZ(m, res, c);
        m.cycles += CYC_RTYPE;
        break; }
    case OP_ADDI: {
        uint32_t r32 = (uint32_t)m.R[rs] + (uint16_t)sext6(f_imm6(w));
        uint16_t res = (uint16_t)r32;
        setReg(m, rd, res); flagsZ(m, res, r32 > 0xFFFF);
        m.cycles += CYC_ADDI;
        break; }
    case OP_LW: {
        uint16_t a = (uint16_t)(m.R[rs] + sext6(f_imm6(w)));
        if (a > DATA_POP_END && m.warnData++ < 8)
            fprintf(stderr, "[AVISO c=%llu] LW de 0x%04X: RAM no poblada, en hardware lee basura\n", m.cycles, a);
        setReg(m, rd, m.data[a]);
        m.cycles += CYC_LW;
        break; }
    case OP_SW: {
        uint16_t a = (uint16_t)(m.R[rs] + sext6(f_imm6(w)));
        if (a > DATA_POP_END && m.warnData++ < 8)
            fprintf(stderr, "[AVISO c=%llu] SW a 0x%04X: RAM no poblada, la escritura se pierde\n", m.cycles, a);
        else m.data[a] = m.R[rd];
        m.cycles += CYC_SW;
        break; }
    case OP_SWP: {
        uint16_t a = (uint16_t)(m.R[rs] + sext6(f_imm6(w)));
        if ((int)bank(a) == (int)bank((uint16_t)(m.PC - 1)))
            fprintf(stderr, "[AVISO c=%llu] SWP a 0x%04X: MISMO banco que el codigo en ejecucion "
                            "— prohibido por 03-spec §6, en hardware descarrila\n", m.cycles, a);
        if (m.eBusyBank == (int)bank(a) && m.cycles < m.eBusyUntil)
            fprintf(stderr, "[AVISO c=%llu] SWP a 0x%04X sin esperar los 10 ms: escritura corrupta\n", m.cycles, a);
        m.prog[a] = m.R[rd];
        m.eBusyBank = (int)bank(a);
        m.eBusyUntil = m.cycles + EEPROM_WC_CYCLES;
        m.cycles += CYC_SWP;
        break; }
    case OP_BEQ: case OP_BNE: {
        uint16_t a = m.R[rd], b2 = m.R[rs];
        uint16_t res = (uint16_t)(a - b2);
        flagsZ(m, res, a >= b2);               // el salto PISA las banderas (spec §6.3)
        bool taken = (f_op(w) == OP_BEQ) ? m.Z : !m.Z;
        if (taken) { m.PC = (uint16_t)(pc1 + sext6(f_imm6(w))); m.cycles += CYC_BR_TAKEN; }
        else m.cycles += CYC_BR_NTAKEN;
        break; }
    case OP_LUI: setReg(m, rd, (uint16_t)(f_imm8(w) << 8)); m.cycles += CYC_LUI; break;  // sin banderas
    case OP_ORI: {
        uint16_t res = (uint16_t)(m.R[rd] | f_imm8(w));
        setReg(m, rd, res); flagsZ(m, res, false);
        m.cycles += CYC_ORI;
        break; }
    case OP_JALR: {
        if (rd == rs && m.warnJalr++ < 4)
            fprintf(stderr, "[AVISO c=%llu] JALR con rd=rs: el enlace pisa la fuente, "
                            "salta al fall-through (el ensamblador lo rechaza)\n", m.cycles);
        setReg(m, rd, pc1);        // primero el enlace... (T1)
        m.PC = m.R[rs];            // ...despues el salto (T2) — reproduce el hazard
        m.cycles += CYC_JALR;
        break; }
    case OP_JMP: m.PC = (uint16_t)(pc1 + sext12(f_imm12(w))); m.cycles += CYC_JMP; break;
    case OP_IN: {
        unsigned p = f_imm8(w);
        uint16_t v = m.port[p];
        if (p == 1) { int c = getchar(); v = (c == EOF) ? 0 : (uint16_t)c; }   // consola provisoria
        setReg(m, rd, v);
        m.cycles += CYC_IN;
        break; }
    case OP_OUT: {
        unsigned p = f_imm8(w);
        uint16_t v = m.R[rd];
        m.port[p] = v;
        if (p == 0) { putchar(v & 0xFF); fflush(stdout); }                     // consola provisoria
        if (p == 2) { printf("%04X\n", v); }                                   // debug hex
        m.cycles += CYC_OUT;
        break; }
    case OP_HALT: m.halted = true; m.cycles += CYC_HALT; break;
    case OP_SYS:
        switch (f_funct(w)) {
        case SYS_EI:   m.IE = true;  break;
        case SYS_DI:   m.IE = false; break;
        case SYS_RETI: m.PC = m.R[7]; m.IE = true; break;
        default: fprintf(stderr, "[AVISO] subcampo de sistema reservado: %u\n", f_funct(w));
        }
        m.cycles += CYC_SYS;
        break;
    case OP_PREFIX:
        fprintf(stderr, "[ERROR c=%llu] opcode 1111 (prefijo reservado) en 0x%04X: "
                        "comportamiento indefinido — deteniendo\n", m.cycles, (uint16_t)(m.PC - 1));
        m.halted = true;
        break;
    }
    return !m.halted;
}

static bool loadBin(Machine& m, const std::string& spec) {
    std::string path = spec; uint32_t at = 0;
    size_t amp = spec.find('@');
    if (amp != std::string::npos) {
        path = spec.substr(0, amp);
        at = (uint32_t)strtoul(spec.substr(amp + 1).c_str(), nullptr, 0);
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "no puedo abrir %s\n", path.c_str()); return false; }
    std::vector<char> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (size_t i = 0; i + 1 < buf.size(); i += 2) {
        if (at + i / 2 > 0xFFFF) { fprintf(stderr, "%s no entra en 64K palabras\n", path.c_str()); return false; }
        m.prog[at + i / 2] = (uint16_t)((uint8_t)buf[i] | ((uint8_t)buf[i + 1] << 8));
    }
    printf("emu16: %s -> prog[0x%04X..0x%04X]\n", path.c_str(), at, (unsigned)(at + buf.size() / 2 - 1));
    return true;
}

int main(int argc, char** argv) {
    Machine m;
    bool trace = false;
    unsigned long long maxSteps = 10000000ULL, irqAt = 0;
    bool haveIrq = false;
    long dumpA = -1, dumpN = 0;
    bool loaded = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--trace") trace = true;
        else if (a == "--steps"   && i + 1 < argc) maxSteps = strtoull(argv[++i], nullptr, 0);
        else if (a == "--irq-at"  && i + 1 < argc) { irqAt = strtoull(argv[++i], nullptr, 0); haveIrq = true; }
        else if (a == "--dump-data" && i + 2 < argc) { dumpA = strtol(argv[++i], nullptr, 0); dumpN = strtol(argv[++i], nullptr, 0); }
        else if (a == "--log" && i + 1 < argc) {
            g_out = fopen(argv[++i], "w");
            if (!g_out) { fprintf(stderr, "no puedo crear %s\n", argv[i]); return 1; }
        }
        else if (a[0] == '-') { fprintf(stderr, "opcion desconocida %s\n", a.c_str()); return 1; }
        else { if (!loadBin(m, a)) return 1; loaded = true; }
    }
    if (!loaded) { fprintf(stderr, "uso: emu16 prog.bin [prog2.bin@0x8000] [--trace] [--steps N] [--irq-at C] [--dump-data A N] [--log ARCHIVO]\n"); return 1; }

    while (!m.halted && m.instrs < maxSteps) {
        if (haveIrq && m.cycles >= irqAt) { m.irqLine = true; haveIrq = false; }
        step(m, trace);
    }

    double secs = (double)m.cycles / (double)F_MAX_HZ;
    fprintf(g_out, "\n--- estado final ---\n");
    for (int i = 0; i < 8; i++) fprintf(g_out, "R%d=0x%04X%s", i, m.R[i], i == 3 ? "\n" : "  ");
    fprintf(g_out, "\nPC=0x%04X  Z=%d C=%d IE=%d  %s\n", m.PC, m.Z, m.C, m.IE, m.halted ? "HALT" : "(limite de pasos)");
    fprintf(g_out, "%llu instrucciones, %llu ciclos = %.3f s a 480 kHz\n", m.instrs, m.cycles, secs);
    if (dumpA >= 0) {
        fprintf(g_out, "--- datos [0x%04lX..0x%04lX] ---\n", dumpA, dumpA + dumpN - 1);
        for (long i = 0; i < dumpN; i++) {
            if (i % 8 == 0) fprintf(g_out, "%04lX:", dumpA + i);
            fprintf(g_out, " %04X", m.data[(uint16_t)(dumpA + i)]);
            if (i % 8 == 7 || i == dumpN - 1) printf("\n");
        }
    }
    if (g_out != stdout) fclose(g_out);
    return m.halted ? 0 : 2;
}
