CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g
LDFLAGS  =

SRCDIR   = _code
TSTDIR   = $(SRCDIR)/testbenchs
OBJDIR   = build
INPUTDIR = test_cases/inputs
EXPDIR   = test_cases/expected

SRCS     = $(wildcard $(SRCDIR)/*.cpp)
OBJS     = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
MAIN_OBJ = $(OBJDIR)/Main.o

EXEC     = executable

TB_NAMES = Componentes Instrucao ReservationStations Thread Processador
TB_BINS  = $(addprefix $(OBJDIR)/tb_,$(TB_NAMES))

INPUTS   = $(wildcard $(INPUTDIR)/*.txt)

.PHONY: all testbenchs test simtest simtest-completo simtest-update clean rebuild

all: $(EXEC) testbenchs

$(OBJDIR):
	mkdir -p $(OBJDIR)

# --- Compilação dos objetos ---
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- Executável principal ---
$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# --- Testbenchs ---
$(OBJDIR)/tb_Componentes: $(OBJDIR)/Componentes.o $(TSTDIR)/tb_Componentes.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Componentes.cpp $(OBJDIR)/Componentes.o -o $@

$(OBJDIR)/tb_Instrucao: $(OBJDIR)/Componentes.o $(OBJDIR)/Instrucao.o $(TSTDIR)/tb_Instrucao.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Instrucao.cpp $(OBJDIR)/Componentes.o $(OBJDIR)/Instrucao.o -o $@

$(OBJDIR)/tb_ReservationStations: $(OBJDIR)/Componentes.o $(OBJDIR)/Instrucao.o $(OBJDIR)/ReservationStations.o $(TSTDIR)/tb_ReservationStations.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_ReservationStations.cpp $(OBJDIR)/Componentes.o $(OBJDIR)/Instrucao.o $(OBJDIR)/ReservationStations.o -o $@

$(OBJDIR)/tb_Thread: $(OBJDIR)/Componentes.o $(OBJDIR)/Instrucao.o $(OBJDIR)/ReservationStations.o $(OBJDIR)/Thread.o $(TSTDIR)/tb_Thread.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Thread.cpp $(OBJDIR)/Componentes.o $(OBJDIR)/Instrucao.o $(OBJDIR)/ReservationStations.o $(OBJDIR)/Thread.o -o $@

$(OBJDIR)/tb_Processador: $(OBJS) $(TSTDIR)/tb_Processador.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(TSTDIR)/tb_Processador.cpp $(filter-out $(MAIN_OBJ),$(OBJS)) -o $@

testbenchs: $(TB_BINS)

# --- Testes unitários (apenas falhas) ---
test: testbenchs
	@total=0; fail=0; failed_list=""; \
	echo ""; \
	for tb in $(TB_BINS); do \
		total=$$((total + 1)); \
		name=$$(basename $$tb); \
		printf "  %-25s" "$$name"; \
		result=$$($$tb 2>&1); \
		last=$$(echo "$$result" | tail -1); \
		if echo "$$last" | grep -qE "^Resultado: [0-9]+ OK, [1-9][0-9]* FALHOU"; then \
			fail=$$((fail + 1)); \
			failed_list="$$failed_list\n    $$name"; \
			echo "$$(printf '\033[0;31mFALHOU\033[0m')"; \
			echo "$$result" | grep "\[FALHOU\]" | sed 's/.*/    &/'; \
		else \
			echo "$$(printf '\033[0;32mOK\033[0m')"; \
		fi; \
	done; \
	echo ""; \
	if [ $$fail -eq 0 ]; then \
		echo "$$(printf '\033[0;32mTodos os %d testbenchs OK!\033[0m' $$total)"; \
	else \
		echo "$$(printf '\033[0;31m%d de %d testbenchs FALHARAM:\033[0m' $$fail $$total)$$failed_list"; \
		exit 1; \
	fi

# --- Testes de simulação (apenas diferenças) ---
simtest: $(EXEC)
	@total=0; diff_count=0; missing=0; failed_list=""; \
	echo ""; \
	mkdir -p $(EXPDIR); \
	for input in $(INPUTS); do \
		total=$$((total + 1)); \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		if [ ! -f $$expected ]; then \
			missing=$$((missing + 1)); \
			./$(EXEC) < $$input > $$expected 2>&1; \
			echo "  $$name -> referência gerada"; \
		fi; \
	done; \
	if [ $$missing -gt 0 ]; then echo ""; fi; \
	echo "Comparando saídas..."; echo ""; \
	for input in $(INPUTS); do \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		printf "  %-40s" "$$name"; \
		./$(EXEC) < $$input > /tmp/$$name.out 2>&1; \
		if diff -q /tmp/$$name.out $$expected > /dev/null 2>&1; then \
			echo "$$(printf '\033[0;32mOK\033[0m')"; \
		else \
			diff_count=$$((diff_count + 1)); \
			failed_list="$$failed_list\n    $$name"; \
			echo "$$(printf '\033[0;31mDIFERENTE\033[0m')"; \
		fi; \
	done; \
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
	fi

# --- Testes completos (mostra tudo) ---
simtest-completo: $(EXEC) testbenchs
	@has_fail=0; \
	echo ""; \
	echo "=========================================="; \
	echo "  TESTBENCHS"; \
	echo "=========================================="; \
	echo ""; \
	for tb in $(TB_BINS); do \
		name=$$(basename $$tb); \
		echo "--- $$name ---"; \
		$$tb 2>&1; \
		echo ""; \
	done; \
	echo "=========================================="; \
	echo "  CASOS DE SIMULACAO"; \
	echo "=========================================="; \
	echo ""; \
	mkdir -p $(EXPDIR); \
	for input in $(INPUTS); do \
		name=$$(basename $$input .txt); \
		expected=$(EXPDIR)/$$name.expected; \
		echo "--- $$name ---"; \
		if [ ! -f $$expected ]; then \
			./$(EXEC) < $$input > $$expected 2>&1; \
			echo "[OK - referencia gerada]"; \
		else \
			./$(EXEC) < $$input > /tmp/$$name.full 2>&1; \
			if diff -q /tmp/$$name.full $$expected > /dev/null 2>&1; then \
				echo "[OK]"; \
			else \
				echo "[DIFERENTE]"; \
				has_fail=1; \
			fi; \
		fi; \
		cat $(EXPDIR)/$$name.expected; \
		echo ""; \
	done; \
	if [ $$has_fail -eq 0 ]; then \
		echo "$$(printf '\033[0;32mTodos os testes OK!\033[0m')"; \
	else \
		echo "$$(printf '\033[0;31mHa diferencas nos casos de simulacao.\033[0m')"; \
	fi; \
	exit $$has_fail

# --- Atualizar referências ---
simtest-update: $(EXEC)
	@mkdir -p $(EXPDIR); \
	total=0; \
	echo ""; \
	for input in $(INPUTS); do \
		total=$$((total + 1)); \
		name=$$(basename $$input .txt); \
		printf "  Atualizando %-40s" "$$name"; \
		./$(EXEC) < $$input > $(EXPDIR)/$$name.expected 2>&1; \
		echo "$$(printf '\033[0;32mOK\033[0m')"; \
	done; \
	echo ""; \
	echo "$$(printf '\033[0;32mReferencias atualizadas para %d casos.\033[0m' $$total)"

# --- Limpeza ---
clean:
	rm -rf $(OBJDIR) $(EXEC)

rebuild: clean all
