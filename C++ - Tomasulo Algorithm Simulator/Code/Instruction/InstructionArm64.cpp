/* Instruction/InstructionArm64.cpp */
#include "headers/InstructionArm64.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int biggest_instruction{6};

// Opcodes da arquitetura:
static const std::vector<std::string> LOADS
    {"ldr", "ldur", "ldp", "ldrb", "ldrh", "ldrsb", "ldrsh", "ldurb", "ldurh", "ldursb", "ldursh"};

static const std::vector<std::string> STORES
    {"str", "stur", "stp", "strb", "strh", "sturb", "sturh"};

static const std::vector<std::string> INT_BASIC
    {"add", "adds", "sub", "subs", "and", "orr", "eor", "lsl", "lsr", "mov", "movz", "movk", "movn", "mvn", "bic", "eon", "orn", "cmp", "cmn", "tst", "neg", "adc", "sbc", "asr", "ror", "nop"};

static const std::vector<std::string> INT_MUL
    {"mul", "smull", "umull", "madd", "msub", "smaddl", "umaddl"};

static const std::vector<std::string> INT_DIV
    {"sdiv", "udiv"};

static const std::vector<std::string> BRANCHES
    {"b", "b.eq", "b.ne", "bl", "blr", "ret", "cbz", "cbnz", "b.lt", "b.gt", "b.le", "b.ge", "b.hs", "b.hi", "b.ls", "b.lo", "b.mi", "b.pl", "b.vs", "b.vc", "tbz", "tbnz", "br"};

static const std::vector<std::string> FLOAT_BASIC
    {"fadd", "fsub", "fsqrt", "fcvt", "scvtf", "ucvtf", "fcvtzs", "fcvtzu", "fabs", "fneg", "fmin", "fmax", "fmla", "fcmp"};

static const std::vector<std::string> FLOAT_MUL
    {"fmul", "fmadd", "fmsub", "fnmadd", "fnmsub"};

static const std::vector<std::string> FLOAT_DIV
    {"fdiv"};

// Verifica se o opcode existe (está na tabela).
static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// Helper para identificar as posições nominais dos destinos da instrução.
// Retorna {-1} caso a instrução não possua um destino nominal entre os tokens.
static std::vector<int> GetDestines(
    const std::string& op,
    const INSTRUCTION_TYPE type
){
    // Stores e desvios não possuem token de destino.
    if (type == INSTRUCTION_TYPE::STORE || type == INSTRUCTION_TYPE::BRANCH) return {-1};
    // nop (HINT #0) não lê nem escreve registrador.
    if (op == "nop")                                                          return {-1};
    // Comparações afetam a flag implícita CPSR, logo não há token nominal de destino.
    if (op == "cmp" || op == "cmn" || op == "tst" || op == "fcmp")           return {-1};
    // ldp possui sempre dois destinos.
    if (op == "ldp")                                                         return {1, 2};

    // Regra geral (LOAD, Aritmética e Float): destino localizado no token 1.
    return {1};
}

// Identifica as posições nominais de fontes da instrução.
// - Retorna {-1} caso a instrução não possua fonte nominal entre os tokens.
static std::vector<int> GetSources(
    const std::string& op,
    const INSTRUCTION_TYPE type,
    const size_t num_tokens
){
    if (type == INSTRUCTION_TYPE::STORE) {
        if (op == "stp") return {1, 2, 3}; // Ex: stp x0, x1, [sp]
        return {1, 2};                     // Ex: str x0, [x1]
    }
    if (type == INSTRUCTION_TYPE::LOAD) {
        if (op == "ldp") return {3};       // Ex: ldp x0, x1, [sp]
        return {2};                        // Ex: ldr x0, [x1]
    }
    if (type == INSTRUCTION_TYPE::BRANCH) {
        // Desvios que requerem leitura nominal de registrador.
        if (op == "cbz" || op == "cbnz" || op == "tbz" || op == "tbnz" || op == "br" || op == "blr") return {1};
        if (op == "ret" && num_tokens == 2) return {1}; // ret com argumento
        // Demais branches não possuem fonte nominal nos tokens (ou usam x30/CPSR implicitamente).
        return {-1};
    }
    if (op == "cmp" || op == "cmn" || op == "tst" || op == "fcmp") {
        std::vector<int> srcs;
        for (size_t i = 1; i < num_tokens; ++i) srcs.push_back(static_cast<int>(i));
        return srcs.empty() ? std::vector<int>{-1} : srcs;
    }
    // nop (HINT #0) não lê nem escreve registrador.
    if (op == "nop") return {-1};

    // Regra geral para Aritmética e Float: o destino é o token 1, as fontes vão do 2 em diante.
    std::vector<int> srcs;
    for (size_t i = 2; i < num_tokens; ++i) srcs.push_back(static_cast<int>(i));
    return srcs.empty() ? std::vector<int>{-1} : srcs;
}

// xzr/wzr são o zero arquitetural: nunca são sobrescritos.
// - Não criam dependências nem ocupam slot no CDB (mesma ideia dos imediatos).
static bool IsZeroRegister(
    const std::string& token
){
    std::string lower = token;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower == "xzr" || lower == "wzr";
}

// Recebe o registrador alvo para procurar dependências de hardware:
// - x0 (64 bits) contém o w0 (32 bits de baixo).
// - Quando o x0 é usado, ele bloqueia w0 e vice-versa.
// - Mesmo id + máscaras sobrepostas = mesmo espaço de hardware (bloqueiam-se mutuamente).

// Mais simples que X86, porque, se dividem o mesmo hardware, necessariamente um bloqueia o outro.
static std::vector<Register> GetMaskedRegisters(
    const Register& target_reg
){
    std::vector<Register> blocked_regs;

    for (const auto& [name, reg] : RegisterTable()) {
        if (
            target_reg.GetType()   != reg.GetType() && // Tipo igual e id igual => o próprio registrador.
            target_reg.GetId()     == reg.GetId()   &&
            ((target_reg.GetMask() &  reg.GetMask()) != 0))
        {
            blocked_regs.push_back(reg);
        }
    }

    return blocked_regs;
}

// Adiciona o registrador e suas variantes mascaradas, sem duplicar slots:
// - ex: mov al, bl -> dests {al, ax, eax, rax} (al + os que compartilham hardware).
static void PushWithMasked(
    std::vector<Register>& regs,
    const Register&        reg
){
    for (const Register& candidate : regs) {
        if (candidate.GetType() == reg.GetType() && candidate.GetId() == reg.GetId() && candidate.GetMask() == reg.GetMask())
            return; // Já adicionado.
    }

    regs.push_back(reg);
    for (const Register& variant : GetMaskedRegisters(reg))
        regs.push_back(variant);
}

// Interpreta um operando de endereço ("[sp, 16]", "[x1]", "[x1, x2]", "[x1, x2, lsl 3]") e empurra cada registrador encontrado como fonte:
// - Base ("x1")  -> vira fonte.
// - Índice ("x2") -> vira fonte, se presente.
// - Imediato ("16") e mnemônico de shift/extensão ("lsl") -> ignorados (não estão na tabela).
// - xzr/wzr -> ignorado (IsZeroRegister()).
// Sem colchetes (defensivo, ARM64 não tem forma reg-reg para LOAD/STORE):
// - Trata como registrador único, igual aos demais operandos da instrução.
static void PushAddressSources(
    std::vector<Register>& sources,
    const std::string&     token,
    const std::string&     context
){
    std::string name{token};
    if (name.empty()) return;

    if (name.front() != '[') {
        if (IsRegister(name, RegisterTable()) && !IsZeroRegister(name))
            PushWithMasked(sources, LookupRegister(name, context, RegisterTable()));
        return;
    }

    // Verifica se a delimitação é válida (o SplitInstruction sempre fecha o colchete
    // antes de virar token, mas mantém o check por segurança contra entrada malformada).
    if (name.back() != ']') {
        std::cerr << "[ERRO] Endereço não fechado por colchetes:\n"  <<
        "Endereço: "<< token << '\n';
        std::abort();
    }

    // Remove os colchetes.
    name = name.substr(1, name.size() - 2);
    if (name.empty()) {
        std::cerr << "[ERRO] Endereço vazio (apenas colchetes enviados)!\n";
        std::abort();
    }

    std::string piece;
    for (char c : name) {
        if (c == ' ' || c == '\t' || c == ',' || c == '#'){
            // Ignora espaços repetidos.
            if (!piece.empty()) {
                if (IsRegister(piece, RegisterTable()) && !IsZeroRegister(piece))
                    PushWithMasked(sources, LookupRegister(piece, context, RegisterTable()));
                piece.clear(); // Limpa a string para comportar mais pieces.
            }
        }
        else piece += c;
    }
    // Pega o último piece capturado (quando termina, faz só o piece += c).
    if (!piece.empty() && IsRegister(piece, RegisterTable()) && !IsZeroRegister(piece))
        PushWithMasked(sources, LookupRegister(piece, context, RegisterTable()));
}

// Função de Instruction.h:
// - Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Aliases compartilham o mesmo id:
    // - ids 0-30:  x0-30 ('L') = w0-30 ('R').
    // - ids 32-63: d0-31 ('S') = s0-31 ('F').
    // - id 80:     cpsr ('G').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).

        // Int (0-30):
        for (int i = 0; i < 31; i++){
            t.emplace("x" + std::to_string(i), Register('L', i, 0xFF));
            t.emplace("w" + std::to_string(i), Register('R', i, 0x0F));
        }

        // Aliases arquiteturais (mesmo hardware dos slots acima):
        // - lr (link register) é o x30.
        // - sp (stack pointer) é o x31 (slot extra do CDB).
        // - xzr/wzr (zero register) ficam de fora (nunca são escritos, então não geram dependência).
        t.emplace("lr", Register('L', 30, 0xFF));
        t.emplace("sp", Register('L', 31, 0xFF));

        // Float (32-63):
        for (int i = 0; i < 32; i++) {
            t.emplace("d" + std::to_string(i), Register('S', 32 + i, 0xFF));
            t.emplace("s" + std::to_string(i), Register('F', 32 + i, 0x0F));
        }

        t.emplace("cpsr", Register('G', 80, 0xFF));
    }

    return t;
}

// Monta o CDB com os registradores físicos:
// - Layout por variante (um slot por (id, mask)), contíguo por variante:
// - ids 0-32:  Faixas int (L/R), máscaras 0xFF/0x0F (31 inclui o sp).
// - ids 32-64: Faixas int (S/F), máscaras 0xFF/0x0F.
// - id 80:     'G' (cspr), 0xFF.
CDB InstructionArm64::MakeCDB() {
    CDB cdb;
    // Layout contíguo por variante:
    FillCDB(cdb, 'L', 0,  32, 0xFF);
    FillCDB(cdb, 'R', 0,  31, 0x0F);
    FillCDB(cdb, 'S', 32, 32, 0xFF);
    FillCDB(cdb, 'F', 32, 32, 0x0F);
    FillCDB(cdb, 'G', 80, 1,  0xFF);

    cdb.print_banks = {
        {'L', 0,   32},
        {'R', 32,  31},
        {'S', 63,  32},
        {'F', 95,  32},
        {'G', 127, 1}
    };
    return cdb;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionArm64::InstructionArm64(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionArm64::IdentifyType(
    const std::vector<std::string>& tokens
){
    std::string op{tokens[0]};
    for (char& c : op) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (Contains(LOADS, op))            type = INSTRUCTION_TYPE::LOAD;
    else if (Contains(STORES, op))      type = INSTRUCTION_TYPE::STORE;
    else if (Contains(INT_BASIC, op))   type = INSTRUCTION_TYPE::INT_BASIC;
    else if (Contains(BRANCHES, op))    type = INSTRUCTION_TYPE::BRANCH;
    else if (Contains(INT_MUL, op))     type = INSTRUCTION_TYPE::INT_MUL;
    else if (Contains(INT_DIV, op))     type = INSTRUCTION_TYPE::INT_DIV;
    else if (Contains(FLOAT_BASIC, op)) type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (Contains(FLOAT_MUL, op))   type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (Contains(FLOAT_DIV, op))   type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    return true;
}

// Privado:
// Separa a instrução nos seus componentes (OpCode + reg/label/imediato ...).
std::vector<std::string> InstructionArm64::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;
    bool in_bracket = false;

    for (char c : str) {
        // Colchetes têm prioridade: tudo entre '[' e ']' vira 1 token só.
        // (ex.: "[sp, 16]", "[x1, x2, lsl 3]"), igual ao x86.
        if (c == '[') {
            in_bracket = true;
            current += c;
            continue;
        }
        if (c == ']') {
            current += c;
            // Fecha o token assim que o colchete termina, pra não colar o '!'
            // de writeback ("[sp, -16]!") no mesmo token do endereço.
            tokens.push_back(current);
            current.clear();
            in_bracket = false;
            continue;
        }

        if ((c == ',' || c == ' ' || c == '\t') && !in_bracket) {
            // Ignora os espaços repetidos.
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    // Evita perda de informação do último caracter (quando termina, faz só o piece += c).
    // - Verifica se está vazio, pois pode ser enviado algo como "add x0 x1 x2 " (espaço no final).
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
// Padroniza os OpCodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionArm64::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Labels de desvio são case-sensitive: só opcode e registradores viram minúsculo.
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0 && !IsRegister(tokens[i], RegisterTable())) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Compacta o interior dos colchetes ("[ X1 , #8 ]" -> "[x1, #8]"):
    // - Remove espaços nas bordas e antes de vírgula; mantém um espaço após a vírgula
    //   (ex.: "lsl 3" dentro de "[x1, x2, lsl 3]" não é alterado).
    for (std::string& t : tokens) {
        if (t.size() >= 2 && t.front() == '[' && t.back() == ']') {
            std::string interior = t.substr(1, t.size() - 2);
            std::string compacted;
            for (size_t k = 0; k < interior.size(); ++k) {
                if (interior[k] == ' ' &&
                    (k == 0 || k + 1 == interior.size() || interior[k + 1] == ','))
                    continue;
                compacted += interior[k];
            }
            t = "[" + compacted + "]";
        }
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

    for (size_t i = 1; i < tokens.size(); ++i)
        // O '!' de writeback ("[sp, -16]!") cola direto no colchete, sem vírgula antes.
        normalized += (i == 1 || tokens[i] == "!" ? "" : ", ") + tokens[i];

    instruction_string = normalized;
}

// Privado:
void InstructionArm64::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    source_registers.clear();

    std::vector<int> expected_dests = GetDestines(tokens[0], type);
    std::vector<int> expected_srcs  = GetSources(tokens[0], type, tokens.size());

    ValidateInstruction(tokens, expected_dests, expected_srcs);

    // Instruções de 5 tokens:
    if (tokens.size() == 5){
        // Comparadores com shift/extensão - "cmp x0, x1, lsl #3":
        // - Fontes  = todo token que for registrador (a partir de tokens[1]);
        // - Destino = flag "cpsr";
        if (tokens[0] == "cmp" || tokens[0] == "cmn" || tokens[0] == "tst" || tokens[0] == "fcmp") {
            for (size_t i{1}; i < tokens.size(); ++i){
                if (IsRegister(tokens[i], RegisterTable()) && !IsZeroRegister(tokens[i]))
                    PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
            }
            dest_registers.push_back(Register('G', 80));
            return;
        }
        // "stp x29, x30, [sp, #-16]!" (writeback pré-indexado):
        // - Fontes   = tokens[1], tokens[2] e endereço (tokens[3]);
        // - Sem destino (writeback na base não é modelado, igual ao pós-indexado);
        // - tokens[4] é o marcador de writeback '!' - ignorado.
        else if (type == INSTRUCTION_TYPE::STORE && tokens[4] == "!") {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (!IsZeroRegister(tokens[2]))
                PushWithMasked(source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
            PushAddressSources(source_registers, tokens[3], instruction_string);
            return;
        }
        // "ldp x0, x1, [sp, #16]!" (writeback pré-indexado):
        // - Fontes   = endereço (tokens[3]);
        // - Destinos = tokens[1] e tokens[2];
        // - tokens[4] é o marcador de writeback '!' - ignorado.
        else if (type == INSTRUCTION_TYPE::LOAD && tokens[4] == "!") {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (!IsZeroRegister(tokens[2]))
                PushWithMasked(dest_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
            PushAddressSources(source_registers, tokens[3], instruction_string);
            return;
        }
    }
    // Instruções de 4 tokens:
    else if (tokens.size() == 4) {
        if (tokens[0] == "ldp" || tokens[0] == "stp") {
            // "ldp x29, x30, [sp, #16]":
            // - Fonte    = tokens[3] (apenas o registrador);
            // - Destinos = tokens[1] e tokens[2];
            if (type == INSTRUCTION_TYPE::LOAD) {
                // Registrador '0' é contabilizado como imediato.
                if (!IsZeroRegister(tokens[1]))
                    PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
                if (!IsZeroRegister(tokens[2]))
                    PushWithMasked(dest_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
                PushAddressSources(source_registers, tokens[3], instruction_string);
                return;
            }
            // "stp x29, x30, [sp, #-16]":
            // - Fontes = tokens[1], tokens[2] e tokens[3];
            // - Sem destino.
            // (A forma com writeback "stp x29, x30, [sp, #-16]!" tem 5 tokens
            // e é tratada no bloco de 5 tokens.)
            else if (type == INSTRUCTION_TYPE::STORE){
                if (!IsZeroRegister(tokens[1]))
                    PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
                if (!IsZeroRegister(tokens[2]))
                    PushWithMasked(source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
                PushAddressSources(source_registers, tokens[3], instruction_string);
                return;
            }
        }
        // "tbz x0, #3, LOOP" / "tbnz x0, #3, LOOP":
        // - Fonte = tokens[1].
        // - Sem destino.
        else if (tokens[0] == "tbz" || tokens[0] == "tbnz") {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // "ldr x0, [x1], #16" (pós-indexado):
        // - Fonte    = tokens[2];
        // - Destino  = tokens[1];
        // - tokens[3] é o deslocamento pós-índice (sempre imediato) - ignorado.
        else if (type == INSTRUCTION_TYPE::LOAD) {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            PushAddressSources(source_registers, tokens[2], instruction_string);
            return;
        }
        // "str x0, [x1], #16" (pós-indexado):
        // - Fontes = tokens[1] e tokens[2];
        // - Sem destino.
        // - tokens[3] é o deslocamento pós-índice (sempre imediato) - ignorado.
        else if (type == INSTRUCTION_TYPE::STORE) {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            PushAddressSources(source_registers, tokens[2], instruction_string);
            return;
        }
        // Aritmética geral - "add x1, x2, x3" (com ou sem shift/extensão):
        // - Fontes  = todo token que for registrador (a partir de tokens[2]);
        // - Destino = tokens[1];
        else {
            for (size_t i{2}; i < tokens.size(); ++i){
                if (IsRegister(tokens[i], RegisterTable()) && !IsZeroRegister(tokens[i]))
                    PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
            }
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            // Sufixo 's' (como em "adds") atualiza a flag "cpsr" (apenas aritmética inteira).
            // - OpCodes FLOAT terminados em 's' (fabs, fcvtzs) não setam flags na ARM64 real.
            if (tokens[0].back() == 's' &&
                (type == INSTRUCTION_TYPE::INT_BASIC ||
                 type == INSTRUCTION_TYPE::INT_MUL ||
                 type == INSTRUCTION_TYPE::INT_DIV))
                dest_registers.push_back(Register('G', 80));
            return;
        }
    }
    // Instruções de 3 tokens:
    else if (tokens.size() == 3) {
        // "ldr x0, [x1, #8]" (com ou sem deslocamento, o colchete inteiro é 1 token):
        // - Fonte    = tokens[2].
        // - Destino  = tokens[1];
        if (type == INSTRUCTION_TYPE::LOAD) {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            PushAddressSources(source_registers, tokens[2], instruction_string);
            return;
        }
        // "str x0, [x1, #8]":
        // - Fontes = tokens[1] e tokens[2].
        // - Sem destino.
        else if (type == INSTRUCTION_TYPE::STORE) {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            PushAddressSources(source_registers, tokens[2], instruction_string);
            return;
        }
        // Comparadores - "cmp x0, x1":
        // - Fontes  = tokens[1] e tokens[2];
        // - Destino = flag "cpsr";
        else if (tokens[0] == "cmp" || tokens[0] == "cmn" || tokens[0] == "tst" || tokens[0] == "fcmp") {
            for (size_t i{1}; i < tokens.size(); ++i){
                if (IsRegister(tokens[i], RegisterTable()) && !IsZeroRegister(tokens[i]))
                    PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
            }
            dest_registers.push_back(Register('G', 80));
            return;
        }
        // "cbz x0, EXIT" / "cbnz x0, EXIT":
        // - Fonte = tokens[1].
        // - Sem destino.
        else if (type == INSTRUCTION_TYPE::BRANCH && (tokens[0] == "cbz" || tokens[0] == "cbnz")) {
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // Aritmética de 2 operandos - "fcvtzs w0, d1" / "mov x0, xzr" / "neg x0, x1":
        // - Fontes  = todo token que for registrador a partir de tokens[2];
        // - Destino = tokens[1];
        else {
            for (size_t i{2}; i < tokens.size(); ++i){
                if (IsRegister(tokens[i], RegisterTable()) && !IsZeroRegister(tokens[i]))
                    PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
            }
            if (!IsZeroRegister(tokens[1]))
                PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            // Sufixo 's' (como em "adds") atualiza a flag "cpsr" (apenas aritmética inteira).
            // - OpCodes FLOAT terminados em 's' (fabs, fcvtzs) não setam flags na ARM64 real.
            if (tokens[0].back() == 's' &&
                (type == INSTRUCTION_TYPE::INT_BASIC ||
                 type == INSTRUCTION_TYPE::INT_MUL ||
                 type == INSTRUCTION_TYPE::INT_DIV))
                dest_registers.push_back(Register('G', 80));
            return;
        }
    }
    // Instruções de 2 tokens:
    else if (tokens.size() == 2) {
        if (type == INSTRUCTION_TYPE::BRANCH) {
            // "b LOOP": desvio incondicional para label.
            // - Não lê nem escreve registrador.
            if (tokens[0] == "b") {
                return;
            }
            // "b.eq LOOP" (e demais condicionais):
            // - Fonte = flag 'cpsr';
            // Sem destino.
            else if (tokens[0].rfind("b.", 0) == 0) {
                source_registers.push_back(Register('G', 80));
                return;
            }
            // "bl func":
            // - Destino = link register (x30);
            // Sem fonte.
            else if (tokens[0] == "bl") {
                dest_registers.push_back(Register('L', 30, 0xFF));
                return;
            }
            // "br x0":
            // - Fonte = tokens[1];
            // Sem destino.
            else if (tokens[0] == "br") {
                if (!IsZeroRegister(tokens[1]))
                    PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
                return;
            }
            // "blr x0":
            // - Fonte   = tokens[1];
            // - Destino = link register (x30);
            else if (tokens[0] == "blr") {
                if (!IsZeroRegister(tokens[1]))
                    PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
                dest_registers.push_back(Register('L', 30, 0xFF));
                return;
            }
            // "ret x5":
            // - Fonte = tokens[1];
            // Sem destino.
            else if (tokens[0] == "ret") {
                if (!IsZeroRegister(tokens[1]))
                    PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
                return;
            }
        }
    }
    // Instruções de 1 token:
    else {
        // "ret":
        // - Fonte = link register (x30);
        // Sem destino.
        if (type == INSTRUCTION_TYPE::BRANCH && tokens[0] == "ret") {
            source_registers.push_back(Register('L', 30, 0xFF));
            return;
        }
        // "nop":
        // - Sem fonte.
        // - Sem destino.
        else if (tokens[0] == "nop") {
            return;
        }
    }
    // Não atendeu nenhum caso (sem return anterior).
    std::cerr << "[ERRO] Instrução com sintaxe não suportada:\n"  <<
    "Instrução: "<< instruction_string << '\n';
    std::abort();
}

// Verifica se os destinos e fontes correspondem à sintaxe da linguagem.
// - Aborta em caso contrário, sem possibilidade de escrita incorreta.
void InstructionArm64::ValidateInstruction(
    const std::vector<std::string>& tokens,
    const std::vector<int>&         expected_dests,
    const std::vector<int>&         expected_srcs

){
    // 1. Destinos nominais devem ser invariavelmente registradores.
    for (int pos : expected_dests) {
        if (pos != -1 && pos < static_cast<int>(tokens.size())) {
            // Rejeita imediatos, endereços ou nomenclaturas que não existam na RegisterTable.
            if (tokens[pos].front() == '#' || tokens[pos].front() == '[' ||
                (!IsRegister(tokens[pos], RegisterTable()) && !IsZeroRegister(tokens[pos]))) {
                std::cerr << "[ERRO] Operando inválido no destino. Esperado registrador em: '" << tokens[pos] << "'\n"
                          << "Instrução: " << instruction_string << '\n';
                std::abort();
            }
        }
    }

    // 2. Validações restritas por tipo de instrução (Labels, Imediatos, Endereços).
    if (type == INSTRUCTION_TYPE::BRANCH) {
        // Em desvios que requerem label, ele deve ser o último token validado.
        if (tokens[0] == "b" || tokens[0].rfind("b.", 0) == 0 || tokens[0] == "bl" || tokens[0] == "cbz" || tokens[0] == "cbnz" || tokens[0] == "tbz" || tokens[0] == "tbnz") {
            const std::string& label = tokens.back();
            if (IsRegister(label, RegisterTable()) || IsZeroRegister(label) || label.front() == '#' || label.front() == '[') {
                std::cerr << "[ERRO] Esperado LABEL no desvio. Recebido tipo incompatível: '" << label << "'\n"
                          << "Instrução: " << instruction_string << '\n';
                std::abort();
            }
        }
        // Fontes nominais de desvio (ex: br x1, cbz x2, Target) devem ser registradores.
        for (int pos : expected_srcs) {
            if (pos != -1 && pos < static_cast<int>(tokens.size())) {
                if (!IsRegister(tokens[pos], RegisterTable()) && !IsZeroRegister(tokens[pos])) {
                    std::cerr << "[ERRO] Esperado registrador como fonte no branch. Recebido: '" << tokens[pos] << "'\n"
                              << "Instrução: " << instruction_string << '\n';
                    std::abort();
                }
            }
        }
    } else if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // Uma instrução de Load/Store não aceita um imediato avulso solto como endereço/base.
        for (int pos : expected_srcs) {
            if (pos != -1 && pos < static_cast<int>(tokens.size()) && tokens[pos].front() == '#') {
                std::cerr << "[ERRO] Endereço de memória ou base inválida. Recebido: '" << tokens[pos] << "'\n"
                          << "Instrução: " << instruction_string << '\n';
                std::abort();
            }
        }
    } else {
        // Instruções Aritméticas e Lógicas não aceitam colchetes (endereços de memória) injetados nas fontes.
        for (int pos : expected_srcs) {
            if (pos != -1 && pos < static_cast<int>(tokens.size()) && tokens[pos].front() == '[') {
                std::cerr << "[ERRO] Operando inválido para aritmética (endereço não suportado): '" << tokens[pos] << "'\n"
                          << "Instrução: " << instruction_string << '\n';
                std::abort();
            }
        }
    }
}
} // namespace processor
