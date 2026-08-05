CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -MMD -MP -I$(SRCDIR)/headers
LDFLAGS  =

SRCDIR   = .
TSTDIR   = testbenchs
OBJDIR   = build
INPUTDIR = test-cases/inputs
EXPDIR   = test-cases/expected

# Módulo multi-arquitetura (Code/Instruction/) — mantido em pasta separada
# para receber as novas arquiteturas (ver multiarchitecture.md).
SRCS     = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/Instruction/*.cpp)
OBJS     = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
MAIN_OBJ = $(OBJDIR)/Main.o

EXEC     = $(OBJDIR)/executable

TB_NAMES = Components Instruction ReservationStations Thread Processor
TB_BINS  = $(addprefix $(OBJDIR)/tb_,$(TB_NAMES))

INPUTS   = $(wildcard $(INPUTDIR)/*.txt)

DEPS     = $(OBJS:.o=.d)

.PHONY: all testbenchs test simtest simtest-completo simtest-update clean rebuild

all: $(EXEC) testbenchs

$(OBJDIR):
	mkdir -p $(OBJDIR)

# --- Compilação dos objetos com dependências automáticas ---
# - mkdir -p $(dir $@): garante subdiretórios (ex.: build/Instruction/) para o módulo multi-arquitetura.
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

# --- Executável principal (dentro de build/) ---
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# --- Testbenchs (suas regras originais com dependências) ---
# --- Testbenchs (regra genérica com dependências automáticas) ---
$(OBJDIR)/tb_%: $(TSTDIR)/tb_%.cpp $(OBJS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(OBJDIR)/$*.d $< $(filter-out $(MAIN_OBJ),$(OBJS)) -o $@

# Inclui dependências dos testbenchs
-include $(patsubst $(OBJDIR)/%,$(OBJDIR)/%.d,$(TB_BINS))

testbenchs: $(TB_BINS)

# --- Testes unitários (falhas + pontuacao; crash/abort conta como falha) ---
test: testbenchs
	@total_ok=0; total_all=0; total_tb=0; fail_tb=0; failed_list=""; \
	echo ""; \
	echo "═══ TESTBENCHS ══════════════════════════════════════"; \
	nome=0; \
	for tb in $(TB_BINS); do \
		n=$$(basename $$tb); len=$${#n}; \
		if [ $$len -gt $$nome ]; then nome=$$len; fi; \
	done; \
	nome=$$((nome + 12)); \
	status=6; bracket=9; sep=1; rightw=$$((bracket + sep + status)); \
	for tb in $(TB_BINS); do \
		total_tb=$$((total_tb + 1)); \
		name=$$(basename $$tb); \
		result=$$($$tb 2>&1); \
		last=$$(echo "$$result" | tail -1); \
		if echo "$$last" | grep -qE "^Resultado: [0-9]+ OK, [0-9]+ FALHOU"; then \
			ok=$$(echo "$$last" | sed -E 's/^Resultado: ([0-9]+) OK.*/\1/'); \
			bad=$$(echo "$$last" | sed -E 's/.*OK, ([0-9]+) FALHOU.*/\1/'); \
			count=$$((ok + bad)); \
			total_ok=$$((total_ok + ok)); \
			total_all=$$((total_all + count)); \
			if [ "$$bad" -gt 0 ]; then \
				fail_tb=$$((fail_tb + 1)); \
				failed_list="$$failed_list\n    $$name"; \
				printf "  %-*s [%3d/%-3d] \033[0;31m%*s\033[0m\n" "$$nome" "$$name" "$$ok" "$$count" "$$status" "FALHOU"; \
				echo "$$result" | grep "\[FALHOU\]" | sed 's/.*/    &/'; \
			else \
				printf "  %-*s [%3d/%-3d] \033[0;32m%*s\033[0m\n" "$$nome" "$$name" "$$ok" "$$count" "$$status" "OK"; \
			fi; \
		else \
			fail_tb=$$((fail_tb + 1)); \
			failed_list="$$failed_list\n    $$name (CRASH/ABORT)"; \
			crash="CRASH/ABORT"; pad=$$((rightw - $${#crash})); \
			printf "  %-*s %*s\033[0;31m%s\033[0m\n" "$$nome" "$$name" "$$pad" "" "$$crash"; \
			echo "$$result" | tail -5 | sed 's/^/    /'; \
		fi; \
	done; \
	echo "═════════════════════════════════════════════════════"; \
	echo ""; \
	if [ $$fail_tb -eq 0 ]; then \
		echo "$$(printf '\033[0;32mTodos os %d testbenchs OK! [%d/%d]\033[0m' $$total_tb $$total_ok $$total_all)"; \
	else \
		echo "$$(printf '\033[0;31m%d de %d testbenchs FALHARAM [%d/%d]:\033[0m' $$fail_tb $$total_tb $$total_ok $$total_all)$$failed_list"; \
		exit 1; \
	fi; \
	echo "";

# --- Testes de simulação (apenas diferenças) ---
simtest: $(EXEC)
	@total=0; diff_count=0; missing=0; failed_list=""; \
	echo ""; \
	echo "═══ SIMULAÇÃO ═══════════════════════════════════════"; \
	mkdir -p $(EXPDIR); \
	nome=0; \
	for input in $(INPUTS); do \
		n=$$(basename $$input .txt); len=$${#n}; \
		if [ $$len -gt $$nome ]; then nome=$$len; fi; \
	done; \
	nome=$$((nome + 20)); \
	status=0; \
	for s in "OK" "DIFERENTE"; do \
		len=$${#s}; if [ $$len -gt $$status ]; then status=$$len; fi; \
	done; \
	status=$$((status + 1)); \
	for input in $(INPUTS); do \
		total=$$((total + 1)); \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		if [ ! -f $$expected ]; then \
			missing=$$((missing + 1)); \
			$(EXEC) < $$input > $$expected 2>&1; \
			echo "  $$name -> referência gerada"; \
		fi; \
	done; \
	if [ $$missing -gt 0 ]; then echo ""; fi; \
	for input in $(INPUTS); do \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		$(EXEC) < $$input > /tmp/$$name.out 2>&1; \
		if diff -q /tmp/$$name.out $$expected > /dev/null 2>&1; then \
			printf "  %-*s \033[0;32m%*s\033[0m\n" "$$nome" "$$name" "$$status" "OK"; \
		else \
			diff_count=$$((diff_count + 1)); \
			failed_list="$$failed_list\n    $$name"; \
			printf "  %-*s \033[0;31m%*s\033[0m\n" "$$nome" "$$name" "$$status" "DIFERENTE"; \
		fi; \
	done; \
	echo "═════════════════════════════════════════════════════"; \
	echo ""; \
	if [ $$diff_count -eq 0 ]; then \
		echo "$$(printf '\033[0;32mTodos os %d casos de teste OK!\033[0m' $$total)"; \
	else \
		echo "$$(printf '\033[0;33m%d de %d casos de teste diferentes:\033[0m' $$diff_count $$total)$$failed_list"; \
		echo ""; \
		for input in $(INPUTS); do \
			name=$$(basename $$input .txt); \
			expected=$(EXPDIR)/$$name.expected; \
			if ! diff -q /tmp/$$name.out $$expected > /dev/null 2>&1; then \
				echo "$$(printf '\033[0;33m--- %s ---\033[0m' $$name)"; \
				diff /tmp/$$name.out $$expected; \
				echo ""; \
			fi; \
		done; \
		exit 1; \
	fi; \
	echo "";

# --- Testes completos (resumo no console + grava cada teste em build/simtest/output-<nome>.txt) ---
simtest-completo: $(EXEC) testbenchs
	@outdir=$(OBJDIR)/simtest; \
	echo ""; \
	echo "═══ COMPLETO ════════════════════════════════════════"; \
	mkdir -p $$outdir $(EXPDIR); \
	has_fail=0; fail_count=0; \
	nome=0; \
	for tb in $(TB_BINS); do \
		n=$$(basename $$tb); len=$${#n}; \
		if [ $$len -gt $$nome ]; then nome=$$len; fi; \
	done; \
	for input in $(INPUTS); do \
		n=$$(basename $$input .txt); len=$${#n}; \
		if [ $$len -gt $$nome ]; then nome=$$len; fi; \
	done; \
	nome=$$((nome + 12)); \
	tb_status=6; bracket=9; sep=1; rightw=$$((bracket + sep + tb_status)); \
	sim_status=0; \
	for s in "OK" "DIFERENTE" "OK - referencia gerada"; do \
		len=$${#s}; if [ $$len -gt $$sim_status ]; then sim_status=$$len; fi; \
	done; \
	sim_status=$$((sim_status + 1)); \
	tb_count=0; input_count=0; \
	failed_list=""; \
	echo "Outputs enviados para: $$outdir/"; \
	echo ""; \
	echo "- Testbenchs:"; \
	for tb in $(TB_BINS); do \
		name=$$(basename $$tb); \
		outf="$$outdir/output-$$name.txt"; \
		result=$$($$tb 2>&1); \
		last=$$(echo "$$result" | tail -1); \
		if echo "$$last" | grep -qE "^Resultado: [0-9]+ OK, [0-9]+ FALHOU"; then \
			ok=$$(echo "$$last" | sed -E 's/^Resultado: ([0-9]+) [OK].*/\1/'); \
			bad=$$(echo "$$last" | sed -E 's/.*[OK], ([0-9]+) [FALHOU].*/\1/'); \
			count=$$((ok + bad)); \
			if [ "$$bad" -gt 0 ]; then \
				has_fail=1; fail_count=$$((fail_count + 1)); \
				failed_list="$$failed_list\n    $$name"; \
				printf "  %-*s [%3d/%-3d] \033[0;31m%*s\033[0m\n" "$$nome" "$$name" "$$ok" "$$count" "$$tb_status" "FALHOU"; \
				echo "$$result" | grep "\[FALHOU\]" | sed 's/.*/    &/'; \
			else \
				printf "  %-*s [%3d/%-3d] \033[0;32m%*s\033[0m\n" "$$nome" "$$name" "$$ok" "$$count" "$$tb_status" "OK"; \
			fi; \
		else \
			has_fail=1; fail_count=$$((fail_count + 1)); \
			failed_list="$$failed_list\n    $$name (CRASH/ABORT)"; \
			crash="CRASH/ABORT"; pad=$$((rightw - $${#crash})); \
			printf "  %-*s %*s\033[0;31m%s\033[0m\n" "$$nome" "$$name" "$$pad" "" "$$crash"; \
			echo "$$result" | tail -5 | sed 's/^/    /'; \
		fi; \
		{ \
			echo "$$name"; \
			echo ""; \
			echo "$$result"; \
			echo ""; \
		} > "$$outf"; \
		tb_count=$$((tb_count + 1)); \
	done; \
	echo ""; \
	echo "- Casos de Simulação:"; \
	nome=$$((nome - 7)); \
	for input in $(INPUTS); do \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		outf="$$outdir/output-$$name.txt"; \
		if [ ! -f $$expected ]; then \
			$(EXEC) < $$input > $$expected 2>&1; \
			printf "  %-*s \033[0;33m%*s\033[0m\n" "$$nome" "$$name" "$$sim_status" "[OK - referencia gerada]"; \
			{ \
				echo "$$name"; \
				echo ""; \
				cat $$expected; \
				echo ""; \
			} > "$$outf"; \
		else \
			$(EXEC) < $$input > /tmp/$$name.full 2>&1; \
			if diff -q /tmp/$$name.full $$expected > /dev/null 2>&1; then \
				printf "  %-*s \033[0;32m%*s\033[0m\n" "$$nome" "$$name" "$$sim_status" "OK"; \
				{ \
					echo "$$name"; \
					echo ""; \
					cat $$expected; \
					echo ""; \
				} > "$$outf"; \
			else \
				printf "  %-*s \033[0;31m%*s\033[0m\n" "$$nome" "$$name" "$$sim_status" "DIFERENTE"; \
				has_fail=1; fail_count=$$((fail_count + 1)); \
				failed_list="$$failed_list\n    $$name"; \
				{ \
					echo "  $$name"; \
					echo ""; \
					echo "[DIFERENTE]"; \
					diff /tmp/$$name.full $$expected; \
					echo ""; \
				} > "$$outf"; \
			fi; \
		fi; \
		input_count=$$((input_count + 1)); \
	done; \
	total=$$((tb_count + input_count)); \
	echo ""; \
	echo "═════════════════════════════════════════════════════"; \
	echo ""; \
	if [ $$has_fail -eq 0 ]; then \
		echo "$$(printf '\033[0;32mTodos os %d testes OK!\033[0m' $$total)"; \
	else \
		echo "$$(printf '\033[0;31m%d de %d testes FALHARAM:\033[0m' $$fail_count $$total)$$failed_list"; \
		echo "  Outputs em: $$outdir/"; \
		exit 1; \
	fi; \
	echo ""; \
	exit 0

# --- Atualizar referências ---
simtest-update: $(EXEC)
	@mkdir -p $(EXPDIR); \
	total=0; \
	echo ""; \
	echo "═══ UPDATE ══════════════════════════════════════════"; \
	nome=0; \
	for input in $(INPUTS); do \
		n=$$(basename $$input .txt); len=$${#n}; \
		if [ $$len -gt $$nome ]; then nome=$$len; fi; \
	done; \
	nome=$$((nome + 17)); \
	for input in $(INPUTS); do \
		total=$$((total + 1)); \
		name=$$(basename $$input .txt); \
		printf "  Atualizando %-*s" "$$nome" "$$name"; \
		$(EXEC) < $$input > $(EXPDIR)/$$name.expected 2>&1; \
		echo "$$(printf '\033[0;32mOK\033[0m')"; \
	done; \
	echo "═════════════════════════════════════════════════════"; \
	echo ""; \
	echo "$$(printf '\033[0;32mReferencias atualizadas para %d casos.\033[0m' $$total)"; \
	echo ""

# --- Limpeza ---
clean:
	rm -rf $(OBJDIR)

rebuild: clean all
