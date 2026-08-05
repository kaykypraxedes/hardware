/* headers/Instruction.h */
#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "Components.h"
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

namespace processor {

enum class INSTRUCTION_TYPE {
    INVALID,
    LOAD,
    STORE,
    BRANCH,
    INT_BASIC,
    INT_MUL,
    INT_DIV,
    FLOAT_BASIC,
    FLOAT_MUL,
    FLOAT_DIV
};

// Fases da pipeline do processador Tomasulo.
enum class INSTRUCTION_PHASE_TOMASULO {
    UNUSED,
    IS,
    EX,
    MEM,
    WR,
    COMMIT
};

class Instruction {
public:
    static std::vector<int> base_ex_latencies;
    static std::vector<int> base_mem_latencies;

    explicit Instruction(const int position = -1);
    virtual ~Instruction() = default;

    // Getters públicos
    int GetPosition() const;
    int GetExLatency() const;
    int GetMemLatency() const;
    INSTRUCTION_TYPE GetInstructionType() const;
    const std::string& GetInstructionString() const;
    const std::vector<Register>& GetDestRegisters() const;
    const std::vector<Register>& GetSourceRegisters() const;

    // Setters públicos
    void SetMemLatency(const int latency);
    void SetExLatency(const int latency);

    // Template method público de parseamento
    void Parse(const std::string& instruction_string);

protected:
    int position{-1};
    std::string instruction_string;
    int ex_latency{};
    int mem_latency{};
    INSTRUCTION_TYPE type{INSTRUCTION_TYPE::INVALID};

    std::vector<Register> dest_registers;
    std::vector<Register> source_registers;

    // Métodos virtuais puros que cada arquitetura deve implementar
    virtual std::vector<std::string> SplitInstruction(const std::string& str) const = 0;
    virtual bool IdentifyType(const std::string& op) = 0;
    virtual void NormalizeInstruction(std::vector<std::string>& tokens) = 0;
    virtual void SetAttributes(const std::vector<std::string>& tokens) = 0;

    void SetLatencies();
};

} // namespace processor

#endif
