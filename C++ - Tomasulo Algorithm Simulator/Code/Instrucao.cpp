/* Instrucao.cpp */
#include "headers/Instrucao.h"
#include "headers/Componentes.h"
#include <string>

// Elementos static
std::vector<int> Instrucao::latencias_basicas_EX  = {0,1,1,1,1,4,10,9,14,40};
std::vector<int> Instrucao::latencias_basicas_MEM = {1,1};

// Métodos públicos
// Construtor:
Instrucao::Instrucao(
    int PC, std::string instrucao_em_string
):
    instrucao_em_string(instrucao_em_string),
    PC(PC)
{
    stringParaInstrucao();
}
// Códigos pequenos:
int Instrucao::getPC()                       const { return PC; }
int Instrucao::getLatenciaEX()               const { return latencia_EX; }
int Instrucao::getLatenciaMEM()              const { return latencia_MEM; }
TipoInstrucao Instrucao::getTipoInstrucao()  const { return tipo; }
Registrador Instrucao::getRegDestino()       const { return reg_destino; }
Registrador Instrucao::getJ()                const { return reg_J; }
Registrador Instrucao::getK()                const { return reg_K; }
const std::string& Instrucao::getInstrucaoString()  const { return instrucao_em_string; } // const & para evitar cópia
// Códigos grandes:
void Instrucao::setLatenciaMEM(
    int latencia
){
    latencia_MEM = latencia;
}

void Instrucao::setLatenciaEX(
    int latencia
){
    latencia_EX = latencia;
}
// Métodos privados
// Códigos grandes:
void Instrucao::stringParaInstrucao(){
    std::vector<std::string> tokens{splitInstrucao()};
    identificaTipo(tokens[0]);
    defineLatencias();
    defineAtributos(tokens);
}

std::vector<std::string> Instrucao::splitInstrucao(){
    // ADD   R1 R2 R3 => tokens[0] = ADD,   tokens[1] = R1, tokens[2] = R2,   tokens[3] = R3
    // LOAD  R1 n(R2) => tokens[0] = LOAD,  tokens[1] = R1, tokens[2] = n,    tokens[3] = R2
    // STORE R1 n(R2) => tokens[0] = STORE, tokens[1] = R1, tokens[2] = n,    tokens[3] = R2
    // BNEZ  R1 label => tokens[0] = BNEZ,  tokens[1] = R1, tokens[2] = label
    // JR    R1       => tokens[0] = JR,    tokens[1] = R1
    // JUMP  label    => tokens[0] = JUMP,  tokens[1] = label
    std::vector<std::string> tokens{};
    std::string tokenAtual{};
    for (char c : instrucao_em_string) {
        if (c == ',' || c == ' ' || c == '(' || c == ')' || c == '\t') {
            if (!tokenAtual.empty()) {       // acabou um token
                tokens.push_back(tokenAtual);
                tokenAtual = "";
            } // se tokenAtual já está vazio, é espaço repetido — ignora
        } else tokenAtual += c; // acumula caractere no token
    }
    if (!tokenAtual.empty()) tokens.push_back(tokenAtual); // último token (sem separador no fim)
    return tokens;
}

void Instrucao::identificaTipo(
    const std::string& op
){
    // LOAD
    if (op=="LOAD"||op=="LW"||op=="LB"||op=="LH"||op=="LBU"||op=="LHU"||op=="L.D"||op=="L.S"||op=="LD"||op=="LWU"||op=="LL")
        tipo = TipoInstrucao::LOAD;
    // STORE
    else if (op=="STORE"||op=="SW"||op=="SB"||op=="SH"||op=="S.D"||op=="S.S"|| op=="SD"||op=="SC")
        tipo = TipoInstrucao::STORE;
    // INT_BASICO (inclui adição, subtração, shifts, operações lógicas e comparações sem saltos)
    else if (// ADD e SUB
        op=="ADD" ||op=="ADDI" ||op=="ADDU" ||op=="ADDIU"||op=="DADDU"||op=="DADDIU"||op=="SUB" ||op=="SUBI" ||op=="SUBU" ||op=="DSUBU" ||
        // Operadores lógicos
        op=="AND" ||op=="ANDI" ||op=="OR" ||op=="ORI" ||op=="XOR" ||op=="XORI" || op=="NOR" ||op=="LUI" ||
        // Shift
        op=="SLL" ||op=="SRL" ||op=="SRA" ||op=="SLLV" ||op=="SRLV" ||op=="SRAV" ||op=="DSLL" ||op=="DSRL" ||op=="DSRA" ||
        // Comparadores
        op=="SLT" ||op=="SLTI" ||op=="SLTU" ||op=="SLTIU"||op=="DSLT" ||op=="DSLTI"||op=="DSLTU"||op=="DSLTIU")
        tipo = TipoInstrucao::INT_BASICO;
    // BRANCH
    else if (// Condicionais
        op=="BEQ" ||op=="BNE" ||op=="BNEZ" ||op=="BEQZ" ||op=="BGTZ" ||op=="BLTZ"||op=="BGEZ" ||op=="BLEZ" ||op=="BLTZAL"||op=="BGEZAL"||
        // Jumps
        op=="J" ||op=="JAL" ||op=="JR" ||op=="JALR")
        tipo = TipoInstrucao::BRANCH;
    // INT_MUL
    else if (op=="MULT"||op=="MULTU"||op=="MUL"||op=="DMULT"||op=="DMULTU")
        tipo = TipoInstrucao::INT_MUL;
    // INT_DIV
    else if (op=="DIV" ||op=="DIVU" ||op=="DDIV"||op=="DDIVU")
        tipo = TipoInstrucao::INT_DIV;
    // FLOAT_BASICO
    else if (// ADD e SUB
        op=="ADD.D"||op=="ADD.S"|| op=="SUB.D"||op=="SUB.S")
        tipo = TipoInstrucao::FLOAT_BASICO;
    // FLOAT_MUL
    else if (op=="MUL.D"||op=="MUL.S")
        tipo = TipoInstrucao::FLOAT_MUL;
    // FLOAT_DIV
    else if (op=="DIV.D"||op=="DIV.S")
        tipo = TipoInstrucao::FLOAT_DIV;
    // Instrução não suportada
    else tipo = TipoInstrucao::NAO_EXISTE;
}

void Instrucao::defineLatencias(){
    if(tipo == TipoInstrucao::LOAD){
        latencia_MEM = latencias_basicas_MEM[0];
    } else if(tipo == TipoInstrucao::STORE){
        latencia_MEM = latencias_basicas_MEM[1];
    }
    latencia_EX = latencias_basicas_EX[static_cast<int>(tipo)]; // Enums podem ser convertidos em int ()
}

void Instrucao::defineAtributos(
    std::vector<std::string>& tokens
){
    // LOAD  R1 n(R2)    => regDestino = R1, J = -,   K = R2
    // STORE R1 n(R2)    => regDestino = -,  J = R1,  K = R2
    // BEQ   R1 R2 label => regDestino = -,  J = R1,  K = R2
    // BNEZ  R1 label    => regDestino = -,  J = R1,  K = -
    // JR    R1          => regDestino = -,  J = R1,  K = -
    // J     label       => regDestino = -,  J = -,   K = -
    // ADD   R1 R2 R3    => regDestino = R1, J = R2,  K = R3
    if (tipo == TipoInstrucao::LOAD){
        reg_destino = Registrador(tokens[1]);
        reg_K       = Registrador(tokens[3]);
    } else if (tipo == TipoInstrucao::STORE){
        reg_J       = Registrador(tokens[1]);
        reg_K       = Registrador(tokens[3]);
    } else if (tipo == TipoInstrucao::BRANCH){
        if(std::toupper(tokens[1][0]) == 'F' || std::toupper(tokens[1][0]) == 'R')
            reg_J   = Registrador(tokens[1]);
        if(tokens.size() > 3) if(std::toupper(tokens[2][0]) == 'F' || std::toupper(tokens[2][0]) == 'R')
            reg_K   = Registrador(tokens[2]);
    } else {
        reg_destino = Registrador(tokens[1]);
        reg_J       = Registrador(tokens[2]);
        reg_K       = Registrador(tokens[3]);
    }
}
