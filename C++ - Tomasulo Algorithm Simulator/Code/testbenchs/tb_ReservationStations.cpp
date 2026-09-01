/* tb_ReservationStations.cpp */
// Testbench isolado de ReservationStations.cpp
#include "../headers/ReservationStations.h"
#include "../headers/Components.h"
#include "../headers/Architecture.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include "tb_SyntheticInstruction.h"
#include <vector>

using namespace processor;

// Monta uma instrução simplificada com posição lógica controlada pelo teste.
static std::shared_ptr<Instruction> make_inst(
    const std::string& line,
    const int          position = 0
) {
    auto instruction{std::make_shared<InstructionSimplified>(position)};
    instruction->Parse(line);
    return instruction;
}

static RegisterStatusTable MakeRegisterStatus() {
    // Mesmo caminho da Thread: gera os slots
    // físicos com os ids corretos (R 0-31, F 32-63, M 64-65).
    return RegisterStatusTable(InstructionFactory::MakeRegisterLayout(ARCHITECTURE::SIMPLIFIED).references);
}

// Helpers de acesso ao Register Status pelo nome arquitetural (F(register_status, 4) == F4 == slot 36).
// GetReg pesquisa apenas pelo id físico global — F<n> fica em 32 + n (faixa 32-63).
static Register R(const int i) { return Register('R', i); }
static Register F(const int i) { return Register('F', 32 + i); }
// hi/lo são o único caso desta arquitetura sem índice — registradores fixos (ids 64/65).
static Register HI() { return Register('M', 64); }
static Register LO() { return Register('M', 65); }

// Retorna o produtor pendente mais recente apenas para asserções do teste.
static int LatestPending(
    const RegisterStatusTable& register_status,
    const Register&            reference
) {
    const REGISTER_STATUS_VIEW view{register_status.FindStatus(reference)};
    for (int i{static_cast<int>(view.producer_positions.size()) - 1}; i >= 0; i--)
        if (!register_status.IsProducerResolved(reference, view.producer_positions[i]))
            return view.producer_positions[i];
    return -1;
}

static FUNCTIONAL_UNITS makeFU(int n = 2) {
    FUNCTIONAL_UNITS fu;
    for (int i = 0; i < n; i++) fu.memory_access.push_back(FU{});
    for (int i = 0; i < n; i++) fu.int_basic_alu.push_back(FU{});
    for (int i = 0; i < n; i++) fu.int_mult_div_alu.push_back(FU{});
    for (int i = 0; i < n; i++) fu.float_basic_alu.push_back(FU{});
    for (int i = 0; i < n; i++) fu.float_mult_div_alu.push_back(FU{});
    fu.wr = 2;
    return fu;
}

// Verifica se TODAS as posições de um vetor de dependências (ex_Q/mem_Q) já
// foram resolvidas — útil quando o teste não precisa checar uma posição
// específica, só "nada está pendente".
static bool all_resolved(const std::vector<int>& q) {
    for (const int producer_position : q)
        if (producer_position != -1) return false;
    return true;
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E ESTADO INICIAL
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E ESTADO INICIAL");

    section("1.1 ReservationStation() — construtor");
    {
        RS r("load0");
        check("GetId() == 'load0'",            r.GetId() == "load0");
        check("IsBusy() == false",             r.IsBusy() == false);
        check("GetCountdown() == -1",          r.GetCountdown() == -1);
        check("GetFUPosition() == -1",         r.GetFUPosition() == -1);
        check("GetTimes() vazio",              r.GetTimes().empty());
        check("GetInstructions() vazio",       r.GetInstructions().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 2. ISSUE — EMISSÃO NA RS (AddIssue)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. ISSUE — EMISSÃO NA RS (AddIssue)");

    section("2.1 AddIssue() — instrução sem dependências (add r3, r1, r2)");
    {
        RS rs("int0");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        auto instr = make_inst("add r3, r1, r2", 0);
        bool ok = rs.AddIssue(instr, register_status, 1, true);
        check("AddIssue() retorna true",
            ok);
        check("IsBusy() == true",
            rs.IsBusy());
        check("GetInstructionPhase() == ISSUE",
            rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::IS);
        check("ex_Q com 2 posições, ambas resolvidas (r1 e r2 livres)",
            rs.GetExDependencies().size() == 2 &&
            all_resolved(rs.GetExDependencies()));
        check("mem_Q vazio (add não usa fase MEM)",
            rs.GetMemDependencies().empty());
        check("GetInstructions()[0] == 'add    r3, r1, r2'",
              rs.GetInstructions().size() == 1 && rs.GetInstructions()[0] == "add    r3, r1, r2");
        check("GetTimes()[0] == 1 (ciclo de issue)",
              rs.GetTimes().size() == 1 && rs.GetTimes()[0] == 1);
        check("Register Status de R3 mantém produtor 0",
            LatestPending(register_status, R(3)) == 0);
        check("RS mantém a instrução e a etapa alocadas",
            rs.GetCurrentInstruction().GetPosition() == instr->GetPosition() &&
            rs.GetCurrentStage() == 0);
        check("histórico do produtor preserva 'int0'",
            register_status.FindStatus(R(3)).allocated_rs == std::vector<std::string>{"int0"});
    }

    section("2.2 AddIssue() — RS ocupada retorna false");
    {
        RS rs("int0");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, register_status, 1, true);

        auto instr2 = make_inst("sub r5, r1, r2", 1);
        bool dup = rs.AddIssue(instr2, register_status, 2, true);
        check("AddIssue em RS ocupada retorna false", !dup);
    }

    section("2.3 AddIssue() — dependência em ex_Q[0] (mul.d f4, f2, f0 quando f2 pendente)");
    {
        RS rs("fmul0");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const int producer_position{7};
        register_status.AllocateProducer(F(2), producer_position, "load0", 1);

        auto instr = make_inst("mul.d f4, f2, f0", 8);
        rs.AddIssue(instr, register_status, 2, true);
        check("ex_Q[0] == 7 (f2 pendente)", rs.GetExDependencies()[0] == producer_position);
        check("ex_Q[1] == -1 (f0 livre)",   rs.GetExDependencies()[1] == -1);
        check("Register Status F[4] -> produtor 8", LatestPending(register_status, F(4)) == 8);
    }

    section("2.4 AddIssue() — 'add r1, r1, r2' sem auto-dependência");
    {
        RS rs("int1");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        auto instr = make_inst("add r1, r1, r2");
        rs.AddIssue(instr, register_status, 1, true);
        check("Sem auto-dependência: ex_Q inteiramente resolvido (r1 e r2 estavam livres)",
            all_resolved(rs.GetExDependencies()));
    }

    section("2.5 AddIssue() — MÚLTIPLOS DESTINOS: 'mult' aloca hi E lo simultaneamente no Register Status");
    {
        RS rs("intmul1");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        auto instr = make_inst("mult r1, r2");
        bool ok = rs.AddIssue(instr, register_status, 3, true);

        check("AddIssue() retorna true",
            ok);
        check("ex_Q com 2 posições (r1, r2), ambas resolvidas",
            rs.GetExDependencies().size() == 2 &&
            all_resolved(rs.GetExDependencies()));
        check("Register Status HI marcado com produtor 0", LatestPending(register_status, HI()) == 0);
        check("Register Status LO marcado com produtor 0", LatestPending(register_status, LO()) == 0);
        check("histórico de HI preserva 'intmul1'",
            register_status.FindStatus(HI()).allocated_rs == std::vector<std::string>{"intmul1"});
    }

    section("2.6 AddIssue() — posição lógica negativa aborta");
    {
        check("instrução sem posição válida é rejeitada", Aborts([]() {
            RS rs("int_invalid");
            RegisterStatusTable register_status{MakeRegisterStatus()};
            rs.AddIssue(
                make_inst("add r3, r1, r2", -1),
                register_status,
                1,
                true
            );
        }));
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. DEPENDÊNCIAS — ENTRADA EM EX (UpdateDependencies)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. DEPENDÊNCIAS — ENTRADA EM EX (UpdateDependencies)");

    section("3.1 UpdateDependencies() — instrução pronta (ex_Q resolvido) entra em EX");
    {
        RS rs("int2");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, register_status, 1, true);

        bool started = rs.UpdateDependencies(register_status, fu, 2);
        check("UpdateDependencies retorna true (pronta)",  started);
        check("fase == EX após UpdateDependencies",        rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX);
        check("GetCountdown() == exLat == 1",              rs.GetCountdown() == 1);
        check("GetFUPosition() >= 0 (FU alocada)",         rs.GetFUPosition() >= 0);

        bool second = rs.UpdateDependencies(register_status, fu, 2);
        check("Segunda chamada retorna false (já em EX)",     !second);
    }

    section("3.2 UpdateDependencies() — aguarda ex_Q[0] ser liberado");
    {
        RS rs("fmul1");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        const int producer_position{7};
        register_status.AllocateProducer(F(2), producer_position, "load0", 1);

        auto instr = make_inst("mul.d f4, f2, f0", 8);
        rs.AddIssue(instr, register_status, 2, true);
        check("ex_Q[0] == 7 antes de liberar", rs.GetExDependencies()[0] == producer_position);

        bool before = rs.UpdateDependencies(register_status, fu, 3);
        check("UpdateDependencies retorna false com ex_Q[0] pendente", !before);

        register_status.DeallocateProducer(F(2), producer_position, 3);
        bool after = rs.UpdateDependencies(register_status, fu, 4);
        check("UpdateDependencies retorna true após ex_Q[0] liberado", after);
        check("fase == EX após liberar", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX);
    }

    section("3.3 STORE com endereço pronto (ex_Q) mas dado pendente (mem_Q) entra em EX");
    {
        RS rs("store1");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        const int producer_position{8};
        register_status.AllocateProducer(F(8), producer_position, "fmul2", 1); // dado (f8) pendente; endereço (r1) livre

        auto instr = make_inst("s.d f8, 0(r1)", 9);
        rs.AddIssue(instr, register_status, 2, true);
        check("ex_Q (endereço) vazio",
            all_resolved(rs.GetExDependencies()));
        check("mem_Q[0] == 8 (dado pendente)",
            rs.GetMemDependencies().size() == 1 &&
            rs.GetMemDependencies()[0] == producer_position);

        bool entered_ex = rs.UpdateDependencies(register_status, fu, 3);
        check("STORE entra em EX mesmo com dado (mem_Q) pendente — EX só olha ex_Q", entered_ex);

        rs.UpdateCountdown(fu, 3);
        check("fase == MEM aguardando dado", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM);

        bool blocked = rs.UpdateDependencies(register_status, fu, 4);
        check("MEM bloqueado enquanto mem_Q não resolve", !blocked);

        register_status.DeallocateProducer(F(8), producer_position, 5);
        bool mem_ok = rs.UpdateDependencies(register_status, fu, 5);
        check("MEM inicia após dado (mem_Q) resolvido", mem_ok);
    }

    section("3.4 STORE com endereço pendente (ex_Q) NÃO entra em EX");
    {
        RS rs("store2");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        const int producer_position{9};
        register_status.AllocateProducer(R(9), producer_position, "int5", 1); // endereço pendente

        auto instr = make_inst("s.d f0, 0(r9)", 10);
        rs.AddIssue(instr, register_status, 2, true);
        check("ex_Q[0] == 9 (endereço pendente)",
            rs.GetExDependencies().size() == 1 &&
            rs.GetExDependencies()[0] == producer_position);

        bool blocked = rs.UpdateDependencies(register_status, fu, 3);
        check("STORE não entra em EX com endereço (ex_Q) pendente", !blocked);
        check("fase permanece ISSUE",             rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::IS);
    }

    section("3.5 ResolveDependency() — captura direta via broadcast simulado");
    {
        RS rs("fmul3");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const int producer_position{12};
        register_status.AllocateProducer(F(2), producer_position, "load2", 1);

        auto instr = make_inst("mul.d f4, f2, f0", 14);
        rs.AddIssue(instr, register_status, 2, true);
        check("ex_Q[0] == 12 antes do broadcast",
            rs.GetExDependencies()[0] == producer_position);

        rs.ResolveDependency(producer_position, F(2));
        check("ex_Q[0] limpo após ResolveDependency", rs.GetExDependencies()[0] == -1);

        // Posição lógica diferente não deve afetar nada.
        RS rs2("fmul4");
        const int other_producer{13};
        register_status.AllocateProducer(F(10), other_producer, "load3", 1);
        auto instr2 = make_inst("mul.d f12, f10, f0", 14);
        rs2.AddIssue(instr2, register_status, 2, true);
        rs2.ResolveDependency(99, F(10));
        check("ResolveDependency com posição diferente não altera ex_Q",
            rs2.GetExDependencies()[0] == other_producer);
    }

    section("3.6 WAW — produtor antigo não resolve dependência do mais novo");
    {
        RS rs("fmul_waw");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        const Register source{F(2)};
        register_status.AllocateProducer(source, 30, "load0", 1);
        register_status.AllocateProducer(source, 31, "load1", 2);

        auto instruction = make_inst("mul.d f4, f2, f0", 32);
        rs.AddIssue(instruction, register_status, 3, true);
        check("Q captura somente o produtor lógico mais novo", rs.GetExDependencies()[0] == 31);

        register_status.DeallocateProducer(source, 30, 4);
        rs.ResolveDependency(30, source);
        check("broadcast antigo não limpa Q do produtor novo", rs.GetExDependencies()[0] == 31);
        check("instrução continua bloqueada", !rs.UpdateDependencies(register_status, fu, 4));

        register_status.DeallocateProducer(source, 31, 5);
        check("instrução inicia após o produtor correto finalizar",
            rs.UpdateDependencies(register_status, fu, 5));
    }

    section("3.7 Produtor mais recente concluído deixa a fonte pronta");
    {
        RS rs("int_completed_latest");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const Register source{R(1)};
        register_status.AllocateProducer(source, 10, "old_pending", 1);
        register_status.AllocateProducer(source, 20, "latest_completed", 2);
        register_status.DeallocateProducer(source, 20, 3);

        auto instruction = make_inst("add r3, r1, r2", 30);
        rs.AddIssue(instruction, register_status, 4, true);

        check("fonte pronta não recua para produtor antigo pendente",
            rs.GetExDependencies()[0] == -1 &&
            rs.GetExValues()[0].GetId() == 1 &&
            register_status.IsBusy(source));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. MÚLTIPLAS FONTES E DESTINOS — VETORES EM AÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. MÚLTIPLAS FONTES E DESTINOS — VETORES EM AÇÃO");

    section("4.1 Fonte parcialmente pendente — só a posição [1] fica esperando");
    {
        RS rs("int8");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const int producer_position{14};
        register_status.AllocateProducer(R(2), producer_position, "load4", 1); // r2 (2a fonte) pendente; r1 (1a fonte) livre

        auto instr = make_inst("add r3, r1, r2", 15);
        rs.AddIssue(instr, register_status, 2, true);

        check("ex_Q[0] == -1 (r1 estava livre)", rs.GetExDependencies()[0] == -1);
        check("ex_Q[1] == 14 (r2 pendente)",
            rs.GetExDependencies()[1] == producer_position);
    }

    section("4.2 Mesmo produtor em duas posições — 'add r3, r1, r1' com r1 pendente");
    {
        RS rs("int9");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const int producer_position{15};
        register_status.AllocateProducer(R(1), producer_position, "load5", 1);

        auto instr = make_inst("add r3, r1, r1", 16);
        rs.AddIssue(instr, register_status, 2, true);
        check("ex_Q[0] == 15",
            rs.GetExDependencies()[0] == producer_position);
        check("ex_Q[1] == 15 (mesma dependência nas duas posições)",
            rs.GetExDependencies()[1] == producer_position);

        rs.ResolveDependency(producer_position, R(1));
        check("ResolveDependency limpa AMBAS as posições de uma vez",
            rs.GetExDependencies()[0] == -1 &&
            rs.GetExDependencies()[1] == -1);
    }

    section("4.3 Encadeamento via HI/LO — 'mflo' depende do 'lo' produzido por um 'mult' anterior");
    {
        RS rs_mult("intmul2");
        RS rs_mflo("int10");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();

        auto i_mult = make_inst("mult r1, r2", 16);
        rs_mult.AddIssue(i_mult, register_status, 1, true); // hi/lo ficam pendentes de 'intmul2'

        auto i_mflo = make_inst("mflo r3", 17);
        rs_mflo.AddIssue(i_mflo, register_status, 2, true);
        check("mflo: ex_Q[0] == 16 (lo ainda pendente do mult)",
            rs_mflo.GetExDependencies().size() == 1 &&
            rs_mflo.GetExDependencies()[0] == 16);

        register_status.DeallocateProducer(LO(), 16, 3); // simula fim do broadcast do mult
        bool started = rs_mflo.UpdateDependencies(register_status, fu, 3);
        check("mflo entra em EX assim que 'lo' é liberado", started);
    }

    section("4.4 Instrução sem fontes EX — vetor vazio nunca bloqueia");
    {
        RS rs("int11");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("lui r1, #5");
        rs.AddIssue(instr, register_status, 1, true);

        check("lui: ex_Q vazio (nenhuma fonte)", rs.GetExDependencies().empty());
        bool started = rs.UpdateDependencies(register_status, fu, 2);
        check("lui: entra em EX imediatamente (vetor vazio não bloqueia)", started);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. CONTAGEM DE CICLOS — PROGRESSÃO DE FASES (UpdateCountdown)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. CONTAGEM DE CICLOS — PROGRESSÃO DE FASES (UpdateCountdown)");

    section("5.1 UpdateCountdown() — INT_BASIC (exLat=1): EX -> WR");
    {
        RS rs("int3");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, register_status, 1, true);
        rs.UpdateDependencies(register_status, fu, 2);

        bool finished = rs.UpdateCountdown(fu, 2);
        check("UpdateCountdown retorna true (EX terminou)", finished);
        check("fase == WR após EX de 1 ciclo",              rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
        check("GetFUPosition() == -1 (FU liberada)",        rs.GetFUPosition() == -1);
    }

    section("5.2 UpdateCountdown() — LOAD (EX->MEM->WR)");
    {
        RS rs("load0");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("l.d f2, 0(r1)");
        rs.AddIssue(instr, register_status, 1, true);
        rs.UpdateDependencies(register_status, fu, 2);

        bool ex_done = rs.UpdateCountdown(fu, 2);
        check("LOAD: UpdateCountdown sinaliza fim do EX", ex_done);
        check("LOAD: fase == MEM após EX",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM);
        check("LOAD: countdown == -1 (aguarda iniciar MEM)", rs.GetCountdown() == -1);

        bool started_mem = rs.UpdateDependencies(register_status, fu, 3);
        check("LOAD: UpdateDependencies inicia MEM no ciclo 3", started_mem);
        check("LOAD: countdown == latMEM == 1 após iniciar MEM", rs.GetCountdown() == 1);

        bool mem_end = rs.UpdateCountdown(fu, 3);
        check("LOAD: UpdateCountdown sinaliza fim do MEM", mem_end);
        check("LOAD: fase == WR após MEM",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
    }

    section("5.3 UpdateCountdown() — STORE (EX->espera dado->MEM->WR)");
    {
        RS rs("store0");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        const int producer_position{18};
        register_status.AllocateProducer(F(6), producer_position, "float_basico0", 1);

        auto instr = make_inst("s.d f6, 0(r2)", 19);
        rs.AddIssue(instr, register_status, 2, true);
        rs.UpdateDependencies(register_status, fu, 3);

        bool ex_done = rs.UpdateCountdown(fu, 3);
        check("STORE: fim do EX sinalizado", ex_done);
        check("STORE: countdown == -1 após EX (aguarda dado)", rs.GetCountdown() == -1);

        register_status.DeallocateProducer(F(6), producer_position, 4);
        bool mem_ok = rs.UpdateDependencies(register_status, fu, 4);
        check("STORE: UpdateDependencies inicia MEM após dado liberado", mem_ok);
        check("STORE: fase == MEM", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM);

        bool mem_end = rs.UpdateCountdown(fu, 4);
        check("STORE: fim do MEM sinalizado", mem_end);
        check("STORE: fase == WR", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. LIBERAÇÃO E REUSO (Release)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. LIBERAÇÃO E REUSO (Release)");

    section("6.1 Release() — RS de inteiros liberada");
    {
        RS rs("int4");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, register_status, 1, true);
        rs.UpdateDependencies(register_status, fu, 2);
        rs.UpdateCountdown(fu, 2);

        rs.Release(3);
        check("IsBusy() == false após Release",         !rs.IsBusy());
        check("GetCountdown() == -1",                   rs.GetCountdown() == -1);
        check("GetFUPosition() == -1",                  rs.GetFUPosition() == -1);
        check("Release limpa ex_V/ex_Q da RS",
            rs.GetExValues().empty() && rs.GetExDependencies().empty());
        check("Release limpa mem_V/mem_Q da RS",
            rs.GetMemValues().empty() && rs.GetMemDependencies().empty());
        check("GetTimes() tem 2 entradas após Release", rs.GetTimes().size() == 2);
        check("GetTimes()[1] == 3 (ciclo de release)",  rs.GetTimes()[1] == 3);
        check("Release restaura o índice de etapa da RS", rs.GetCurrentStage() == 0);
    }

    section("6.2 Release() após LOAD — caminho de liberação completo e reuso");
    {
        RS rs("load1");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("l.d f4, 0(r2)", 0);

        rs.AddIssue(instr, register_status, 1, true);
        rs.UpdateDependencies(register_status, fu, 2);
        rs.UpdateCountdown(fu, 2);
        rs.UpdateDependencies(register_status, fu, 3);
        rs.UpdateCountdown(fu, 3);

        check("LOAD antes de Release: fase == WR",  rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
        check("LOAD antes de Release: busy == true", rs.IsBusy());

        rs.Release(4);

        check("LOAD: busy == false após Release",    !rs.IsBusy());
        check("LOAD: countdown == -1 após Release",  rs.GetCountdown() == -1);
        check("LOAD: fuPosition == -1 após Release", rs.GetFUPosition() == -1);
        check("LOAD: Release limpa ex_V/ex_Q da RS",
            rs.GetExValues().empty() && rs.GetExDependencies().empty());
        check("LOAD: Release limpa mem_V/mem_Q da RS",
            rs.GetMemValues().empty() && rs.GetMemDependencies().empty());
        check("LOAD: GetTimes().size() == 2",        rs.GetTimes().size() == 2);
        check("LOAD: GetTimes()[0] == 1 (issue)",    rs.GetTimes()[0] == 1);
        check("LOAD: GetTimes()[1] == 4 (release)",  rs.GetTimes()[1] == 4);
        auto instr2 = make_inst("l.d f6, 0(r3)", 1);
        bool reuse = rs.AddIssue(instr2, register_status, 5, true);
        check("LOAD: RS pode ser reusada após Release",  reuse);
    }

    section("6.3 Reuso de RS — sem autodependência espúria (regressão do resíduo 'tag == id')");
    {
        RS rs("int7");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        FUNCTIONAL_UNITS fu = makeFU();

        auto i1 = make_inst("add r7, r1, r2", 20);
        rs.AddIssue(i1, register_status, 1, true);
        rs.UpdateDependencies(register_status, fu, 2);
        rs.UpdateCountdown(fu, 2);           // WR
        register_status.DeallocateProducer(R(7), 20, 3); // simula fim do broadcast
        rs.Release(3);

        auto i2 = make_inst("add r7, r7, r1", 21); // lê e escreve r7 de novo, mesma RS
        rs.AddIssue(i2, register_status, 4, true);
        check("Sem autodependência espúria ao reler R7 já resolvido",
            all_resolved(rs.GetExDependencies()));
    }

    section("6.4 Issue interno — recaptura fontes sem reservar destinos");
    {
        RS first_rs("int_first");
        RS second_rs("int_second");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const Register source{R(1)};
        register_status.AllocateProducer(source, 30, "load_old", 1);

        auto instruction = make_inst("add r3, r1, r2", 32);
        first_rs.AddIssue(instruction, register_status, 2, true);
        check("primeiro Issue captura o produtor antigo",
            first_rs.GetExDependencies()[0] == 30);
        first_rs.Release(2);

        // O novo Issue captura novamente a fonte no estado corrente.
        register_status.AllocateProducer(source, 31, "load_new", 3);
        register_status.DeallocateProducer(source, 30, 3);
        bool internal_issue{second_rs.AddIssue(instruction, register_status, 4, false)};

        check("Issue interno recaptura o produtor lógico anterior mais recente",
            internal_issue && second_rs.GetExDependencies()[0] == 31);
        check("destino é reservado uma única vez",
            register_status.FindStatus(R(3)).producer_positions == std::vector<int>{32});
        check("histórico do destino preserva somente a primeira RS",
            register_status.FindStatus(R(3)).allocated_rs == std::vector<std::string>{"int_first"});
    }

    section("6.5 Issue interno captura somente as fontes da nova etapa");
    {
        RS first_rs("load_stage");
        RS second_rs("mul_stage");
        RegisterStatusTable register_status{MakeRegisterStatus()};
        const Register first_source{R(1)};
        const Register second_source{R(6)};
        register_status.AllocateProducer(first_source, 20, "first_old", 1);
        register_status.AllocateProducer(second_source, 25, "second_old", 1);

        auto instruction{std::make_shared<SyntheticInstruction>(30)};
        instruction->Parse("multi");
        first_rs.AddIssue(instruction, register_status, 2, true);
        check("primeira etapa captura somente R1",
            first_rs.GetExDependencies() == std::vector<int>{20});

        first_rs.Release(3);
        register_status.AllocateProducer(second_source, 35, "second_newer", 3);
        second_rs.AddIssue(instruction, register_status, 4, false, 1);

        check("nova etapa recaptura R6 e ignora produtor igual/posterior",
            second_rs.GetCurrentStage() == 1 &&
            second_rs.GetExDependencies() == std::vector<int>({25, -1}));
        check("Issue interno não altera a descrição completa",
            instruction->GetInstructionType() == INSTRUCTION_TYPE::LOAD &&
            instruction->GetInstructionTypes().size() == 3);
        check("destino permanece registrado uma única vez pela instrução",
            register_status.FindStatus(second_source).producer_positions ==
                std::vector<int>({25, 30, 35}));
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. HAZARDS ESTRUTURAIS — FU
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. HAZARDS ESTRUTURAIS — FU");

    section("7.1 FU esgotada -> AddIssue ok mas UpdateDependencies retorna false");
    {
        FUNCTIONAL_UNITS fu;
        fu.int_basic_alu.push_back(FU{});
        fu.memory_access.push_back(FU{});
        fu.int_mult_div_alu.push_back(FU{});
        fu.float_basic_alu.push_back(FU{});
        fu.float_mult_div_alu.push_back(FU{});
        fu.wr = 1;

        RegisterStatusTable register_status{MakeRegisterStatus()};
        RS rs0("int0"), rs1("int1");
        auto i0 = make_inst("add r3, r1, r2", 0);
        auto i1 = make_inst("sub r5, r3, r4", 1);
        rs0.AddIssue(i0, register_status, 1, true);
        rs1.AddIssue(i1, register_status, 1, true);

        rs0.UpdateDependencies(register_status, fu, 2);
        bool blocked = rs1.UpdateDependencies(register_status, fu, 2);
        check("Segunda RS bloqueada quando FU esgotada", !blocked);
        check("rs1 ainda em ISSUE",
              rs1.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::IS);
    }

    section("7.2 Grupos de FU são independentes — EX (int_basic_alu) vs MEM (memory_access)");
    {
        FUNCTIONAL_UNITS fu = makeFU(1); // 1 FU por grupo, mais fácil de saturar
        RegisterStatusTable register_status{MakeRegisterStatus()};

        RS rsA("load2"), rsB("load3");
        auto ldA = make_inst("l.d f2, 0(r1)", 0);
        auto ldB = make_inst("l.d f4, 0(r2)", 1);
        rsA.AddIssue(ldA, register_status, 1, true);
        rsB.AddIssue(ldB, register_status, 1, true);

        rsA.UpdateDependencies(register_status, fu, 2); // ocupa int_basic_alu
        rsA.UpdateCountdown(fu, 2);         // libera int_basic_alu, vai para MEM (sem FU ainda)

        bool rsB_ex = rsB.UpdateDependencies(register_status, fu, 2); // int_basic_alu livre de novo
        check("rsB usa int_basic_alu livre após rsA liberar", rsB_ex && rsB.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX);

        bool memA = rsA.UpdateDependencies(register_status, fu, 3); // memory_access livre
        check("rsA inicia MEM (memory_access livre)", memA);

        rsB.UpdateCountdown(fu, 3);         // libera int_basic_alu, tenta ir para MEM
        bool memB = rsB.UpdateDependencies(register_status, fu, 3);
        check("rsB bloqueado no MEM: memory_access já ocupado por rsA", !memB);
    }

    section("7.3 Roteamento de FU — INT_MUL/INT_DIV usam int_mult_div_alu; int_basic_alu não é afetado");
    {
        FUNCTIONAL_UNITS fu = makeFU(1); // 1 FU por grupo, fácil de saturar
        RegisterStatusTable register_status{MakeRegisterStatus()};

        RS rsMul("intmul0"), rsDiv("intdiv0"), rsAdd("intbasic0");
        auto iMul = make_inst("mult r1, r2", 0); // mult/div não têm destino explícito (vai para hi/lo)
        auto iDiv = make_inst("div r1, r2", 1);
        auto iAdd = make_inst("add r6, r1, r2", 2);
        rsMul.AddIssue(iMul, register_status, 1, true);
        rsDiv.AddIssue(iDiv, register_status, 1, true);
        rsAdd.AddIssue(iAdd, register_status, 1, true);

        bool mulEx = rsMul.UpdateDependencies(register_status, fu, 2);
        check("INT_MUL entra em EX e ocupa int_mult_div_alu", mulEx);
        check("INT_MUL: countdown == exLat == 4", rsMul.GetCountdown() == 4);

        bool divEx = rsDiv.UpdateDependencies(register_status, fu, 2);
        check("INT_DIV bloqueado: int_mult_div_alu saturado pelo MULT", !divEx);

        bool addEx = rsAdd.UpdateDependencies(register_status, fu, 2);
        check("INT_BASIC não é afetado pela saturação de int_mult_div_alu", addEx);
        check("INT_BASIC: countdown == exLat == 1", rsAdd.GetCountdown() == 1);
    }

    section("7.4 Roteamento de FU — FLOAT_BASIC usa float_basic_alu (independente de float_mult_div_alu)");
    {
        FUNCTIONAL_UNITS fu = makeFU(1);
        RegisterStatusTable register_status{MakeRegisterStatus()};

        RS rsFadd("fbasic0"), rsFadd2("fbasic1"), rsFmul("fmul5");
        auto iFadd = make_inst("add.d f2, f0, f4", 0);
        auto iFadd2 = make_inst("sub.d f8, f0, f4", 1);
        auto iFmul = make_inst("mul.d f6, f0, f4", 2);
        rsFadd.AddIssue(iFadd, register_status, 1, true);
        rsFadd2.AddIssue(iFadd2, register_status, 1, true);
        rsFmul.AddIssue(iFmul, register_status, 1, true);

        bool faddEx = rsFadd.UpdateDependencies(register_status, fu, 2);
        check("FLOAT_BASIC (add.d) entra em EX e ocupa float_basic_alu", faddEx);
        check("FLOAT_BASIC: countdown == exLat == 9", rsFadd.GetCountdown() == 9);

        bool fadd2Ex = rsFadd2.UpdateDependencies(register_status, fu, 2);
        check("Segundo FLOAT_BASIC (sub.d) bloqueado: float_basic_alu saturado", !fadd2Ex);

        bool fmulEx = rsFmul.UpdateDependencies(register_status, fu, 2);
        check("FLOAT_MUL não é afetado pela saturação de float_basic_alu", fmulEx);
        check("FLOAT_MUL: countdown == exLat == 14", rsFmul.GetCountdown() == 14);
    }

    section("7.5 Roteamento de FU — FLOAT_MUL/FLOAT_DIV compartilham float_mult_div_alu");
    {
        FUNCTIONAL_UNITS fu = makeFU(1);
        RegisterStatusTable register_status{MakeRegisterStatus()};

        RS rsFmul("fmul6"), rsFdiv("fdiv0"), rsFadd("fbasic2");
        auto iFmul = make_inst("mul.d f6, f0, f4", 0);
        auto iFdiv = make_inst("div.d f10, f0, f4", 1);
        auto iFadd = make_inst("add.d f12, f0, f4", 2);
        rsFmul.AddIssue(iFmul, register_status, 1, true);
        rsFdiv.AddIssue(iFdiv, register_status, 1, true);
        rsFadd.AddIssue(iFadd, register_status, 1, true);

        bool fmulEx = rsFmul.UpdateDependencies(register_status, fu, 2);
        check("FLOAT_MUL entra em EX e ocupa float_mult_div_alu", fmulEx);
        check("FLOAT_MUL: countdown == exLat == 14", rsFmul.GetCountdown() == 14);

        bool fdivEx = rsFdiv.UpdateDependencies(register_status, fu, 2);
        check("FLOAT_DIV bloqueado: float_mult_div_alu saturado pelo FLOAT_MUL", !fdivEx);

        bool faddEx = rsFadd.UpdateDependencies(register_status, fu, 2);
        check("FLOAT_BASIC não é afetado pela saturação de float_mult_div_alu", faddEx);
        check("FLOAT_DIV: exLat esperado == 40 (checado isoladamente)", make_inst("div.d f0, f2, f4")->GetExLatency() == 40);
    }

    std::cout << "\n";
    print_title("8. DISTRIBUIÇÃO DE BROADCAST");

    section("8.1 Um evento resolve um ou múltiplos consumidores");
    {
        RegisterStatusTable register_status{MakeRegisterStatus()};
        register_status.AllocateProducer(R(1), 10, "producer", 1);

        RESERVATION_STATION station;
        station.int_basic.push_back(RS("consumer0"));
        station.int_basic.push_back(RS("consumer1"));
        station.int_basic[0].AddIssue(
            make_inst("add r3, r1, r2", 20),
            register_status,
            2,
            true
        );
        station.int_basic[1].AddIssue(
            make_inst("sub r4, r1, r2", 21),
            register_status,
            2,
            true
        );

        station.ResolveBroadcast({10, R(1)});
        check("primeiro consumidor resolve Q",
            station.int_basic[0].GetExDependencies()[0] == -1);
        check("segundo consumidor resolve o mesmo Q",
            station.int_basic[1].GetExDependencies()[0] == -1);
    }

    section("8.2 Destino ausente ou produtor diferente não altera Q");
    {
        RegisterStatusTable register_status{MakeRegisterStatus()};
        register_status.AllocateProducer(R(1), 10, "producer", 1);

        RESERVATION_STATION station;
        station.int_basic.push_back(RS("consumer"));
        station.int_basic[0].AddIssue(
            make_inst("add r3, r1, r2", 20),
            register_status,
            2,
            true
        );

        station.ResolveBroadcast({10, R(8)});
        check("destino sem consumidor preserva Q",
            station.int_basic[0].GetExDependencies()[0] == 10);
        station.ResolveBroadcast({11, R(1)});
        check("produtor diferente preserva Q",
            station.int_basic[0].GetExDependencies()[0] == 10);
    }

    section("8.3 Dois destinos do mesmo produtor são eventos independentes");
    {
        RegisterStatusTable register_status{MakeRegisterStatus()};
        register_status.AllocateProducer(HI(), 10, "producer", 1);
        register_status.AllocateProducer(LO(), 10, "producer", 1);

        RESERVATION_STATION station;
        station.int_basic.push_back(RS("hi_consumer"));
        station.int_basic.push_back(RS("lo_consumer"));
        station.int_basic[0].AddIssue(
            make_inst("mfhi r3", 20),
            register_status,
            2,
            true
        );
        station.int_basic[1].AddIssue(
            make_inst("mflo r4", 21),
            register_status,
            2,
            true
        );

        station.ResolveBroadcast({10, HI()});
        check("broadcast de HI não resolve LO",
            station.int_basic[0].GetExDependencies()[0] == -1 &&
            station.int_basic[1].GetExDependencies()[0] == 10);
        station.ResolveBroadcast({10, LO()});
        check("broadcast de LO conclui o segundo destino",
            station.int_basic[1].GetExDependencies()[0] == -1);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
