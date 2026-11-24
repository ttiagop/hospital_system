#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include "../include/hospital.h"
#include "../include/ipc.h"

// Cores para os logs
#define GREEN   "\033[1;32m"
#define RESET   "\033[0m"

typedef struct {
    char command[512];
    system_config_t *config;
    ipc_resources_t *ipc;
} patient_args_t;

int get_int_param(char *src, char *label, int default_val) {
    char *ptr = strstr(src, label);
    if (ptr) return atoi(ptr + strlen(label));
    return default_val;
}

void* patient_lifecycle(void* arg) {
    patient_args_t *args = (patient_args_t*)arg;
    char command[512];
    snprintf(command, sizeof(command), "%s", args->command);
    
    char nome[50];
    sscanf(command, "%*s %s", nome);
    int init_time = get_int_param(command, "init:", 0);

    // 1. Espera pelo tempo de início (Simulação Temporal)
    // Isto mantém-se porque o enunciado pode pedir entradas diferidas
    while (1) {
        int current_time;
        // Verifica o tempo global na SHM
        pthread_mutex_lock(&args->ipc->shm_stats->mutex);
        current_time = args->ipc->shm_stats->simulation_time_units;
        pthread_mutex_unlock(&args->ipc->shm_stats->mutex);
        
        if (current_time >= init_time) break;
        
        // Dorme um pouco para não queimar CPU enquanto espera
        usleep(args->config->time_unit_ms * 1000); 
    }

    // 2. Envia o paciente para a Triagem
    int fd = open(PIPE_TRIAGE, O_WRONLY);
    if (fd != -1) {
        char buffer[520];
        // Garante que o comando tem newline para o parsing funcionar bem no triage.c
        snprintf(buffer, sizeof(buffer), "%s\n", command);
        if (write(fd, buffer, strlen(buffer)) == -1) {
            perror("[PACIENTE] Erro critico ao escrever no pipe");
        }
        close(fd);
        printf(GREEN "[PACIENTE] %s entrou na Triagem." RESET "\n", nome);
    } else {
        perror("[PACIENTE] Erro ao abrir pipe da triagem");
    }
    free(args);
    return NULL;
}

void start_patient_thread(char *command, system_config_t *config, ipc_resources_t *ipc) {
    pthread_t t;
    patient_args_t *args = malloc(sizeof(patient_args_t));
    
    if (args) {
        snprintf(args->command, sizeof(args->command), "%s", command);
        args->config = config;
        args->ipc = ipc;
        
        if (pthread_create(&t, NULL, patient_lifecycle, (void*)args) == 0) {
            pthread_detach(t); // Liberta recursos automaticamente quando acabar
        } else {
            free(args);
        }
    }
}