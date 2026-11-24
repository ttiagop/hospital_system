CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -lrt -g -O2
LDFLAGS = -pthread -lrt
INCLUDES = -Iinclude

# Objetos (O professor guarda os .o na pasta src, ao lado dos .c)
OBJS = src/main.o src/triage.o src/surgery.o src/pharmacy.o \
       src/laboratory.o src/patient_thread.o src/ipc_utils.o \
       src/sync_utils.o src/log_manager.o src/stats_manager.o \
       src/config_parser.o src/time_simulation.o

# Target principal
TARGET = hospital_system

# Regras
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Debug build (Desliga otimizações para conseguires usar o debugger)
debug: CFLAGS += -DDEBUG -O0 -ggdb3
debug: clean $(TARGET)

# Cleanup
clean:
	rm -f src/*.o $(TARGET)
	rm -f logs/*.txt
	rm -f results/*/*.txt
	ipcrm -a 2>/dev/null || true

# ================= TESTES =================

# Nota: Estes testes assumem que tens os scripts na pasta 'tests'
test_basic: $(TARGET)
	./tests/test_basic.sh

test_concurrent: $(TARGET)
	./tests/test_concurrent.sh

test_stress: $(TARGET)
	./tests/test_stress.sh

# Corre todos os testes de seguida
test: test_basic test_concurrent test_stress

# ================= VERIFICAÇÕES (Valgrind) =================

# Verifica Memory Leaks (O que estavamos a fazer)
check_memory: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Verifica erros de concorrência (Race conditions)
check_threads: $(TARGET)
	valgrind --tool=helgrind ./$(TARGET)

# Verifica Deadlocks
check_deadlock: $(TARGET)
	valgrind --tool=drd ./$(TARGET)

# Limpeza completa de recursos IPC
ipc_clean:
	@echo "Removendo recursos IPC..."
	@ipcs -q | awk '$$3 ~ /^[0-9]/ {print $$2}' | xargs -r ipcrm -q
	@ipcs -m | awk '$$3 ~ /^[0-9]/ {print $$2}' | xargs -r ipcrm -m
	@ipcs -s | awk '$$3 ~ /^[0-9]/ {print $$2}' | xargs -r ipcrm -s
	@rm -f /dev/shm/sem.sem_*
	@rm -f input_pipe triage_pipe surgery_pipe pharmacy_pipe lab_pipe
	@echo "Recursos IPC removidos."

.PHONY: all clean debug test check_memory check_threads check_deadlock ipc_clean