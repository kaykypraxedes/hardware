/* Architectures/X86Intel.cpp */
#include "headers/X86Intel.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header "Architecture.h"
 *
 * Dessa maneira, evita-se a redundância de comentários em cada
 * subclasse, visto que todos os métodos tem o mesmo resultado,
 * mudando apenas o método de implementação (consequência da sua
 * arquitetura e organização interna).
 */

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const size_t biggest_instruction{10}; // cvttss2si

// Opcodes da arquitetura:
static const std::vector<std::string> MOVS
    {"mov", "movss", "movsd", "movaps", "movups", "movq", "movd"};

// movsx/movzx ficam FORA de MOVS:
// - Mudam a largura entre destino e fonte (sign/zero-extend), diferente do padrão "mesma largura" do MOV puro.
static const std::vector<std::string> MOVSX_MOVZX
    {"movsx", "movzx"};

static const std::vector<std::string> INT_BASIC
    {"add", "sub", "and", "or", "xor", "inc", "dec", "cmp", "shl", "shr", "not", "neg", "test", "rol", "ror", "sar", "sal", "sbb", "adc", "lea", "push", "pop"};

static const std::vector<std::string> INT_MUL
    {"imul", "mul"};

static const std::vector<std::string> INT_DIV
    {"idiv", "div"};

static const std::vector<std::string> BRANCHES
    {"jmp", "je", "jne", "jg", "jge", "jl", "jle", "call", "jbe", "ja", "jae", "jb", "js", "jns", "jp", "jo", "ret"};

static const std::vector<std::string> FLOAT_BASIC
    {"addss", "addsd", "subss", "subsd", "sqrtss", "addps", "subps", "cvtsi2ss", "cvttss2si", "comiss", "ucomiss", "pxor", "pand", "por"};

// mulps/divps:
// - Variantes "packed" (128-bit) de mulss/mulsd e divss/divsd.
static const std::vector<std::string> FLOAT_MUL
    {"mulss", "mulsd", "mulps"};

static const std::vector<std::string> FLOAT_DIV
    {"divss", "divsd", "divps"};

// Opcodes de "escrita pura": nunca leem o valor antigo do destino antes de sobrescrever
// - Diferente do padrão RMW (Read-Modify-Write) do resto do x86 CISC.
static const std::vector<std::string> PURE_WRITE
    {"movsx", "movzx", "cvtsi2ss", "cvttss2si", "sqrtss"};

// Opcodes de INT_BASIC que NÃO alteram flags (mov/movsx/movzx entram via MOVS).
static const std::vector<std::string> NO_FLAGS
    {"movsx", "movzx", "not"};

// Função de Instruction.h.
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Tabela: (nome, registrador físico).
    // - Famílias de propósito geral compartilham o MESMO id entre larguras diferentes (ex.: id 3 = rdx/edx/dx/dl).
    //
    // - ids 0-16:  'L' (64-bit, máscara 0xFF) > 'R' (32-bit, 0x0F) > 'W' (16-bit, 0x03) > 'B' (8-bit, 0x01 = byte baixo).
    // - ids 0-4:   'B' (8-bit, 0x02 = byte alto - só al/bl/cl/dl têm "metade alta" endereçável, ah/bh/ch/dh).
    // - ids 32-47: 'F' (xmm0-15), sem sombreamento (0xFF fixo).
    // - ids 80-85: 'G' (cf, pf, af, zf, sf, of), sem sombreamento (0xFF fixo).
    static std::unordered_map<std::string, Register> t;

    if (t.empty()) { // Evita refazer os emplaces a cada chamada da função (já que "t" é "static").
        const std::vector<std::string> l64{"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp"};
        const std::vector<std::string> r32{"eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp"};
        const std::vector<std::string> w16{"ax",  "bx",  "cx",  "dx",  "si",  "di",  "sp",  "bp"};
        const std::vector<std::string> b8lo{"al",  "bl",  "cl",  "dl",  "sil", "dil", "spl", "bpl"};
        const std::vector<std::string> b8hi{"ah",  "bh",  "ch",  "dh"}; // só al/bl/cl/dl têm metade alta.

        for (int i = 0; i < 8; i++) {
            t.emplace(l64[i],  Register('L', i, 0xFF));
            t.emplace(r32[i],  Register('R', i, 0x0F));
            t.emplace(w16[i],  Register('W', i, 0x03));
            t.emplace(b8lo[i], Register('B', i, 0x01));

            // r8-r15 (só existem nas 4 larguras "planas", sem ah/bh/ch/dh equivalente).
            t.emplace("r" + std::to_string(8 + i),       Register('L', 8 + i, 0xFF));
            t.emplace("r" + std::to_string(8 + i) + "d", Register('R', 8 + i, 0x0F));
            t.emplace("r" + std::to_string(8 + i) + "w", Register('W', 8 + i, 0x03));
            t.emplace("r" + std::to_string(8 + i) + "b", Register('B', 8 + i, 0x01));
        }
        for (int i = 0; i < 4; i++) t.emplace(b8hi[i], Register('B', i, 0x02));

        // Registradores SIMD/Float.
        for (int i = 0; i < 16; i++) t.emplace("xmm" + std::to_string(i), Register('F', 32 + i));

        // Flags implícitas rastreadas separadamente.
        const std::vector<std::string> flags{"cf", "pf", "af", "zf", "sf", "of"};
        for (int i = 0; i < 6; i++) t.emplace(flags[i], Register('G', 80 + i));

        // 'rip' não entra na tabela de propósito: é implícito e nunca é reescrito
        // por instrução (a dependência estaria sempre pronta), então nunca deve
        // virar fonte rastreada — ver PushOperandSources().
    }
    return t;
}

// ─── HELPERS ──────────────────────────────────────────────────────

// Normaliza tokens para permitir comparações case-insensitive sem alterar a entrada original.
static std::string ToLower(const std::string& token) {
    std::string lower{token};
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower;
}

// Qualquer registrador da tabela, de qualquer classe (GPR, XMM ou flag).
static bool IsRegister(const std::string& token) {
    return RegisterTable().find(ToLower(token)) != RegisterTable().end();
}

// Identifica registradores de propósito geral em qualquer largura ou byte endereçável.
static bool IsIntReg(const std::string& token) {
    const auto& table{RegisterTable()};

    // Qualquer registrador de propósito geral, em QUALQUER largura (L/R/W/B).
    const auto  it{table.find(ToLower(token))};
    if (it == table.end()) return false;
    const char t{it->second.GetType()};
    return (t == 'L' || t == 'R' || t == 'W' || t == 'B');
}

// Identifica registradores XMM pelo tipo físico registrado na arquitetura.
static bool IsFloatReg(const std::string& token) {
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};
    return (it != table.end() && it->second.GetType() == 'F');
}

// Resolve o registrador canônico de um token já validado e interrompe em caso de quebra dessa invariável.
static const Register& LookupReg(const std::string& token) {
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};
    if (it == table.end()) {
        std::cerr << "[ERRO] Registrador não encontrado na tabela: " << token << '\n';
        std::abort();
    }
    return it->second;
}

// Retorna aliases do mesmo registrador físico cujas máscaras se sobrepõem ao alvo.
static std::vector<Register> GetMaskedRegisters(const Register& target) {
    std::vector<Register> blocked;
    for (const auto& [name, reg] : RegisterTable()) {
        if (target.GetType() != reg.GetType() &&
            target.GetId()   == reg.GetId()   &&
            (target.GetMask() & reg.GetMask()) != 0) {
            blocked.push_back(reg);
        }
    }
    return blocked;
}

// Compara a identidade completa de dois registradores, incluindo a faixa mascarada.
static bool SameRegister(const Register& lhs, const Register& rhs) {
    return lhs.GetType() == rhs.GetType() &&
           lhs.GetId()   == rhs.GetId()   &&
           lhs.GetMask() == rhs.GetMask();
}

// Adiciona um registrador somente quando sua identidade completa ainda não está no vetor.
static void PushUnique(std::vector<Register>& regs, const Register& reg) {
    for (const Register& candidate : regs)
        if (SameRegister(candidate, reg)) return;
    regs.push_back(reg);
}

static void PushWithMasked(std::vector<Register>& regs, const Register& reg) {
    // Adiciona o registrador e todas as variantes realmente sobrepostas.
    // Cada variante é verificada separadamente: retornar cedo ao encontrar
    // somente o registrador principal poderia omitir aliases ainda ausentes.
    PushUnique(regs, reg);
    for (const Register& variant : GetMaskedRegisters(reg)) PushUnique(regs, variant);
}

// Adiciona todas as flags rastreadas como dependências, preservando a regra de aliases.
static void PushAllFlags(std::vector<Register>& regs) {
    static const std::vector<std::string> flags{"cf", "pf", "af", "zf", "sf", "of"};
    for (const std::string& flag : flags) PushWithMasked(regs, LookupReg(flag));
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
// Público:
CDB InstructionX86Intel::MakeCDB() {
    // Layout contíguo por variante:
    // - ids 0-15:  L/R/W/B (larguras de GPR).
    // - ids 32-47: F (xmm0-15).
    // - ids 80-85: G (cf, pf, af, zf, sf, of).
    CDB cdb;
    FillCDB(cdb, 'L', 0,  16, 0xFF);
    FillCDB(cdb, 'R', 0,  16, 0x0F);
    FillCDB(cdb, 'W', 0,  16, 0x03);
    FillCDB(cdb, 'B', 0,  16, 0x01);
    FillCDB(cdb, 'B', 0,  4,  0x02); // ah/bh/ch/dh
    FillCDB(cdb, 'F', 32, 16, 0xFF);
    FillCDB(cdb, 'G', 80, 6,  0xFF);

    // Registra as faixas na mesma ordem de inserção para imprimir cada variante como um banco separado.
    cdb.print_banks = {
        {'L', 0,  16},
        {'R', 16, 16},
        {'W', 32, 16},
        {'B', 48, 16},
        {'B', 64, 4},
        {'F', 68, 16},
        {'G', 84, 6}
    };
    return cdb;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionX86Intel::InstructionX86Intel(const int position) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
std::vector<std::string> InstructionX86Intel::SplitInstruction(const std::string& str) const {
    std::vector<std::string> tokens;

    // Remove espaços externos sem modificar os espaços internos dos operandos.
    std::size_t begin{};
    while (begin < str.size() && (str[begin] == ' ' || str[begin] == '\t')) begin++;
    std::size_t end{str.size()};
    while (end > begin && (str[end - 1] == ' ' || str[end - 1] == '\t')) end--;
    if (begin == end) return tokens;

    // Restringe a entrada à gramática que o restante do parser sabe interpretar.
    // - Também rejeita caracteres de controle e bytes não ASCII antes da tokenização.
    for (std::size_t i{begin}; i < end; i++) {
        const unsigned char c{static_cast<unsigned char>(str[i])};
        const bool supported{std::isalnum(c) || c == '_' || c == ' ' || c == '\t' ||
                             c == ',' || c == '[' || c == ']' || c == '+' || c == '-' || c == '*'};
        if (c >= 128 || (c < 32 && c != '\t') || !supported) {
            std::cerr << "[ERRO] Caractere não suportado na instrução.\n";
            std::abort();
        }
    }

    // Separa o opcode e proíbe vírgula colada a ele, pois vírgulas delimitam apenas operandos.
    std::size_t opcode_end{begin};
    while (opcode_end < end && str[opcode_end] != ' ' && str[opcode_end] != '\t' && str[opcode_end] != ',') opcode_end++;
    if (opcode_end == begin || str[opcode_end == end ? end - 1 : opcode_end] == ',') {
        std::cerr << "[ERRO] Separador inválido junto ao opcode.\n";
        std::abort();
    }
    tokens.push_back(str.substr(begin, opcode_end - begin));

    std::size_t cursor{opcode_end};
    while (cursor < end && (str[cursor] == ' ' || str[cursor] == '\t')) cursor++;
    if (cursor == end) return tokens;

    // Separa operandos somente por vírgulas externas aos colchetes.
    // - A expressão de memória permanece inteira para ParseMemoryOperand().
    // - bracket_depth também detecta fechamento sem abertura e aninhamento não suportado.
    std::size_t operand_begin{cursor};
    int bracket_depth{};
    for (std::size_t i{cursor}; i < end; i++) {
        const char c{str[i]};
        if (c == '[') {
            if (bracket_depth != 0) {
                std::cerr << "[ERRO] Colchetes aninhados não são suportados.\n";
                std::abort();
            }
            bracket_depth = 1;
        } else if (c == ']') {
            if (bracket_depth != 1) {
                std::cerr << "[ERRO] Colchete de fechamento sem abertura.\n";
                std::abort();
            }
            bracket_depth = 0;
        } else if (c == ',' && bracket_depth == 0) {
            std::size_t piece_end{i};
            while (piece_end > operand_begin && (str[piece_end - 1] == ' ' || str[piece_end - 1] == '\t')) piece_end--;
            if (piece_end == operand_begin) {
                std::cerr << "[ERRO] Operando vazio entre vírgulas.\n";
                std::abort();
            }
            tokens.push_back(str.substr(operand_begin, piece_end - operand_begin));
            operand_begin = i + 1;
            while (operand_begin < end && (str[operand_begin] == ' ' || str[operand_begin] == '\t')) operand_begin++;
            i = operand_begin == 0 ? 0 : operand_begin - 1;
        }
    }
    if (bracket_depth != 0) {
        std::cerr << "[ERRO] Operando de memória sem colchete de fechamento.\n";
        std::abort();
    }

    // Salva o último operando e rejeita uma vírgula final sem conteúdo posterior.
    std::size_t piece_end{end};
    while (piece_end > operand_begin && (str[piece_end - 1] == ' ' || str[piece_end - 1] == '\t')) piece_end--;
    if (piece_end == operand_begin) {
        std::cerr << "[ERRO] Vírgula sem operando posterior.\n";
        std::abort();
    }
    tokens.push_back(str.substr(operand_begin, piece_end - operand_begin));
    return tokens;
}

// Privado:
// Converte literal decimal ou hexadecimal para valor validado e representação canônica.
InstructionX86Intel::INTEGER_LITERAL InstructionX86Intel::ParseInteger(
    const std::string& token,
    bool               memory_displacement
) {
    INTEGER_LITERAL literal;

    // Extrai sinal e prefixo de base antes de processar os dígitos.
    std::size_t cursor{};
    if (!token.empty() && token[0] == '-') {
        literal.negative = true;
        cursor = 1;
    }
    if (cursor == token.size() || token[cursor] == '+') {
        std::cerr << "[ERRO] Literal inteiro incompleto.\n";
        std::abort();
    }
    if (cursor + 1 < token.size() && token[cursor] == '0' &&
        (token[cursor + 1] == 'x' || token[cursor + 1] == 'X')) {
        literal.radix = 16;
        cursor += 2;
    }
    if (cursor == token.size()) {
        std::cerr << "[ERRO] Literal hexadecimal incompleto.\n";
        std::abort();
    }

    // Acumula a magnitude verificando cada dígito e prevenindo overflow antes da multiplicação.
    const std::size_t digits_begin{cursor};
    unsigned long magnitude{};
    for (; cursor < token.size(); cursor++) {
        const unsigned char c{static_cast<unsigned char>(token[cursor])};
        int digit{-1};
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (literal.radix == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (literal.radix == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        if (digit < 0 || digit >= literal.radix ||
            magnitude > (std::numeric_limits<unsigned long>::max() - digit) / literal.radix) {
            std::cerr << "[ERRO] Literal inteiro inválido ou fora do limite.\n";
            std::abort();
        }
        magnitude = magnitude * literal.radix + static_cast<unsigned>(digit);
    }

    // Deslocamentos de memória precisam caber em long; imediatos positivos podem usar toda a faixa unsigned long.
    const unsigned long negative_limit{static_cast<unsigned long>(std::numeric_limits<long>::max()) + 1};
    if ((literal.negative && magnitude > negative_limit) ||
        (memory_displacement && !literal.negative && magnitude > static_cast<unsigned long>(std::numeric_limits<long>::max()))) {
        std::cerr << "[ERRO] Literal inteiro fora do limite com sinal.\n";
        std::abort();
    }
    literal.magnitude = magnitude;

    // Remove zeros redundantes, padroniza hexadecimal e preserva o sinal na forma normalizada.
    std::string digits{token.substr(digits_begin)};
    std::size_t nonzero{};
    while (nonzero + 1 < digits.size() && digits[nonzero] == '0') nonzero++;
    digits = ToLower(digits.substr(nonzero));
    literal.normalized = std::string(literal.negative ? "-" : "") +
                         (literal.radix == 16 ? "0x" : "") + digits;
    return literal;
}

// Privado:
// Classifica um token pela gramática x86 e preenche somente a representação correspondente.
InstructionX86Intel::OPERAND InstructionX86Intel::ParseOperand(
    const std::string& token
) const {
    OPERAND operand;
    const std::string lower{ToLower(token)};

    // A ordem evita interpretar registradores e expressões entre colchetes como labels.
    if (IsRegister(lower)) {
        operand.type = OPERAND_TYPE::REGISTER;
        operand.register_name = lower;
        return operand;
    }

    // Qualquer colchete torna o token candidato a memória; ParseMemoryOperand() valida o par e o conteúdo.
    if (token.find('[') != std::string::npos || token.find(']') != std::string::npos) {
        operand.type = OPERAND_TYPE::MEMORY;
        operand.memory = ParseMemoryOperand(token);
        return operand;
    }

    // Tokens iniciados como número devem ser literais completos; não caem para label em caso de erro.
    const bool numeric_start{!token.empty() && (std::isdigit(static_cast<unsigned char>(token[0])) || token[0] == '-' || token[0] == '+')};
    if (numeric_start) {
        operand.type = OPERAND_TYPE::IMMEDIATE;
        operand.immediate = ParseInteger(token, false);
        return operand;
    }

    // O caso restante é uma label case-sensitive que não pode reutilizar um nome reservado.
    if (!IsValidLabel(token)) {
        std::cerr << "[ERRO] Label inválida.\n";
        std::abort();
    }
    operand.type = OPERAND_TYPE::LABEL;
    operand.label = token;
    return operand;
}

// Privado:
// Verifica a gramática de label e impede que registradores ou RIP sejam reinterpretados como símbolos.
bool InstructionX86Intel::IsValidLabel(
    const std::string& token
) {
    if (token.empty() || ToLower(token) == "rip" || IsRegister(token) ||
        (!std::isalpha(static_cast<unsigned char>(token[0])) && token[0] != '_')) return false;

    for (const unsigned char c : token)
        if (!std::isalnum(c) && c != '_') return false;
    return true;
}

// Privado:
// Verifica se uma peça da expressão de memória começa por um dígito.
bool InstructionX86Intel::IsNumericPiece(
    const std::string& piece
) {
    return !piece.empty() && std::isdigit(static_cast<unsigned char>(piece[0]));
}

// Privado:
// Verifica se a peça nomeia um registrador geral de 64 bits válido como base de endereço.
bool InstructionX86Intel::IsGpr64(
    const std::string& piece
) {
    const auto& table{RegisterTable()};
    const auto it{table.find(ToLower(piece))};
    return it != table.end() && it->second.GetType() == 'L';
}

// Privado:
// Valida registrador de índice x86, excluindo rsp/r12 porque codificam ausência de índice no SIB.
bool InstructionX86Intel::IsValidMemoryIndex(
    const std::string& piece
) {
    const std::string lower{ToLower(piece)};
    return IsGpr64(lower) && lower != "rsp" && lower != "r12";
}

// Privado:
// Valida e grava o deslocamento com sinal de uma expressão de memória.
void InstructionX86Intel::SetMemoryDisplacement(
    MEMORY_OPERAND&    memory,
    const std::string& sign,
    const std::string& value
) {
    if (!IsNumericPiece(value) || (sign != "+" && sign != "-")) {
        std::cerr << "[ERRO] Deslocamento de memória inválido.\n";
        std::abort();
    }
    memory.has_displacement = true;
    memory.displacement = ParseInteger((sign == "-" ? "-" : "") + value, true);
}

// Privado:
// Valida e grava uma das escalas permitidas pelo endereçamento SIB.
void InstructionX86Intel::SetMemoryScale(
    MEMORY_OPERAND&    memory,
    const std::string& value
) {
    if (value != "1" && value != "2" && value != "4" && value != "8") {
        std::cerr << "[ERRO] Escala de índice inválida.\n";
        std::abort();
    }
    memory.scale = value[0] - '0';
}

// Privado:
// Decompõe um operando de memória nos campos de tamanho, base, índice, escala e deslocamento.
InstructionX86Intel::MEMORY_OPERAND InstructionX86Intel::ParseMemoryOperand(
    const std::string& token
) const {
    MEMORY_OPERAND memory;

    // Valida um único par de colchetes e impede qualquer conteúdo significativo após o fechamento.
    const std::size_t open{token.find('[')};
    const std::size_t close{token.find(']')};
    if (open == std::string::npos || close == std::string::npos || close < open ||
        token.find('[', open + 1) != std::string::npos || token.find(']', close + 1) != std::string::npos) {
        std::cerr << "[ERRO] Colchetes inválidos no operando de memória.\n";
        std::abort();
    }
    for (std::size_t i{close + 1}; i < token.size(); i++)
        if (token[i] != ' ' && token[i] != '\t') {
            std::cerr << "[ERRO] Conteúdo após operando de memória.\n";
            std::abort();
        }

    // Interpreta o prefixo opcional "<tamanho> ptr" antes da expressão de endereço.
    std::string prefix{token.substr(0, open)};
    while (!prefix.empty() && (prefix.back() == ' ' || prefix.back() == '\t')) prefix.pop_back();
    std::size_t prefix_begin{};
    while (prefix_begin < prefix.size() && (prefix[prefix_begin] == ' ' || prefix[prefix_begin] == '\t')) prefix_begin++;
    prefix = ToLower(prefix.substr(prefix_begin));
    if (!prefix.empty()) {
        std::vector<std::string> words;
        std::string word;
        for (const char c : prefix) {
            if (c == ' ' || c == '\t') {
                if (!word.empty()) { words.push_back(word); word.clear(); }
            } else word += c;
        }
        if (!word.empty()) words.push_back(word);
        if (words.size() != 2 || words[1] != "ptr") {
            std::cerr << "[ERRO] Qualificador de memória incompleto.\n";
            std::abort();
        }
        if      (words[0] == "byte")    memory.size = MEMORY_SIZE::BYTE;
        else if (words[0] == "word")    memory.size = MEMORY_SIZE::WORD;
        else if (words[0] == "dword")   memory.size = MEMORY_SIZE::DWORD;
        else if (words[0] == "qword")   memory.size = MEMORY_SIZE::QWORD;
        else if (words[0] == "xmmword") memory.size = MEMORY_SIZE::XMMWORD;
        else {
            std::cerr << "[ERRO] Tamanho de memória não suportado.\n";
            std::abort();
        }
    }

    // Tokeniza o interior em átomos e operadores, ignorando espaços.
    // - Isso permite reconhecer formas suportadas sem avaliar uma expressão textual genérica.
    std::vector<std::string> pieces;
    std::string atom;
    for (std::size_t i{open + 1}; i < close; i++) {
        const char c{token[i]};
        if (c == ' ' || c == '\t') {
            if (!atom.empty()) { pieces.push_back(atom); atom.clear(); }
        } else if (c == '+' || c == '-' || c == '*') {
            if (!atom.empty()) { pieces.push_back(atom); atom.clear(); }
            pieces.emplace_back(1, c);
        } else {
            atom += c;
        }
    }
    if (!atom.empty()) pieces.push_back(atom);
    if (pieces.empty()) {
        std::cerr << "[ERRO] Operando de memória vazio.\n";
        std::abort();
    }

    // RIP relativo aceita apenas deslocamento com sinal ou label somada ao RIP.
    // - RIP não vira dependência porque não é renomeado nem escrito pelo simulador.
    const std::string first{ToLower(pieces[0])};
    if (first == "rip") {
        if (pieces.size() != 3 || (pieces[1] != "+" && pieces[1] != "-")) {
            std::cerr << "[ERRO] Endereçamento relativo a RIP inválido.\n";
            std::abort();
        }
        memory.rip_relative = true;
        if (IsNumericPiece(pieces[2])) SetMemoryDisplacement(memory, pieces[1], pieces[2]);
        else {
            if (pieces[1] != "+" || !IsValidLabel(pieces[2])) {
                std::cerr << "[ERRO] Label relativa a RIP inválida.\n";
                std::abort();
            }
            memory.label = pieces[2];
        }
        return memory;
    }

    // Formas simples: [base], [deslocamento] e [-deslocamento].
    if (pieces.size() == 1 && IsGpr64(first)) {
        memory.base = first;
        return memory;
    }
    if (pieces.size() == 1 && IsNumericPiece(pieces[0])) {
        SetMemoryDisplacement(memory, "+", pieces[0]);
        return memory;
    }
    if (pieces.size() == 2 && pieces[0] == "-" && IsNumericPiece(pieces[1])) {
        SetMemoryDisplacement(memory, "-", pieces[1]);
        return memory;
    }

    // Forma sem base: [índice * escala] com deslocamento opcional.
    if (pieces.size() >= 3 && IsValidMemoryIndex(first) && pieces[1] == "*" && IsNumericPiece(pieces[2])) {
        memory.index = first;
        SetMemoryScale(memory, pieces[2]);
        if (pieces.size() == 3) return memory;
        if (pieces.size() == 5) {
            SetMemoryDisplacement(memory, pieces[3], pieces[4]);
            return memory;
        }
    }

    // Forma base mais deslocamento, positivo ou negativo.
    if (IsGpr64(first) && pieces.size() == 3 && (pieces[1] == "+" || pieces[1] == "-") && IsNumericPiece(pieces[2])) {
        memory.base = first;
        SetMemoryDisplacement(memory, pieces[1], pieces[2]);
        return memory;
    }

    // Forma SIB completa: [base + índice * escala] com deslocamento opcional quando a escala é explícita.
    if (IsGpr64(first) && pieces.size() >= 3 && pieces[1] == "+" && IsValidMemoryIndex(pieces[2])) {
        memory.base = first;
        memory.index = ToLower(pieces[2]);
        std::size_t cursor{3};
        bool explicit_scale{};
        if (cursor < pieces.size() && pieces[cursor] == "*") {
            if (cursor + 1 >= pieces.size()) {
                std::cerr << "[ERRO] Escala sem valor.\n";
                std::abort();
            }
            SetMemoryScale(memory, pieces[cursor + 1]);
            cursor += 2;
            explicit_scale = true;
        }
        if (cursor == pieces.size()) return memory;
        if (explicit_scale && cursor + 2 == pieces.size()) {
            SetMemoryDisplacement(memory, pieces[cursor], pieces[cursor + 1]);
            return memory;
        }
    }

    // Rejeita formas ambíguas ou não modeladas em vez de inferir semântica diferente do simulador.
    std::cerr << "[ERRO] Expressão de memória fora da gramática suportada.\n";
    std::abort();
}

// Privado:
bool InstructionX86Intel::SetStages(const std::vector<std::string>& tokens) {
    const std::string op{ToLower(tokens[0])};
    INSTRUCTION_TYPE instruction_type{INSTRUCTION_TYPE::INVALID};

    // Analisa os operandos uma única vez e guarda a estrutura usada nas etapas posteriores do Parse().
    // - Validação, normalização e montagem das etapas compartilham exatamente essa interpretação.
    operands.clear();
    for (std::size_t i{1}; i < tokens.size(); i++) operands.push_back(ParseOperand(tokens[i]));

    // MOV: polimórfico (LOAD, STORE ou cópia registrador-registrador/imediato).
    if (ContainsOpcode(MOVS, op)) {
        if (tokens.size() != 3) return false;
        if (operands[0].type == OPERAND_TYPE::MEMORY) {
            instruction_type = INSTRUCTION_TYPE::STORE;
        } else if (operands[1].type == OPERAND_TYPE::MEMORY) {
            instruction_type = INSTRUCTION_TYPE::LOAD;
        } else {
        // Sem memória envolvida:
        // - O tipo depende de QUAL registrador, não do nome do opcode.
            instruction_type =
                ((operands[0].type == OPERAND_TYPE::REGISTER && IsFloatReg(operands[0].register_name)) ||
                 (operands[1].type == OPERAND_TYPE::REGISTER && IsFloatReg(operands[1].register_name)))
                ? INSTRUCTION_TYPE::FLOAT_BASIC
                : INSTRUCTION_TYPE::INT_BASIC;
        }
    } else if (ContainsOpcode(MOVSX_MOVZX, op)) {
        // MOVSX/MOVZX: LOAD quando a fonte é memória; INT_BASIC quando é registrador.
        if (tokens.size() != 3) return false;
        instruction_type = operands[1].type == OPERAND_TYPE::MEMORY
                           ? INSTRUCTION_TYPE::LOAD
                           : INSTRUCTION_TYPE::INT_BASIC;
    } else if (ContainsOpcode(INT_BASIC, op)) {
        instruction_type = INSTRUCTION_TYPE::INT_BASIC;
    } else if (ContainsOpcode(BRANCHES, op)) {
        instruction_type = INSTRUCTION_TYPE::BRANCH;
    } else if (ContainsOpcode(INT_MUL, op)) {
        instruction_type = INSTRUCTION_TYPE::INT_MUL;
    } else if (ContainsOpcode(INT_DIV, op)) {
        instruction_type = INSTRUCTION_TYPE::INT_DIV;
    } else if (ContainsOpcode(FLOAT_BASIC, op)) {
        instruction_type = INSTRUCTION_TYPE::FLOAT_BASIC;
    } else if (ContainsOpcode(FLOAT_MUL, op)) {
        instruction_type = INSTRUCTION_TYPE::FLOAT_MUL;
    } else if (ContainsOpcode(FLOAT_DIV, op)) {
        instruction_type = INSTRUCTION_TYPE::FLOAT_DIV;
    } else {
        return false;
    }

    ValidateInstruction(tokens, instruction_type);

    std::vector<Register> ex_sources;
    std::vector<Register> mem_sources;
    SetStageAttributes(tokens, instruction_type, ex_sources, mem_sources);
    AddStage(instruction_type, ex_sources, mem_sources);
    return true;
}

// Privado:
void InstructionX86Intel::ValidateInstruction(
    const std::vector<std::string>& tokens,
    const INSTRUCTION_TYPE          instruction_type
) {
    const std::string op{ToLower(tokens[0])};

    // SetStages() já restringiu a família; cada ramo confirma quantidade, posição e classe dos operandos.
    // - A validação aceita somente formas cuja semântica de dependências o simulador consegue representar.
    switch (instruction_type) {
        case INSTRUCTION_TYPE::LOAD: {
            // LOAD sempre move memória para um registrador inteiro ou XMM.
            if (tokens.size() == 3 && operands[0].type == OPERAND_TYPE::REGISTER &&
                (IsIntReg(operands[0].register_name) || IsFloatReg(operands[0].register_name)) &&
                operands[1].type == OPERAND_TYPE::MEMORY) return;
            break;
        }
        case INSTRUCTION_TYPE::STORE: {
            // STORE sempre move registrador/imediato para memória.
            if (tokens.size() == 3 && operands[0].type == OPERAND_TYPE::MEMORY &&
                ((operands[1].type == OPERAND_TYPE::REGISTER &&
                  (IsIntReg(operands[1].register_name) || IsFloatReg(operands[1].register_name))) ||
                 operands[1].type == OPERAND_TYPE::IMMEDIATE)) return;
            break;
        }
        case INSTRUCTION_TYPE::BRANCH: {
            // Saltos incondicionais e calls podem ser diretos ou indiretos; JCCs permanecem label-only.
            if (op == "jmp" || op == "call") {
                if (tokens.size() == 2 &&
                    (operands[0].type == OPERAND_TYPE::LABEL ||
                     (operands[0].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name)) ||
                     operands[0].type == OPERAND_TYPE::MEMORY)) return;
            } else if (op == "ret") {
                if (tokens.size() == 1) return;
            } else {
                // JCC (condicionais).
                if (tokens.size() == 2 && operands[0].type == OPERAND_TYPE::LABEL) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_BASIC: {
            // Separa formas unárias e binárias porque cada grupo posiciona fontes e destinos de modo diferente.
            if (op == "push") {
                // Restrito a IREG/IMM: "push [mem]" exigiria ler memória E
                // escrever na pilha na MESMA instrução, fora do par LOAD/STORE
                // puro usado no resto do simulador. Não documentado no
                // _sintax.txt original — mantido fora do escopo.
                if (tokens.size() == 2 &&
                    ((operands[0].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name)) ||
                     operands[0].type == OPERAND_TYPE::IMMEDIATE)) return;
            } else if (op == "pop") {
                if (tokens.size() == 2 && operands[0].type == OPERAND_TYPE::REGISTER &&
                    IsIntReg(operands[0].register_name)) return;
            } else if (op == "inc" || op == "dec" || op == "not" || op == "neg") {
                if (tokens.size() == 2 &&
                    ((operands[0].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name)) ||
                     operands[0].type == OPERAND_TYPE::MEMORY)) return;
            } else if (op == "lea") {
                // "lea" SEMPRE escreve num registrador (nunca memória) e SEMPRE lê
                // um endereço entre colchetes (nunca um registrador/imediato
                // direto) — sintaxe estruturalmente diferente do resto de
                // INT_BASIC. A versão anterior caía no ramo genérico abaixo, que
                // aceitaria por engano algo como "lea eax, ebx".
                if (tokens.size() == 3 && operands[0].type == OPERAND_TYPE::REGISTER &&
                    IsIntReg(operands[0].register_name) && operands[1].type == OPERAND_TYPE::MEMORY) return;
            } else {
                // Binárias genéricas (add, sub, mov, movsx/movzx com fonte
                // registrador, etc.)
                if (tokens.size() == 3 &&
                    ((operands[0].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name)) ||
                     operands[0].type == OPERAND_TYPE::MEMORY) &&
                    ((operands[1].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[1].register_name)) ||
                     operands[1].type == OPERAND_TYPE::IMMEDIATE || operands[1].type == OPERAND_TYPE::MEMORY)) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_MUL: {
            // mul usa um operando explícito; imul modelado usa destino explícito e uma segunda fonte.
            if (op == "mul") {
                if (tokens.size() == 2 &&
                    ((operands[0].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name)) ||
                     operands[0].type == OPERAND_TYPE::MEMORY)) return;
            } else if (op == "imul") {
                if (tokens.size() == 3 && operands[0].type == OPERAND_TYPE::REGISTER &&
                    IsIntReg(operands[0].register_name) &&
                    ((operands[1].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[1].register_name)) ||
                     operands[1].type == OPERAND_TYPE::MEMORY || operands[1].type == OPERAND_TYPE::IMMEDIATE)) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_DIV: {
            // div/idiv recebem apenas o divisor explícito; o dividendo e o resultado usam registradores implícitos.
            if ((op == "div" || op == "idiv") && tokens.size() == 2 &&
                ((operands[0].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name)) ||
                 operands[0].type == OPERAND_TYPE::MEMORY)) return;
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_BASIC:
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV: {
            // Operações SSE usam destino XMM, exceto conversões cujo resultado é inteiro.
            if (op == "cvttss2si") {
                // Único opcode do grupo com destino INTEIRO / fonte FLOAT — o
                // resto do grupo é sempre destino float. A checagem genérica
                // abaixo (IsFloatReg no destino) rejeitava essa instrução por
                // engano nas duas versões anteriores.
                if (tokens.size() == 3 && operands[0].type == OPERAND_TYPE::REGISTER &&
                    operands[1].type == OPERAND_TYPE::REGISTER && IsIntReg(operands[0].register_name) &&
                    IsFloatReg(operands[1].register_name)) return;
            } else {
                if (tokens.size() == 3 && operands[0].type == OPERAND_TYPE::REGISTER &&
                    IsFloatReg(operands[0].register_name) &&
                    ((operands[1].type == OPERAND_TYPE::REGISTER &&
                      (IsFloatReg(operands[1].register_name) || IsIntReg(operands[1].register_name))) ||
                     operands[1].type == OPERAND_TYPE::MEMORY)) return;
            }
            break;
        }
        default: break;
    }

    std::cerr << "[ERRO] Sintaxe inválida para a instrução:\n- Instrução: ";
    for (size_t i{}; i < tokens.size(); i++) std::cerr << tokens[i] << ' ';
    std::cerr << '\n';
    std::abort();
}

// Privado:
void InstructionX86Intel::NormalizeInstruction(std::vector<std::string>& tokens) {
    // Reconstrói a instrução canônica sem modificar labels case-sensitive.
    const std::string op{ToLower(tokens[0])};
    instruction_string = op;

    // Padroniza o espaço entre o OpCode e o primeiro elemento (facilita o debugging).
    for (size_t i{op.size()}; i < biggest_instruction; i++) instruction_string += " ";

    for (std::size_t i{}; i < operands.size(); i++)
        instruction_string += (i == 0 ? "" : ", ") + NormalizeOperand(operands[i]);
}

// Privado:
// Converte a estrutura de um operando para a forma textual canônica usada na saída do simulador.
std::string InstructionX86Intel::NormalizeOperand(const OPERAND& operand) const {
    // Operandos escalares já foram validados e normalizados durante o parsing.
    if (operand.type == OPERAND_TYPE::REGISTER) return operand.register_name;
    if (operand.type == OPERAND_TYPE::IMMEDIATE) return operand.immediate.normalized;
    if (operand.type == OPERAND_TYPE::LABEL) return operand.label;

    const MEMORY_OPERAND& memory{operand.memory};
    std::string normalized;

    // Recria o qualificador opcional de tamanho antes dos colchetes.
    switch (memory.size) {
        case MEMORY_SIZE::BYTE:    normalized = "byte ptr "; break;
        case MEMORY_SIZE::WORD:    normalized = "word ptr "; break;
        case MEMORY_SIZE::DWORD:   normalized = "dword ptr "; break;
        case MEMORY_SIZE::QWORD:   normalized = "qword ptr "; break;
        case MEMORY_SIZE::XMMWORD: normalized = "xmmword ptr "; break;
        default: break;
    }
    normalized += '[';

    // Emite primeiro a base implícita RIP ou a base geral e depois o índice escalado.
    bool has_component{};
    if (memory.rip_relative) {
        normalized += "rip";
        has_component = true;
    } else if (!memory.base.empty()) {
        normalized += memory.base;
        has_component = true;
    }
    if (!memory.index.empty()) {
        if (has_component) normalized += " + ";
        normalized += memory.index;
        if (memory.scale != 1 || memory.base.empty() || memory.has_displacement)
            normalized += " * " + std::to_string(memory.scale);
        has_component = true;
    }

    // Label relativa ou deslocamento fecham a expressão com sinal e espaçamento padronizados.
    if (!memory.label.empty()) {
        normalized += " + " + memory.label;
    } else if (memory.has_displacement) {
        const std::string magnitude{memory.displacement.normalized[0] == '-'
                                    ? memory.displacement.normalized.substr(1)
                                    : memory.displacement.normalized};
        if (!has_component) normalized += memory.displacement.normalized;
        else normalized += memory.displacement.negative ? " - " + magnitude : " + " + magnitude;
    }
    return normalized + ']';
}

// Privado:
// Adiciona as dependências explícitas de um registrador ou dos componentes que calculam um endereço.
void InstructionX86Intel::PushOperandSources(
    std::vector<Register>& sources,
    const OPERAND&         operand
) const {
    // Imediatos, labels e RIP não dependem de um valor produzido por outra instrução.
    if (operand.type == OPERAND_TYPE::REGISTER) {
        PushWithMasked(sources, LookupReg(operand.register_name));
    } else if (operand.type == OPERAND_TYPE::MEMORY) {
        if (!operand.memory.base.empty()) PushWithMasked(sources, LookupReg(operand.memory.base));
        if (!operand.memory.index.empty()) PushWithMasked(sources, LookupReg(operand.memory.index));
    }
}

// Privado:
void InstructionX86Intel::SetStageAttributes(
    const std::vector<std::string>& tokens,
    const INSTRUCTION_TYPE          instruction_type,
    std::vector<Register>&          ex_sources,
    std::vector<Register>&          mem_sources
) {
    const std::string op{ToLower(tokens[0])};

    // Traduz a semântica x86 para dependências do pipeline:
    // - destinos reservam o CDB; fontes EX precisam estar prontas para executar;
    // - fontes MEM são valores consumidos somente no acesso à memória;
    // - PushWithMasked() inclui aliases sobrepostos do mesmo registrador físico.
    switch (instruction_type) {
        case INSTRUCTION_TYPE::LOAD: {
            // O endereço é fonte de EX; o dado carregado sobrescreve o registrador de destino.
            PushWithMasked(dest_registers, LookupReg(operands[0].register_name));
            PushOperandSources(ex_sources, operands[1]);
            break;
        }
        case INSTRUCTION_TYPE::STORE: {
            // O endereço é calculado em EX, mas o valor armazenado só é consumido em MEM.
            PushOperandSources(ex_sources, operands[0]);
            PushOperandSources(mem_sources, operands[1]);
            break;
        }
        case INSTRUCTION_TYPE::BRANCH: {
            if (op == "call" || op == "ret") {
                // call/ret atualizam a pilha e dependem do valor anterior de rsp.
                PushWithMasked(dest_registers,      LookupReg("rsp"));
                PushWithMasked(ex_sources, LookupReg("rsp"));
            } else if (op != "jmp") {
                // Task 05 refinará cada JCC para seu subconjunto exato.
                PushAllFlags(ex_sources);
            }
            // jmp/call indiretos ("jmp rax", "call [rbx+4]"):
            // - O alvo precisa estar pronto antes do desvio, então vira fonte.
            // - Labels não geram fonte (endereço resolvido estaticamente).
            if ((op == "jmp" || op == "call") && !operands.empty())
                PushOperandSources(ex_sources, operands[0]);
            break;
        }
        case INSTRUCTION_TYPE::INT_BASIC: {
            if (op == "push") {
                // push atualiza rsp em EX e consome o valor enviado à pilha em MEM.
                PushWithMasked(dest_registers,      LookupReg("rsp"));
                PushWithMasked(ex_sources, LookupReg("rsp"));
                PushOperandSources(mem_sources, operands[0]); // Salva na RAM.
            } else if (op == "pop") {
                // pop lê e atualiza rsp, além de escrever o registrador que recebe o valor da memória.
                PushWithMasked(dest_registers,      LookupReg("rsp"));
                PushWithMasked(ex_sources, LookupReg("rsp"));
                PushWithMasked(dest_registers,      LookupReg(operands[0].register_name)); // Puxa da RAM.
            } else if (op == "cmp" || op == "test") {
                // Comparações leem ambos os operandos e escrevem somente flags.
                PushAllFlags(dest_registers);
                PushOperandSources(ex_sources, operands[0]);
                PushOperandSources(ex_sources, operands[1]);
            } else if (op == "lea") {
                // lea não acessa memória de fato (só calcula o endereço) e não
                // mexe nas flags rastreadas.
                PushWithMasked(dest_registers, LookupReg(operands[0].register_name));
                PushOperandSources(ex_sources, operands[1]);
            } else {
                // add, sub, and, or, xor, shl/shr/sar/sal, rol/ror, adc, sbb,
                // inc, dec, not, neg, mov, movsx, movzx (fonte registrador).
                const bool reads_dest = !ContainsOpcode(MOVS, op) && !ContainsOpcode(PURE_WRITE, op);

                if (operands[0].type == OPERAND_TYPE::REGISTER) {
                    PushWithMasked(dest_registers, LookupReg(operands[0].register_name));
                    if (reads_dest) PushOperandSources(ex_sources, operands[0]);
                } else if (operands[0].type == OPERAND_TYPE::MEMORY) {
                    PushOperandSources(ex_sources, operands[0]);
                }

                // Fonte (operando 2).
                if (operands.size() > 1) PushOperandSources(ex_sources, operands[1]);

                // Task 05 refinará cada opcode para seu subconjunto exato.
                if (!ContainsOpcode(MOVS, op) && !ContainsOpcode(NO_FLAGS, op))
                    PushAllFlags(dest_registers);

                // ADC e SBB também leem a flag (carry-in).
                if (op == "adc" || op == "sbb")
                    PushWithMasked(ex_sources, LookupReg("cf"));
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV: {
            if (op == "mul" || op == "div" || op == "idiv") {
                // Par implícito AX (8-bit) / AX:DX / EAX:EDX / RAX:RDX.
                // - A largura é a do operando explícito.
                // - A sintaxe não modela sufixo de tamanho (dword/qword) para operando de memória:
                // "mul [rbx]" assume 32-bit por padrão (simplificação assumida).
                const char operand_width{operands[0].type == OPERAND_TYPE::MEMORY
                                         ? 'R'
                                         : LookupReg(operands[0].register_name).GetType()};
                const bool is_8bit{operand_width == 'B'};
                // 8-bit não tem "par" separado: o resultado inteiro cabe em AX (16-bit), não em AL.
                // - Por isso o 8-bit colapsa para 'W' aqui.
                const char pair_width{(operand_width == 'R' || operand_width == 'L') ? operand_width : 'W'};
                const int  pair_mask{(pair_width == 'L') ? 0xFF : (pair_width == 'R') ? 0x0F : 0x03};

                PushWithMasked(dest_registers,      Register(pair_width, 0, pair_mask)); // ax/eax/rax
                PushWithMasked(ex_sources, Register(pair_width, 0, pair_mask));
                if (!is_8bit) {
                    // id 3 = família rdx/edx/dx (ver ordem em RegisterTable()).
                    PushWithMasked(dest_registers, Register(pair_width, 3, pair_mask)); // dx/edx/rdx
                    if (op != "mul") // mul só LÊ eax; div/idiv leem o par completo (dividendo).
                        PushWithMasked(ex_sources, Register(pair_width, 3, pair_mask));
                }

                PushOperandSources(ex_sources, operands[0]);
                PushAllFlags(dest_registers);
            } else if (op == "imul") {
                PushWithMasked(dest_registers, LookupReg(operands[0].register_name));
                PushOperandSources(ex_sources, operands[0]); // Destino também é fonte (x86 CISC).
                PushOperandSources(ex_sources, operands[1]);
                PushAllFlags(dest_registers);
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_BASIC:
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV: {
            if (op == "comiss" || op == "ucomiss") {
                // Comparações escalares leem operandos XMM e produzem somente flags.
                PushAllFlags(dest_registers);
                PushOperandSources(ex_sources, operands[0]);
                PushOperandSources(ex_sources, operands[1]);
            } else if (op == "cvttss2si") {
                // Destino inteiro, escrita pura (não lê o destino antigo).
                PushWithMasked(dest_registers, LookupReg(operands[0].register_name));
                PushOperandSources(ex_sources, operands[1]);
            } else {
                // Operações de 2 operandos FPU/SSE (addss, mulss, movaps quando
                // cai aqui, sqrtss, cvtsi2ss, ...).
                const bool reads_dest = !ContainsOpcode(MOVS, op) && !ContainsOpcode(PURE_WRITE, op);

                PushWithMasked(dest_registers, LookupReg(operands[0].register_name));
                if (reads_dest) PushOperandSources(ex_sources, operands[0]);
                PushOperandSources(ex_sources, operands[1]);
            }
            break;
        }
        default: break;
    }
}

} // namespace processor
