CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -MMD -MP
LDFLAGS  =

SRCDIR   = .
TSTDIR   = testbenchs
OBJDIR   = build
INPUTDIR = test-cases/inputs
EXPDIR   = test-cases/expected

SRCS     = $(wildcard $(SRCDIR)/*.cpp)
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
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

# --- Executável principal (dentro de build/) ---
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# --- Testbenchs ---
$(OBJDIR)/tb_Components: $(OBJDIR)/Components.o $(TSTDIR)/tb_Components.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Components.cpp $(OBJDIR)/Components.o -o $@

$(OBJDIR)/tb_Instruction: $(OBJDIR)/Components.o $(OBJDIR)/Instruction.o $(TSTDIR)/tb_Instruction.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Instruction.cpp $(OBJDIR)/Components.o $(OBJDIR)/Instruction.o -o $@

$(OBJDIR)/tb_ReservationStations: $(OBJDIR)/Components.o $(OBJDIR)/Instruction.o $(OBJDIR)/ReservationStations.o $(TSTDIR)/tb_ReservationStations.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_ReservationStations.cpp $(OBJDIR)/Components.o $(OBJDIR)/Instruction.o $(OBJDIR)/ReservationStations.o -o $@

$(OBJDIR)/tb_Thread: $(OBJDIR)/Components.o $(OBJDIR)/Instruction.o $(OBJDIR)/ReservationStations.o $(OBJDIR)/Thread.o $(TSTDIR)/tb_Thread.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Thread.cpp $(OBJDIR)/Components.o $(OBJDIR)/Instruction.o $(OBJDIR)/ReservationStations.o $(OBJDIR)/Thread.o -o $@

$(OBJDIR)/tb_Processor: $(OBJS) $(TSTDIR)/tb_Processor.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Processor.cpp $(filter-out $(MAIN_OBJ),$(OBJS)) -o $@

testbenchs: $(TB_BINS)

# --- Testes unitários (falhas + pontuacao; crash/abort conta como falha) ---
test: testbenchs
	@total_ok=0; total_all=0; total_tb=0; fail_tb=0; failed_list=""; \
	echo ""; \
	echo "════════════════ TESTBENCHS ════════════════"; \
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
				printf "  %-29s [%3d/%-3d] %s\n" "$$name" "$$ok" "$$count" "$$(printf '\033[0;31mFALHOU\033[0m')"; \
				echo "$$result" | grep "\[FALHOU\]" | sed 's/.*/    &/'; \
			else \
				printf "  %-29s [%3d/%-3d] %s\n" "$$name" "$$ok" "$$count" "$$(printf '\033[0;32mOK\033[0m')"; \
			fi; \
		else \
			fail_tb=$$((fail_tb + 1)); \
			failed_list="$$failed_list\n    $$name (CRASH/ABORT)"; \
			printf "  %-29s %s\n" "$$name" "$$(printf '\033[0;31mCRASH/ABORT\033[0m')"; \
			echo "$$result" | tail -5 | sed 's/^/    /'; \
		fi; \
	done; \
	echo "════════════════════════════════════════════"; \
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
	echo "═════════════════ SIMULACAO ════════════════"; \
	mkdir -p $(EXPDIR); \
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
	echo "Comparando saídas..."; echo ""; \
	for input in $(INPUTS); do \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		printf "  %-40s" "$$name"; \
		$(EXEC) < $$input > /tmp/$$name.out 2>&1; \
		if diff -q /tmp/$$name.out $$expected > /dev/null 2>&1; then \
			echo "$$(printf '\033[0;32mOK\033[0m')"; \
		else \
			diff_count=$$((diff_count + 1)); \
			failed_list="$$failed_list\n    $$name"; \
			echo "$$(printf '\033[0;31mDIFERENTE\033[0m')"; \
		fi; \
	done; \
	echo "════════════════════════════════════════════"; \
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

# --- Testes completos (grava tudo em build/output.txt; so mostra diff quando ha diferenca) ---
simtest-completo: $(EXEC) testbenchs
	@out=$(OBJDIR)/output.txt; \
	echo ""; \
	echo "═════════════════ COMPLETO ═════════════════"; \
	mkdir -p $(OBJDIR) $(EXPDIR); \
	has_fail=0; \
	tb_count=0; input_count=0; \
	{ \
		echo ""; \
		echo "################################################"; \
		echo "###                TESTBENCHS                ###"; \
		echo "################################################"; \
		for tb in $(TB_BINS); do \
			name=$$(basename $$tb); \
			echo ""; \
			echo "================================================"; \
			echo "  $$name"; \
			echo "================================================"; \
			$$tb 2>&1; \
			tb_count=$$((tb_count + 1)); \
		done; \
		echo ""; \
		echo ""; \
		echo "################################################"; \
		echo "###            CASOS DE SIMULACAO            ###"; \
		echo "################################################"; \
		for input in $(INPUTS); do \
			name=$$(basename $$input .txt); \
			expected=$(EXPDIR)/$$name.expected; \
			echo ""; \
			echo "================================================"; \
			echo "  $$name"; \
			echo "================================================"; \
			if [ ! -f $$expected ]; then \
				$(EXEC) < $$input > $$expected 2>&1; \
				echo "[OK - referencia gerada]"; \
				cat $$expected; \
			else \
				$(EXEC) < $$input > /tmp/$$name.full 2>&1; \
				if diff -q /tmp/$$name.full $$expected > /dev/null 2>&1; then \
					echo "[OK]"; \
					cat $$expected; \
				else \
					echo "[DIFERENTE]"; \
					has_fail=1; \
					diff /tmp/$$name.full $$expected; \
				fi; \
			fi; \
			input_count=$$((input_count + 1)); \
		done; \
		echo ""; \
		if [ $$has_fail -eq 0 ]; then \
			echo "Todos os testes OK!"; \
		else \
			echo "Ha diferencas nos casos de simulacao."; \
		fi; \
	} > $$out 2>&1; \
	total=$$((tb_count + input_count)); \
	echo "  Saida completa em $$out"; \
	echo "════════════════════════════════════════════"; \
	echo ""; \
	if [ $$has_fail -eq 0 ]; then \
		printf '\033[0;32mTodos os %d testes OK!\033[0m\n' "$$total"; \
	else \
		printf '\033[0;31mHa diferencas nos casos de simulacao.\033[0m Veja %s\n' "$$out"; \
	fi; \
	echo ""; \
	exit $$has_fail

# --- Atualizar referências ---
simtest-update: $(EXEC)
	@mkdir -p $(EXPDIR); \
	total=0; \
	echo ""; \
	echo "══════════════════ UPDATE ══════════════════"; \
	for input in $(INPUTS); do \
		total=$$((total + 1)); \
		name=$$(basename $$input .txt); \
		printf "  Atualizando %-28s" "$$name"; \
		$(EXEC) < $$input > $(EXPDIR)/$$name.expected 2>&1; \
		echo "$$(printf '\033[0;32mOK\033[0m')"; \
	done; \
	echo "════════════════════════════════════════════"; \
	echo ""; \
	echo "$$(printf '\033[0;32mReferencias atualizadas para %d casos.\033[0m' $$total)"; \
	echo ""

# --- Limpeza ---
clean:
	rm -rf $(OBJDIR)

rebuild: clean all
