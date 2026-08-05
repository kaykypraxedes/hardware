/* Instruction/headers/InstructionRiscV.h */
#ifndef INSTRUCTION_RISCV_H
#define INSTRUCTION_RISCV_H

#include "../../headers/Instruction.h"
#include <algorithm>
#include <cctype>

namespace processor {

class InstructionRiscV : public Instruction {
public:
    explicit InstructionRiscV(const int position = -1);

protected:
    std::vector<std::string> SplitInstruction(const std::string& str) const override;
    bool IdentifyType(const std::string& op) override;
    void NormalizeInstruction(std::vector<std::string>& tokens) override;
    void SetAttributes(const std::vector<std::string>& tokens) override;
};

} // namespace processor

#endif
