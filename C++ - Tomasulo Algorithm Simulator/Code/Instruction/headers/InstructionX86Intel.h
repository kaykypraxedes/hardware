/* Instruction/headers/InstructionX86Intel.h */
#ifndef INSTRUCTION_X86INTEL_H
#define INSTRUCTION_X86INTEL_H

#include "../../headers/Instruction.h"
#include <algorithm>
#include <cctype>

namespace processor {

class InstructionX86Intel : public Instruction {
public:
    explicit InstructionX86Intel(const int position = -1);

protected:
    std::vector<std::string> SplitInstruction(const std::string& str) const override;
    bool IdentifyType(const std::string& op) override;
    void NormalizeInstruction(std::vector<std::string>& tokens) override;
    void SetAttributes(const std::vector<std::string>& tokens) override;
};

} // namespace processor

#endif
