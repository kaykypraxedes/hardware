/* Architectures/X86Intel.cpp */
#include "headers/X86Intel.h"
#include <string>
#include <vector>

namespace processor {

static Register LookupRegister(
    const std::string& name,
    const std::string& instruction, // Puramente para debugging em caso de erro (não é obrigatório para o funcionamento).
    const std::unordered_map<std::string, Register>& table
){
    auto it{table.find(name)};

    // Verifica se o registrador está dentro da tabela:
    // - O método "find()" retorna "end()" se não encontra o elemento.
    if (it == table.end()) {
        std::cerr <<
            "[ERRO] Registrador inválido: \n" <<
            "- Nome: " << name << '\n' <<
            "- Instrução: " << instruction << '\n';
        std::abort();
    }
    return it->second;
}

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int biggest_instruction{9};

// Opcodes da arquitetura:
static const std::vector<std::string> MOVS // Caso especial, já que pode agir como load, store e aritimético (com reg e com imediato).
    {"mov", "movss", "movsd", "movaps", "movups", "movq", "movd"};

static const std::vector<std::string> INT_BASIC
    {"add", "sub", "and", "or", "xor", "inc", "dec", "cmp", "shl", "shr", "not", "neg", "test", "rol", "ror", "sar", "sal", "sbb", "adc", "movsx", "movzx", "lea", "push", "pop"};

static const std::vector<std::string> INT_MUL
    {"imul", "mul"};

static const std::vector<std::string> INT_DIV
    {"idiv", "div"};

static const std::vector<std::string> BRANCHES
    {"jmp", "je", "jne", "jg", "jge", "jl", "jle", "call", "jbe", "ja", "jae", "jb", "js", "jns", "jp", "jo", "ret"};

// Desvios condicionais (única categoria de BRANCH que lê EFLAGS):
static const std::vector<std::string> JCC
    {"je", "jne", "jg", "jge", "jl", "jle", "jbe", "ja", "jae", "jb", "js", "jns", "jp", "jo"};

static const std::vector<std::string> FLOAT_BASIC
    {"addss", "addsd", "subss", "subsd", "sqrtss", "addps", "subps", "mulps", "divps", "cvtsi2ss", "cvttss2si", "comiss", "ucomiss", "pxor", "pand", "por"};

static const std::vector<std::string> FLOAT_MUL
    {"mulss", "mulsd"};

static const std::vector<std::string> FLOAT_DIV
    {"divss", "divsd"};

// Helpers dos OpCodes para identificar comportamentos específicos de maneira faciltada:

// - Opcodes de escrita pura: copiam/convertem sem ler o destino antigo.
static const std::vector<std::string> PURE_WRITE
    {"movsx", "movzx", "cvtsi2ss", "cvttss2si"};

// - Opcodes que NÃO escrevem no 'eflags' no x86 real (os demais do 'else' genérico setam a flag):
static const std::vector<std::string> NO_FLAGS_COPY
    {"movsx", "movzx", "not", "cvtsi2ss", "cvttss2si", "addss", "addsd", "subss", "subsd", "sqrtss", "addps", "subps", "mulps", "divps", "mulss", "mulsd", "divss", "divsd", "pxor", "pand", "por"};


// Verifica se é uma cópia de registrador/imediato da família MOV (sem memória).
static bool IsMOVCopy(const std::vector<std::string>& tokens) {
    return ContainsOpcode(MOVS, tokens[0]) &&
           !(tokens.size() > 1 && tokens[1].front() == '[') &&
           !(tokens.size() > 2 && tokens[2].front() == '[');
}

// Identificação do tipo da família MOV (caso especial, já que pode ser LOAD, STORE ou cópia reg/reg).
static INSTRUCTION_TYPE IdentifyMOVType(const std::vector<std::string>& tokens) {
    if (tokens.size() > 1 && tokens[1].front() == '[') return INSTRUCTION_TYPE::STORE;
    if (tokens.size() > 2 && tokens[2].front() == '[') return INSTRUCTION_TYPE::LOAD;
    return (tokens[0] == "mov" ? INSTRUCTION_TYPE::INT_BASIC : INSTRUCTION_TYPE::FLOAT_BASIC);
}

// Recebe o registrador alvo para procurar dependências de hardware:
// - ax (16 bits) contém o al (8 bits de baixo) e ah (8 bits de cima) por exemplo.
// - Quando o ax é usado, ele bloqueia al e ah;
// - Quando ah é usado, al ainda pode ser usado (ax não).
// - Simplificação: registrador grande = número binário; registradores menores = pedaços dele.
// - Mesmo id + máscaras sobrepostas = mesmo espaço de hardware (bloqueiam-se mutuamente).
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
// - ex.: mov al, bl -> dests {al, ax, eax, rax} (al + os que compartilham hardware).
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

static void PushPiece(
    std::vector<Register>& sources,
    std::string&           piece,
    const std::string&     context
){
    // Só registradores da tabela viram fonte:
    // - Imediatos ("4", "0x10") ficam de fora (não estão na tabela).
    // - Labels ("var" em "mov eax, [var]") também — endereço de link-time,
    //   não registrador (antes isso abortava no LookupRegister).
    if (!IsRegister(piece, RegisterTable())) return;

    // 'rip' é registrador implícito e nunca é reescrito por instrução: a
    // dependência estaria sempre pronta, então não é rastreado como fonte.
    // - Check defensivo: "rip" não está na tabela hoje, mas se um dia entrar,
    //   continua fora das fontes.
    if (piece == "rip") return;

    PushWithMasked(sources, LookupRegister(piece, context, RegisterTable()));
}

// Adiciona todos os registradores de um operando às fontes:
// - Registrador direto ("ebx") -> ebx.
// - Memória ("[rbx+4]", "[rbx+rcx]", "[rax+rbx*4+8]") -> cada registrador vira fonte.
// - Imediato ("5", "0x10", "-4") -> nada.
static void PushOperandSources(
    std::vector<Register>& sources,
    const std::string&     token,
    const std::string&     context
){
    std::string name{token};
    if (name.empty()) return;

    // A verificação se a delimitação é válida é realizada em 'ValidateInstruction()'.
    if (name.front() == '[') {

        // Remove os colchetes.
        name = name.substr(1, name.size() - 2);

        std::string piece;
        for (char c : name) {
            if (c == ' ' || c == '\t' || c == '+' || c == '-' || c == '*'){
                // Ignora espaços repetidos.
                if(!piece.empty()) {
                    PushPiece(sources, piece, context); // Passa o valor.
                    piece.clear();                      // Limpa a string para comportar mais pieces.
                }
            }
            else piece += c;
        }
        // Pega o último piece capturado (quando termina, faz só o piece += c).
        // - Verifica se está vazio, pois pode ser enviado algo como "mov rax rbx " (espaço no final).
        if(!piece.empty()) PushPiece(sources, piece, context);
        return;
    }

    // Se forem apenas imediatos, ou labels em endereço direto ("mov eax, var"),
    // não opera: só registradores da tabela viram fonte.
    if (!IsRegister(name, RegisterTable())) return;

    // Adiciona os registradores (e seus aliases) como sources.
    PushWithMasked(sources, LookupRegister(name, context, RegisterTable()));
}

// Função de Instruction.h:
// - Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Alias = (id da família, máscara do trecho de bits que ele cobre):
    // - ids 0-15:  RAX..R15 ('L', 0xFF) > EAX..R15D ('R', 0x0F) > AX..R15W ('W', 0x03) > AL/AH..R15B ('B', 0x01/0x02).
    // - ids 64-79: XMM0-15 ('V', 0xFF).
    // - id 80:     EFLAGS ('G', 0xFF).
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).

        // Int (0-15):
        // 64-bit (L, 0-15).
        const char* l64[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp"};
        for (int i = 0; i < 8;  i++){
            // Máscara de 8 bits (0xFF = 11111111).
            t.emplace(l64[i], Register('L', i, 0xFF));
            t.emplace("r" + std::to_string(8 + i), Register('L', 8 + i, 0xFF));
        }
        // 32-bit (R, 0-15).
        const char* r32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp"};
        for (int i = 0; i < 8;  i++){
            // Máscara de 4 bits (0x0F = 00001111).
            t.emplace(r32[i], Register('R', i, 0x0F));
            t.emplace("r" + std::to_string(8 + i) + "d", Register('R', 8 + i, 0x0F));
        }
        // 16-bit (W, 0-15).
        const char* w16[] = {"ax", "bx", "cx", "dx", "si", "di", "sp", "bp"};
        for (int i = 0; i < 8;  i++){
            // Máscara de 2 bits (0x03 = 00000011).
            t.emplace(w16[i], Register('W', i, 0x03));
            t.emplace("r" + std::to_string(8 + i) + "w", Register('W', 8 + i, 0x03));
        }
        // 8-bit (B, 0-15).
        const char* b4sup[] = {"al", "bl", "cl", "dl", "sil", "dil", "spl", "bpl"};
        // Máscara de 1 bits (0x01 = 00000001).
        for (int i = 0; i < 8; i++) t.emplace(b4sup[i], Register('B', i, 0x01));
        // Máscara de 1 bits (0x02 = 0000010).
        const char* b4inf[] = {"ah", "bh", "ch", "dh"};
        for (int i = 0; i < 4; i++) t.emplace(b4inf[i], Register('B', i, 0x02));
        // Máscara de 1 bits (0x01 = 00000001).
        for (int i = 8; i < 16; i++) t.emplace("r" + std::to_string(i) + "b", Register('B', i, 0x01));

        // Vetorial (64-79):
        for (int i = 0; i < 16; i++) t.emplace("xmm" + std::to_string(i), Register('V', 64 + i, 0xFF));

        // Flags (80):
        t.emplace("eflags", Register('G', 80, 0xFF));
        // - 'rip' NÃO entra na tabela de propósito (registrador implícito,
        //   tratado como "sempre pronto" em PushPiece).
    }

    return t;
}

// Monta o CDB com os registradores físicos:
// - Layout por variante (um slot por (id, mask)), contíguo por variante:
// - ids 0-15:  Faixas int (L/R/W/B), máscaras 0xFF/0x0F/0x03/0x01 (0x02 p/ ah-bh-ch-dh).
// - ids 64-79: 'V' (xmm),    0xFF.
// - id 80:     'G' (eflags), 0xFF.
CDB InstructionX86Intel::MakeCDB() {
    CDB cdb;
    // Layout contíguo por variante:
    FillCDB(cdb, 'L', 0,  16, 0xFF);
    FillCDB(cdb, 'R', 0,  16, 0x0F);
    FillCDB(cdb, 'W', 0,  16, 0x03);
    FillCDB(cdb, 'B', 0,  16, 0x01);
    FillCDB(cdb, 'B', 0,  4,  0x02); // ah/bh/ch/dh
    FillCDB(cdb, 'V', 64, 16, 0xFF);
    FillCDB(cdb, 'G', 80, 1,  0xFF);

    cdb.print_banks = {
        {'L', 0,  16},
        {'R', 16, 16},
        {'W', 32, 16},
        {'B', 48, 16},
        {'B', 64, 4},
        {'V', 68, 16},
        {'G', 84, 1}
    };
    return cdb;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionX86Intel::InstructionX86Intel(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionX86Intel::IdentifyType(
    const std::vector<std::string>& tokens
){
    std::string op{tokens[0]};
    for (char& c : op) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ContainsOpcode(MOVS, op))             type = IdentifyMOVType(tokens);
    else if (ContainsOpcode(INT_BASIC, op))   type = INSTRUCTION_TYPE::INT_BASIC;
    else if (ContainsOpcode(BRANCHES, op))    type = INSTRUCTION_TYPE::BRANCH;
    else if (ContainsOpcode(INT_MUL, op))     type = INSTRUCTION_TYPE::INT_MUL;
    else if (ContainsOpcode(INT_DIV, op))     type = INSTRUCTION_TYPE::INT_DIV;
    else if (ContainsOpcode(FLOAT_BASIC, op)) type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (ContainsOpcode(FLOAT_MUL, op))   type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (ContainsOpcode(FLOAT_DIV, op))   type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    return true;
}

// Privado:
// Separa a instrução nos seus componentes (OpCode + reg/label/imediato ...).
std::vector<std::string> InstructionX86Intel::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;
    bool in_bracket = false;

    for (char c : str) {
        if (c == '[') in_bracket = true;
        if (c == ']') in_bracket = false;

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
    // - Verifica se está vazio, pois pode ser enviado algo como "mov rax rbx " (espaço no final).
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
// Padroniza os opcodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionX86Intel::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Labels de desvio são case-sensitive: operando de desvio nunca vira fonte.
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

    for (size_t i = 1; i < tokens.size(); ++i)
        normalized += (i == 1 ? "" : ", ") + tokens[i];

    instruction_string = normalized;
}

// Privado:
// Não foi usado o padrão de número de tokens porque as instruções em sua maioria são polimórficas.
// - Uma instrução pode ter múltiplos atributos e ser de múltiplos tipos, gerando múltiplos testes para a mesma instrução.
void InstructionX86Intel::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    ex_source_registers.clear();
    mem_source_registers.clear();

    // No x86 Intel, o primeiro operando costuma ser o Destino/Fonte (ex: add eax ebx -> eax = eax + ebx)
    if (type == INSTRUCTION_TYPE::BRANCH) {
        // "je end" (e demais JCC):
        // - Fonte   = EFLAGS ('G', 80).
        // - Sem destino.
        if (ContainsOpcode(JCC, tokens[0]))
            ex_source_registers.push_back(Register('G', 80));

        // "call foo" / "ret":
        // - Fonte   = rsp implícito ('L', 6).
        // - Destino = rsp implícito ('L', 6).
        if (tokens[0] == "call" || tokens[0] == "ret") {
            PushWithMasked(dest_registers,   Register('L', 6));
            PushWithMasked(ex_source_registers, Register('L', 6));
        }

        // "jmp rax" / "call [rbx]" (indiretos):
        // - Fonte = tokens[1], se for colchetes ou registrador (labels não).
        // - Sem destino.
        if (tokens.size() > 1) {
            std::string op{tokens[1]};
            for (char& c : op) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (op.front() == '[' || IsRegister(op, RegisterTable()))
                PushOperandSources(ex_source_registers, tokens[1], instruction_string);
        }
    }
    // LOAD - "mov eax, [ebx+4]":
    // - Fontes   = registradores do endereço (tokens[2]);
    // - Destino  = tokens[1];
    else if (type == INSTRUCTION_TYPE::LOAD) {
        if (tokens.size() < 3)  {
            std::cerr << "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        PushOperandSources(ex_source_registers, tokens[2], instruction_string);
    }
    // STORE - "mov [ebx+4], eax":
    // - Fontes   = tokens[2] (dado) e registradores do endereço (tokens[1]);
    // - Sem destino.
    else if (type == INSTRUCTION_TYPE::STORE) {
        if (tokens.size() < 3) {
            std::cerr << "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        PushWithMasked(mem_source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
        PushOperandSources(ex_source_registers, tokens[1], instruction_string);
    }
    // "cmp eax, ebx" / "test" / "comiss" / "ucomiss":
    // - Fontes   = tokens[1] (e tokens[2], quando houver), reg ou memória;
    // - Destino  = EFLAGS ('G', 80).
    else if (tokens[0] == "cmp" || tokens[0] == "test" ||
               tokens[0] == "comiss" || tokens[0] == "ucomiss") {
        if (tokens.size() < 2) {
            std::cerr << "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        PushWithMasked(dest_registers, Register('G', 80));
        PushOperandSources(ex_source_registers, tokens[1], instruction_string);
        if (tokens.size() > 2)
            PushOperandSources(ex_source_registers, tokens[2], instruction_string);
    }
    else if (tokens[0] == "div" || tokens[0] == "idiv" || tokens[0] == "mul" ||
               (tokens[0] == "imul" && tokens.size() == 2)) {
        // "div ebx" (idiv/mul; imul de 1 operando também):
        // - Fontes   = tokens[1] (divisor/multiplicador) e o par implícito
        //              eax:edx (ax:dx ou rax:rdx; só ax em 8 bits);
        // - Destinos = o par implícito eax:edx (quociente/resto/produto);
        // - O operando explícito nunca é destino.
        if (tokens.size() < 2) {
            std::cerr << "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        PushOperandSources(ex_source_registers, tokens[1], instruction_string);

        // Resolve a largura do par implícito a partir da classe do operando:
        // - 'B' (8 bits)  -> só ax;
        // - 'W' (16 bits) -> ax:dx;
        // - 'R' (32 bits) -> eax:edx;
        // - 'L' (64 bits) -> rax:rdx.
        const char cls = LookupRegister(tokens[1], instruction_string, RegisterTable()).GetType();
        const char pair_type = (cls == 'R' || cls == 'L') ? cls : 'W';
        const int  pair_mask = (pair_type == 'R') ? 0x0F : (pair_type == 'L') ? 0xFF : 0x03;
        const bool is_8bit   = (cls == 'B');

        PushWithMasked(dest_registers,   Register(pair_type, 0, pair_mask));
        PushWithMasked(ex_source_registers, Register(pair_type, 0, pair_mask));
        if (!is_8bit) {
            PushWithMasked(dest_registers,   Register(pair_type, 2, pair_mask));
            PushWithMasked(ex_source_registers, Register(pair_type, 2, pair_mask));
        }
    }
    // "push ebx":
    // - Fontes   = tokens[1] (reg/mem/imm) e rsp implícito ('L', 6).
    // - Destino  = rsp implícito ('L', 6).
    // "pop ebx":
    // - Fontes   = rsp implícito ('L', 6) (ou tokens[1], se pop [mem]).
    // - Destinos = tokens[1] e rsp implícito ('L', 6).
    else if (tokens[0] == "push" || tokens[0] == "pop") {
        if (tokens.size() < 2) {
            std::cerr <<
            "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        if (tokens[0] == "push") {
            PushOperandSources(ex_source_registers, tokens[1], instruction_string);
        } else if (tokens[1].front() == '[') {
            PushOperandSources(ex_source_registers, tokens[1], instruction_string);
        } else {
            PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        }
        PushWithMasked(dest_registers,   Register('L', 6));
        PushWithMasked(ex_source_registers, Register('L', 6));
    }
    // "lea eax, [ebx+4]":
    // - Fontes   = registradores do endereço (tokens[2]);
    // - Destino  = tokens[1];
    // - Calcula endereço na ALU (sem memória/EFLAGS).
    else if (tokens[0] == "lea") {
        if (tokens.size() < 3) {
            std::cerr << "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        PushOperandSources(ex_source_registers, tokens[2], instruction_string);
    }
    // Aritmética - "add eax, ebx" (INT e SSE):
    // - Fontes   = tokens[2] e tokens[1] (RMW, exceto escritas puras);
    // - Destinos = tokens[1] e EFLAGS ('G', 80) (exceto NO_FLAGS_COPY).
    else {
        if (tokens.size() < 2) {
            std::cerr << "[ERRO] Instrução não suportada:\n"  <<
            "Instrução: "<< instruction_string << '\n';
            std::abort();
        }
        PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        // RMW: o destino antigo vira fonte (toda aritmética lê o destino, exceto
        // escritas puras como movsx/cvtsi2ss e cópias MOV).
        if (!IsMOVCopy(tokens) && !ContainsOpcode(PURE_WRITE, tokens[0]))
            PushWithMasked(ex_source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        // EFLAGS: destino extra apenas para opcodes que setam flags no x86 real
        // (SSE/AVX não setam; ver NO_FLAGS_COPY).
        if (!IsMOVCopy(tokens) && !ContainsOpcode(NO_FLAGS_COPY, tokens[0]))
            PushWithMasked(dest_registers, Register('G', 80));
        if (tokens.size() > 2)
            PushOperandSources(ex_source_registers, tokens[2], instruction_string);
    }
}

// Verifica se os destinos e fontes correspondem à sintaxe da linguagem.
// - Aborta em caso contrário, sem possibilidade de escrita incorreta.
void InstructionX86Intel::ValidateInstruction(
    const std::vector<std::string>& tokens

){
    if (tokens.empty()) return;

    const std::string& op = tokens[0];

    // 1. VALIDAÇÃO GLOBAL DE PONTUAÇÃO (Colchetes de Memória)
    // Garante que nenhum operando possua colchetes abertos e não fechados (ou vazios).
    for (const auto& token : tokens) {
        if (token.front() == '[') {
            if (token.back() != ']') {
                std::cerr << "[ERRO] Endereço não fechado por colchetes:\n"
                            << "Instrução: " << instruction_string << '\n';
                std::abort();
            }
            if (token.length() <= 2) {
                std::cerr << "[ERRO] Endereço vazio (apenas colchetes enviados):\n"
                            << "Instrução: " << instruction_string << '\n';
                std::abort();
            }
        }
    }

    // 2. VALIDAÇÃO DE DESVIOS (BRANCH)
    if (type == INSTRUCTION_TYPE::BRANCH) {
        if (op != "ret" && tokens.size() < 2) {
            std::cerr << "[ERRO] Instrução de desvio malformada (requer label ou alvo):\n"
                        << "Instrução: " << instruction_string << '\n';
            std::abort();
        }
        return;
    }

    // 3. VALIDAÇÃO DE DESTINOS INVÁLIDOS (Imediato no lugar do Destino)
    // No x86, o token[1] costuma ser o destino. Ele NUNCA pode ser um número puro (imediato).
    // Exceções: 'push' (não escreve no token 1) e 'cmp/test' (apenas leem para setar flags).
    if (tokens.size() > 1 && op != "push" && op != "cmp" && op != "test" &&
        op != "comiss" && op != "ucomiss") {

        const std::string& dest = tokens[1];
        // Se o token 1 não for memória (colchete) e não existir na tabela de registradores:
        if (dest.front() != '[' && !IsRegister(dest, RegisterTable())) {
            std::cerr << "[ERRO] Destino inválido (Imediato ou label no lugar de Reg/Memória):\n"
                        << "Instrução: " << instruction_string << '\n';
            std::abort();
        }
    }

    // 4. VALIDAÇÃO DE QUANTIDADE DE TOKENS (Cópia, Memória e Aritmética)
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE || op == "lea") {
        if (tokens.size() < 3) {
            std::cerr << "[ERRO] Instrução de memória/endereço requer pelo menos 2 operandos:\n"
                        << "Instrução: " << instruction_string << '\n';
            std::abort();
        }
    }
    else if (ContainsOpcode(MOVS, op)) {
        if (tokens.size() < 3) {
            std::cerr << "[ERRO] Instrução 'mov' malformada (requer destino e fonte):\n"
                        << "Instrução: " << instruction_string << '\n';
            std::abort();
        }
    }
    else if (tokens.size() < 2) {
        // Demais operações exigem ao menos um operando explícito (ex: pop, inc, mul)
        std::cerr << "[ERRO] Instrução requer pelo menos 1 operando explícito:\n"
                    << "Instrução: " << instruction_string << '\n';
        std::abort();
    }
}

} // namespace processor
