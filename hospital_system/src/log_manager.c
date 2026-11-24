#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "../include/hospital.h"
#include "../include/ipc.h"

// Implementação compatível com ipc.h (com const char*)
void log_to_file(ipc_resources_t *ipc, const char* module, const char* message) {
    // Silenciar aviso de variável não usada (se não usarmos mutex partilhado)
    (void)ipc; 

    // Abre o ficheiro em modo "append" (acrescentar no fim)
    FILE *f = fopen("hospital_log.txt", "a");
    if (f == NULL) {
        perror("Erro ao abrir ficheiro de log");
        return;
    }

    // Obter hora atual
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    // Remove o \n que o ctime coloca no fim
    if (time_str) {
        time_str[strlen(time_str) - 1] = '\0';
    }

    // Escreve: [DATA] [MODULO] MENSAGEM
    fprintf(f, "[%s] [%s] %s\n", time_str ? time_str : "UNKNOWN", module, message);

    fclose(f);
}