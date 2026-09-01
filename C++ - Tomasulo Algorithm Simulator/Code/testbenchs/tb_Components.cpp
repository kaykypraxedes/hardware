// Testbench isolado de Components.cpp.
#include "../headers/Components.h"
#include "tb_Helpers.h"
#include <iostream>
#include <vector>

using namespace processor;

int main() {
    print_title("1. REFERÊNCIA ARQUITETURAL");

    section("1.1 Register mantém somente identidade");
    {
        const Register empty;
        const Register masked('B', 3, 0x02);
        check("referência vazia preserva defaults",
            empty.GetType() == 'Z' && empty.GetId() == -1 && empty.GetMask() == 255);
        check("referência mascarada preserva (type,id,mask)",
            masked.GetType() == 'B' && masked.GetId() == 3 && masked.GetMask() == 0x02);
    }

    print_title("2. LAYOUT E LOCALIZAÇÃO");

    section("2.1 Ordem e identidade exata");
    {
        RegisterStatusTable table;
        table.AddReference(Register('B', 0, 0x01));
        table.AddReference(Register('B', 0, 0x02));
        table.AddReference(Register('W', 0, 0x03));

        const std::vector<Register> references{table.GetReferences()};
        check("layout preserva quantidade e ordem", table.Size() == 3 && references.size() == 3);
        check("al e ah permanecem referências distintas",
            references[0].GetMask() == 0x01 && references[1].GetMask() == 0x02);
        check("visualização inicial está livre",
            !table.FindStatus(Register('W', 0, 0x03)).busy);
    }

    section("2.2 Duplicata e referência ausente abortam");
    {
        check("referência duplicada aborta", Aborts([]() {
            RegisterStatusTable table;
            table.AddReference(Register('R', 1));
            table.AddReference(Register('R', 1));
        }));
        check("consulta ausente aborta", Aborts([]() {
            RegisterStatusTable table;
            table.AddReference(Register('R', 1));
            table.FindStatus(Register('R', 2));
        }));
    }

    print_title("3. PRODUTORES FUNCIONAIS");

    section("3.1 Produtor anterior, igual e posterior");
    {
        const Register reference('R', 4);
        RegisterStatusTable table;
        table.AddReference(reference);
        table.AllocateProducer(reference, 2, "old", 1);
        table.AllocateProducer(reference, 5, "same", 2);
        table.AllocateProducer(reference, 8, "new", 3);

        check("consumidora escolhe maior produtor anterior",
            table.FindLatestProducerBefore(reference, 7) == 5);
        check("posição igual não cria autodependência",
            table.FindLatestProducerBefore(reference, 5) == 2);
        check("produtor posterior é ignorado",
            table.FindLatestProducerBefore(reference, 4) == 2);
        check("nenhum produtor anterior retorna -1",
            table.FindLatestProducerBefore(reference, 2) == -1);
    }

    section("3.2 WAW e produtor concluído mais recente");
    {
        const Register reference('F', 32);
        RegisterStatusTable table;
        table.AddReference(reference);
        table.AllocateProducer(reference, 2, "old", 1);
        table.AllocateProducer(reference, 5, "latest", 2);

        check("busy deriva dois produtores pendentes", table.IsBusy(reference));
        check("WAW seleciona produtor lógico mais recente",
            table.FindLatestProducerBefore(reference, 9) == 5);
        check("produtor pendente não está resolvido",
            !table.IsProducerResolved(reference, 5));

        table.DeallocateProducer(reference, 5, 4);
        check("produtor concluído continua sendo o mais recente",
            table.FindLatestProducerBefore(reference, 9) == 5);
        check("mais recente concluído está pronto",
            table.IsProducerResolved(reference, 5));
        check("não recua para produtor antigo pendente", !table.IsProducerResolved(reference, 2));
        check("busy permanece derivado do produtor antigo", table.IsBusy(reference));

        table.DeallocateProducer(reference, 2, 5);
        check("busy cai quando nenhum produtor está pendente", !table.IsBusy(reference));
    }

    section("3.3 Produtor desconhecido e conclusão repetida");
    {
        const Register reference('R', 7);
        RegisterStatusTable table;
        table.AddReference(reference);
        table.AllocateProducer(reference, 3, "int0", 2);

        check("produtor desconhecido não está resolvido",
            !table.IsProducerResolved(reference, 99));
        check("conclusão desconhecida retorna false",
            !table.DeallocateProducer(reference, 99, 4));
        check("primeira conclusão retorna true",
            table.DeallocateProducer(reference, 3, 5));
        check("conclusão repetida retorna false",
            !table.DeallocateProducer(reference, 3, 6));
    }

    section("3.4 Posições inválidas e duplicadas abortam");
    {
        check("posição negativa aborta", Aborts([]() {
            const Register reference('R', 1);
            RegisterStatusTable table;
            table.AddReference(reference);
            table.AllocateProducer(reference, -1, "int0", 1);
        }));
        check("posição duplicada aborta", Aborts([]() {
            const Register reference('R', 1);
            RegisterStatusTable table;
            table.AddReference(reference);
            table.AllocateProducer(reference, 4, "int0", 1);
            table.AllocateProducer(reference, 4, "int1", 2);
        }));
    }

    section("3.5 Broadcast é transitório e conclui uma única vez");
    {
        const Register reference('R', 9);
        RegisterStatusTable table(std::vector<Register>{reference});
        table.AllocateProducer(reference, 4, "int0", 2);

        const CDB_BROADCAST broadcast{4, reference};
        check("criar evento não altera estado persistente",
            !table.IsProducerResolved(reference, 4) && table.IsBusy(reference));
        check("primeiro broadcast conclui produtor",
            broadcast.CompleteProducer(table, 5));
        check("conclusão fica persistida no Register Status",
            table.IsProducerResolved(reference, 4) && !table.IsBusy(reference));
        check("broadcast repetido retorna false",
            !broadcast.CompleteProducer(table, 6));
    }

    print_title("4. TRACE SOMENTE LEITURA");

    section("4.1 Trace não decide estado funcional");
    {
        const Register reference('R', 6);
        RegisterStatusTable table;
        table.AddReference(reference);
        table.AllocateProducer(reference, 1, "load0", 2);
        table.DeallocateProducer(reference, 1, 4);
        table.AllocateProducer(reference, 9, "load0", 10);

        const REGISTER_STATUS_VIEW view{table.FindStatus(reference)};
        check("histórico preserva posições", view.producer_positions == std::vector<int>({1, 9}));
        check("histórico preserva RS reutilizada", view.allocated_rs == std::vector<std::string>({"load0", "load0"}));
        check("histórico preserva ciclos", view.allocation_times == std::vector<int>({2, 4, 10, -1}));
        check("view deriva busy do registro funcional", view.busy && table.IsBusy(reference));
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
