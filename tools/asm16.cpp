// asm16 — Ensamblador de dos pasadas para la CPU16.
//
// Uso:  asm16 programa.s [-o base]
// Genera: base.bin     imagen de palabras de 16 bits, little-endian
//         base.lo.bin  byte bajo por palabra  (EEPROM par bajo)
//         base.hi.bin  byte alto por palabra  (EEPROM par alto)
//         base.lst     listado direccion/palabra/fuente
//
// Sintaxis: una instruccion por linea; comentarios con ';'
//   etiqueta:  ADD  R1, R2, R3
//              ADDI R1, R0, -5
//              LW   R2, 3(R1)
//              BEQ  R1, R2, etiqueta
//              LI   R1, 0x1234        ; pseudo -> LUI+ORI (2 palabras)
//   .org expr / .word expr,expr,... / .equ NOMBRE, expr
//   numeros: decimal, 0x hex, 'c' caracter; expresiones con + y -
//
// Validaciones normativas (01-isa-spec.md): rangos de inmediatos,
// JALR con rd = rs rechazado, sin mnemonico para el prefijo 1111.

#include "isa16.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

using namespace isa16;
using std::string;

// ------------------------------------------------------------------ utilidades
static string g_file;
static int    g_line;
static int    g_errors = 0;

static void err(const string& msg) {
    fprintf(stderr, "%s:%d: error: %s\n", g_file.c_str(), g_line, msg.c_str());
    g_errors++;
}

static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Mayusculas, preservando el interior de literales de caracter 'c'
static string fold(const string& s) {
    string r; r.reserve(s.size());
    bool q = false;
    for (char c : s) {
        if (c == '\'') { q = !q; r += c; }
        else r += q ? c : (char)toupper((unsigned char)c);
    }
    return r;
}

static std::vector<string> splitCommas(const string& s) {
    std::vector<string> out; string cur;
    for (char c : s) {
        if (c == ',') { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

// ------------------------------------------------------------------- simbolos
static std::map<string, long> g_sym = {
    {"RESET_ENTRY",  RESET_ENTRY},  {"IRQ_VECTOR", IRQ_VECTOR},
    {"MONITOR_BASE", MONITOR_BASE}, {"USER_BASE",  USER_BASE},
    {"MON_VARS",     MON_VARS},     {"USER_DATA",  USER_DATA},
    {"STACK_TOP",    STACK_TOP},
};

// ------------------------------------------------------------------ expresiones
// gramatica: term (('+'|'-') term)*   ;  term = numero | 'c' | simbolo
static bool evalTerm(const string& t, long& out, bool pass2) {
    if (t.empty()) return false;
    if (t[0] == '\'') {                    // literal de caracter
        if (t.size() == 3 && t[2] == '\'') { out = (unsigned char)t[1]; return true; }
        return false;
    }
    char* end = nullptr;
    if (isdigit((unsigned char)t[0])) {
        out = strtol(t.c_str(), &end, 0);  // decimal o 0x
        return *end == '\0';
    }
    auto it = g_sym.find(t);
    if (it != g_sym.end()) { out = it->second; return true; }
    if (pass2) err("simbolo no definido: '" + t + "'");
    out = 0;
    return !pass2;                          // en pasada 1 puede no existir aun
}

static bool evalExpr(const string& e, long& out, bool pass2) {
    string s = trim(e);
    if (s.empty()) return false;
    long acc = 0; int sign = +1; string term; bool any = false;
    size_t i = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    else if (s[0] == '+') { i = 1; }
    for (; ; i++) {
        bool end = (i >= s.size());
        char c = end ? '\0' : s[i];
        if (end || c == '+' || c == '-') {
            string t = trim(term);
            if (t.empty()) return false;
            long v;
            if (!evalTerm(t, v, pass2)) { if (pass2) err("expresion invalida: '" + t + "'"); return false; }
            acc += sign * v; any = true; term.clear();
            if (end) break;
            sign = (c == '-') ? -1 : +1;
        } else term += c;
    }
    out = acc;
    return any;
}

// ------------------------------------------------------------------ operandos
static bool parseReg(const string& t, unsigned& r) {
    string s = trim(t);
    if (s.size() == 2 && s[0] == 'R' && s[1] >= '0' && s[1] <= '7') { r = s[1] - '0'; return true; }
    return false;
}

// "expr(Rn)" para LW/SW/SWP
static bool parseMem(const string& t, long& off, unsigned& base, bool pass2) {
    size_t p = t.find('(');
    if (p == string::npos || t.back() != ')') return false;
    string offs = trim(t.substr(0, p));
    if (offs.empty()) offs = "0";
    if (!evalExpr(offs, off, pass2)) return false;
    return parseReg(t.substr(p + 1, t.size() - p - 2), base);
}

static bool checkRange(long v, long lo, long hi, const char* what) {
    if (v < lo || v > hi) {
        char b[128];
        snprintf(b, sizeof b, "%s fuera de rango: %ld (permitido %ld..%ld)", what, v, lo, hi);
        err(b);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- ensamblado
struct OutWord { uint16_t addr, word; int line; string src; };

static std::map<uint16_t, OutWord> g_image;
static uint16_t g_addr = 0;
static bool     g_pass2 = false;

static void emit(uint16_t w, const string& src) {
    if (g_pass2) {
        if (g_image.count(g_addr)) err("direccion 0x" + [](uint16_t a){char b[8];snprintf(b,8,"%04X",a);return string(b);}(g_addr) + " emitida dos veces");
        g_image[g_addr] = { g_addr, w, g_line, src };
    }
    g_addr++;
}

// tamano en palabras de una linea ya separada en mnemonico+operandos
static int sizeOf(const string& mn, const std::vector<string>& ops) {
    if (mn == "LI" || mn == "PUSH" || mn == "POP") return 2;
    if (mn == ".WORD") return (int)ops.size();
    return 1;
}

static void assembleLine(const string& mn, const std::vector<string>& ops, const string& src) {
    unsigned rd, rs, rt;
    long v;
    auto needOps = [&](size_t n) {
        if (ops.size() != n) { err(mn + ": espera " + std::to_string(n) + " operandos"); return false; }
        return true;
    };

    // ---- directivas
    if (mn == ".ORG") {
        if (!needOps(1) || !evalExpr(ops[0], v, g_pass2)) return;
        if (!checkRange(v, 0, 0xFFFF, ".org")) return;
        if ((uint16_t)v < g_addr && g_pass2) err(".org retrocede");
        g_addr = (uint16_t)v;
        return;
    }
    if (mn == ".WORD") {
        for (auto& o : ops) {
            if (!evalExpr(o, v, g_pass2)) { g_addr++; continue; }
            if (!checkRange(v, -32768, 65535, ".word")) { g_addr++; continue; }
            emit((uint16_t)(v & 0xFFFF), src);
        }
        return;
    }

    // ---- R-type
    auto rtype3 = [&](unsigned fn) {
        if (!needOps(3) || !parseReg(ops[0], rd) || !parseReg(ops[1], rs) || !parseReg(ops[2], rt))
            { err(mn + ": operandos invalidos (rd, rs, rt)"); return; }
        emit(encR(rd, rs, rt, fn), src);
    };
    auto rtype2 = [&](unsigned fn) {   // SHL/SHR: rt se ignora y va en cero
        if (!needOps(2) || !parseReg(ops[0], rd) || !parseReg(ops[1], rs))
            { err(mn + ": operandos invalidos (rd, rs)"); return; }
        emit(encR(rd, rs, 0, fn), src);
    };
    if (mn == "ADD") return rtype3(FN_ADD);
    if (mn == "SUB") return rtype3(FN_SUB);
    if (mn == "AND") return rtype3(FN_AND);
    if (mn == "OR")  return rtype3(FN_OR);
    if (mn == "XOR") return rtype3(FN_XOR);
    if (mn == "SLT") return rtype3(FN_SLT);
    if (mn == "SHL") return rtype2(FN_SHL);
    if (mn == "SHR") return rtype2(FN_SHR);

    // ---- inmediato de 6 bits
    if (mn == "ADDI") {
        if (!needOps(3) || !parseReg(ops[0], rd) || !parseReg(ops[1], rs) || !evalExpr(ops[2], v, g_pass2))
            { err("ADDI: operandos invalidos (rd, rs, imm)"); return; }
        if (!checkRange(v, -32, 31, "imm6")) return;
        emit(encI(OP_ADDI, rd, rs, (unsigned)v), src);
        return;
    }
    if (mn == "LW" || mn == "SW" || mn == "SWP") {
        long off; unsigned base;
        if (!needOps(2) || !parseReg(ops[0], rd) || !parseMem(ops[1], off, base, g_pass2))
            { err(mn + ": operandos invalidos (rd, imm(rs))"); return; }
        if (!checkRange(off, -32, 31, "imm6")) return;
        unsigned op = (mn == "LW") ? OP_LW : (mn == "SW") ? OP_SW : OP_SWP;
        emit(encI(op, rd, base, (unsigned)off), src);
        return;
    }
    if (mn == "BEQ" || mn == "BNE") {
        if (!needOps(3) || !parseReg(ops[0], rd) || !parseReg(ops[1], rs) || !evalExpr(ops[2], v, g_pass2))
            { err(mn + ": operandos invalidos (rd, rs, destino)"); return; }
        long off = v - (long)(g_addr + 1);          // base: PC+1 (spec §3)
        if (g_pass2 && !checkRange(off, -32, 31, "desplazamiento de salto")) return;
        emit(encI(mn == "BEQ" ? OP_BEQ : OP_BNE, rd, rs, (unsigned)off), src);
        return;
    }

    // ---- inmediato de 8 bits
    auto ltype = [&](unsigned op) {
        if (!needOps(2) || !parseReg(ops[0], rd) || !evalExpr(ops[1], v, g_pass2))
            { err(mn + ": operandos invalidos (rd, imm8)"); return; }
        if (!checkRange(v, 0, 255, "imm8")) return;
        emit(encL(op, rd, (unsigned)v), src);
    };
    if (mn == "LUI") return ltype(OP_LUI);
    if (mn == "ORI") return ltype(OP_ORI);
    if (mn == "IN")  return ltype(OP_IN);
    if (mn == "OUT") return ltype(OP_OUT);

    // ---- saltos
    if (mn == "JALR") {
        if (!needOps(2) || !parseReg(ops[0], rd) || !parseReg(ops[1], rs))
            { err("JALR: operandos invalidos (rd, rs)"); return; }
        if (rd == rs) { err("JALR con rd = rs: prohibido por el ISA (el enlace pisa la fuente)"); return; }
        emit((uint16_t)((OP_JALR << 12) | (rd << 9) | (rs << 6)), src);
        return;
    }
    if (mn == "JMP") {
        if (!needOps(1) || !evalExpr(ops[0], v, g_pass2)) { err("JMP: destino invalido"); return; }
        long off = v - (long)(g_addr + 1);
        if (g_pass2 && !checkRange(off, -2048, 2047, "desplazamiento de JMP")) return;
        emit(encJ(OP_JMP, (unsigned)off), src);
        return;
    }

    // ---- sistema
    if (mn == "HALT") { if (needOps(0)) emit((uint16_t)(OP_HALT << 12), src); return; }
    if (mn == "EI")   { if (needOps(0)) emit(encSys(SYS_EI), src);   return; }
    if (mn == "DI")   { if (needOps(0)) emit(encSys(SYS_DI), src);   return; }
    if (mn == "RETI") { if (needOps(0)) emit(encSys(SYS_RETI), src); return; }

    // ---- pseudo-instrucciones (01-spec §8, 03-spec §5)
    if (mn == "NOP") { if (needOps(0)) emit(encI(OP_ADDI, 0, 0, 0), src); return; }
    if (mn == "MOV") {
        if (!needOps(2) || !parseReg(ops[0], rd) || !parseReg(ops[1], rs))
            { err("MOV: operandos invalidos (rd, rs)"); return; }
        emit(encI(OP_ADDI, rd, rs, 0), src);
        return;
    }
    if (mn == "CLR") {
        if (!needOps(1) || !parseReg(ops[0], rd)) { err("CLR: operando invalido (rd)"); return; }
        emit(encI(OP_ADDI, rd, 0, 0), src);
        return;
    }
    if (mn == "JR") {
        if (!needOps(1) || !parseReg(ops[0], rs)) { err("JR: operando invalido (rs)"); return; }
        if (rs == 0) { err("JR R0: seria JALR R0,R0, prohibido"); return; }
        emit((uint16_t)((OP_JALR << 12) | (0 << 9) | (rs << 6)), src);
        return;
    }
    if (mn == "LI") {           // rd <- const16 : LUI hi8 + ORI lo8
        if (!needOps(2) || !parseReg(ops[0], rd) || !evalExpr(ops[1], v, g_pass2))
            { err("LI: operandos invalidos (rd, const16)"); return; }
        if (!checkRange(v, -32768, 65535, "const16")) return;
        uint16_t u = (uint16_t)(v & 0xFFFF);
        emit(encL(OP_LUI, rd, u >> 8), src + "   ; LUI");
        emit(encL(OP_ORI, rd, u & 0xFF), src + "   ; ORI");
        return;
    }
    if (mn == "PUSH") {         // ADDI R6,R6,-1 ; SW rd,0(R6)
        if (!needOps(1) || !parseReg(ops[0], rd)) { err("PUSH: operando invalido (rd)"); return; }
        emit(encI(OP_ADDI, 6, 6, (unsigned)(-1 & 0x3F)), src + "   ; SP--");
        emit(encI(OP_SW, rd, 6, 0), src + "   ; guardar");
        return;
    }
    if (mn == "POP") {          // LW rd,0(R6) ; ADDI R6,R6,1
        if (!needOps(1) || !parseReg(ops[0], rd)) { err("POP: operando invalido (rd)"); return; }
        emit(encI(OP_LW, rd, 6, 0), src + "   ; leer");
        emit(encI(OP_ADDI, 6, 6, 1), src + "   ; SP++");
        return;
    }

    err("mnemonico desconocido: '" + mn + "'");
}

// ------------------------------------------------------------------------ main
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "uso: asm16 programa.s [-o base]\n"); return 1; }
    g_file = argv[1];
    string base = g_file;
    size_t dot = base.find_last_of('.');
    if (dot != string::npos) base = base.substr(0, dot);
    for (int i = 2; i + 1 < argc; i++)
        if (string(argv[i]) == "-o") base = argv[i + 1];

    std::ifstream in(g_file);
    if (!in) { fprintf(stderr, "no puedo abrir %s\n", g_file.c_str()); return 1; }
    std::vector<string> lines;
    for (string l; std::getline(in, l); ) lines.push_back(l);

    // estructura de linea: [etiqueta:] [mnemonico [operandos]]
    struct Parsed { string label, mn; std::vector<string> ops; string src; int line; };
    std::vector<Parsed> prog;
    for (size_t i = 0; i < lines.size(); i++) {
        g_line = (int)i + 1;
        string raw = lines[i];
        size_t sc = raw.find(';');
        string code = trim(fold(sc == string::npos ? raw : raw.substr(0, sc)));
        if (code.empty()) continue;
        Parsed p; p.src = trim(raw); p.line = g_line;
        size_t colon = code.find(':');
        if (colon != string::npos && code.find(' ') > colon) {
            p.label = trim(code.substr(0, colon));
            code = trim(code.substr(colon + 1));
        }
        if (!code.empty()) {
            size_t sp = code.find_first_of(" \t");
            p.mn = (sp == string::npos) ? code : code.substr(0, sp);
            if (sp != string::npos) p.ops = splitCommas(trim(code.substr(sp)));
        }
        prog.push_back(p);
    }

    // ---- pasada 1: etiquetas y .equ
    g_addr = 0; g_pass2 = false;
    for (auto& p : prog) {
        g_line = p.line;
        if (!p.label.empty()) {
            if (g_sym.count(p.label)) err("simbolo redefinido: " + p.label);
            g_sym[p.label] = g_addr;
        }
        if (p.mn.empty()) continue;
        if (p.mn == ".EQU") {
            if (p.ops.size() != 2) { err(".equ NOMBRE, valor"); continue; }
            long v;
            if (evalExpr(p.ops[1], v, false)) g_sym[p.ops[0]] = v;
            continue;
        }
        if (p.mn == ".ORG") { long v; if (evalExpr(p.ops.size() ? p.ops[0] : "", v, false)) g_addr = (uint16_t)v; continue; }
        g_addr = (uint16_t)(g_addr + sizeOf(p.mn, p.ops));
    }

    // ---- pasada 2: emision
    g_addr = 0; g_pass2 = true;
    for (auto& p : prog) {
        g_line = p.line;
        if (p.mn.empty() || p.mn == ".EQU") continue;
        assembleLine(p.mn, p.ops, p.src);
    }
    if (g_errors) { fprintf(stderr, "%d error(es), no se genero salida\n", g_errors); return 1; }
    if (g_image.empty()) { fprintf(stderr, "programa vacio\n"); return 1; }

    // ---- salidas
    uint16_t maxA = g_image.rbegin()->first;
    std::ofstream fb(base + ".bin", std::ios::binary),
                  fl(base + ".lo.bin", std::ios::binary),
                  fh(base + ".hi.bin", std::ios::binary),
                  ls(base + ".lst");
    ls << "; asm16 — " << g_file << "\n; DIR   WORD  fuente\n";
    for (uint32_t a = 0; a <= maxA; a++) {
        uint16_t w = g_image.count((uint16_t)a) ? g_image[(uint16_t)a].word : 0;
        fb.put((char)(w & 0xFF)); fb.put((char)(w >> 8));
        fl.put((char)(w & 0xFF));
        fh.put((char)(w >> 8));
        if (g_image.count((uint16_t)a)) {
            char b[32]; snprintf(b, sizeof b, "  %04X  %04X  ", a, w);
            ls << b << g_image[(uint16_t)a].src << "\n";
        }
    }
    printf("asm16: %u palabras, 0x0000..0x%04X -> %s.bin (.lo/.hi/.lst)\n",
           (unsigned)g_image.size(), maxA, base.c_str());
    return 0;
}
