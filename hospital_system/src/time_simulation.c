#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include "../include/hospital.h"
#include "../include/ipc.h"

#define BLUE    "\033[1;34m"
#define RESET   "\033[0m"

static volatile int simulation_running = 1;

void time_process(system_config_t *config, ipc_resources_t *ipc) {
    // Ignora o SIGINT (Ctrl+C) para ser encerrado ordenadamente pelo main
    signal(SIGINT, SIG_IGN); 
    
    printf(BLUE "[TIME] Simulacao Temporal Iniciada. (1 unidade = %d ms)" RESET "\n", config->time_unit_ms);

    // Inicializa o relógio na memória partilhada
    pthread_mutex_lock(&ipc->shm_stats->mutex);
    ipc->shm_stats->system_start_time = time(NULL);
    ipc->shm_stats->simulation_time_units = 0;
    pthread_mutex_unlock(&ipc->shm_stats->mutex);

    while(simulation_running) {
        // 1. Aguarda a duração de 1 unidade de tempo
        usleep(config->time_unit_ms * 1000);

        // 2. Incrementa o tempo global
        pthread_mutex_lock(&ipc->shm_stats->mutex);
        ipc->shm_stats->simulation_time_units++;
        
        // PRINTS COMENTADOS (Para não poluir o ecrã)
        /*
        if (ipc->shm_stats->simulation_time_units % 10 == 0) {
            printf("[TIME] Tick: %d\n", ipc->shm_stats->simulation_time_units);
        }
        */
        
        pthread_mutex_unlock(&ipc->shm_stats->mutex);
    }
}