/* Instruction/headers/InstructionArm64.h */
#ifndef INSTRUCTION_ARM64_H
#define INSTRUCTION_ARM64_H

#include "../../headers/Instruction.h"
#include <algorithm>
#include <cctype>

namespace processor {

class InstructionArm64 : public Instruction {
public:
    explicit InstructionArm64(const int position = -1);

protected:
    std::vector<std::string> SplitInstruction(const std::string& str) const override;
    bool IdentifyType(const std::string& op) override;
    void NormalizeInstruction(std::vector<std::string>& tokens) override;
    void SetAttributes(const std::vector<std::string>& tokens) override;
};

} // namespace processor

#endif
