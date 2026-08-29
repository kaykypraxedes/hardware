/* tb_SyntheticInstruction.h */

/**
 * @file tb_SyntheticInstruction.h
 *
 * @brief Instrução sintética compartilhada pelos testes do pipeline
 * multi-etapa.
 */

#ifndef TB_SYNTHETIC_INSTRUCTION_H
#define TB_SYNTHETIC_INSTRUCTION_H

#include "../headers/Architecture.h"

using namespace processor;

// ─── CLASSES ──────────────────────────────────────────────────────

/**
 * @brief Produz planos determinísticos sem alterar arquiteturas reais.
 */
class SyntheticInstruction : public Instruction {
    public:
        SyntheticInstruction(
            const int position = 0
        ) : Instruction(position) {}

    private:
        std::vector<std::string> SplitInstruction(
            const std::string& str
        ) const override {
            return {str};
        }

        bool SetStages(
            const std::vector<std::string>& tokens
        ) override {
            if (tokens[0] == "multi") {
                AddStage(INSTRUCTION_TYPE::LOAD, {Register('R', 1)}, {});
                AddStage(
                    INSTRUCTION_TYPE::INT_MUL,
                    {Register('R', 6), Register('R', 2)},
                    {}
                );
                AddStage(
                    INSTRUCTION_TYPE::STORE,
                    {Register('R', 3)},
                    {Register('R', 4), Register('R', 6)}
                );
                dest_registers = {Register('R', 6)};
                return true;
            }

            if (tokens[0] == "load_store") {
                AddStage(INSTRUCTION_TYPE::LOAD, {Register('R', 1)}, {});
                AddStage(
                    INSTRUCTION_TYPE::STORE,
                    {Register('R', 3)},
                    {Register('R', 4)}
                );
                return true;
            }

            return false;
        }

        void NormalizeInstruction(
            std::vector<std::string>&
        ) override {}
};

#endif
