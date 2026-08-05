/* Instruction/headers/InstructionMips32.h */
#ifndef INSTRUCTION_MIPS32_H
#define INSTRUCTION_MIPS32_H

#include "../../headers/Instruction.h"
#include <algorithm>
#include <cctype>

namespace processor {

class InstructionMips32 : public Instruction {
public:
    explicit InstructionMips32(const int position = -1);

protected:
    std::vector<std::string> SplitInstruction(const std::string& str) const override;
    bool IdentifyType(const std::string& op) override;
    void NormalizeInstruction(std::vector<std::string>& tokens) override;
    void SetAttributes(const std::vector<std::string>& tokens) override;
};

} // namespace processor

#endif
