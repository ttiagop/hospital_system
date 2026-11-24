#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include "../include/hospital.h"
#include "../include/ipc.h"
#include "../include/stats.h"

// Cores (para o print na consola)
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

void print_stats_console(ipc_resources_t *ipc) {
    pthread_mutex_lock(&ipc->shm_stats->mutex);
    printf("\n" BLUE "=== ESTATÍSTICAS ===" RESET "\n");
    printf("Tempo: " YELLOW "%d" RESET "\n", ipc->shm_stats->simulation_time_units);
    printf("Urgentes: " RED "%d" RESET "\n", ipc->shm_stats->total_emergency_patients);
    printf("Consultas: " GREEN "%d" RESET "\n", ipc->shm_stats->total_appointments);
    printf("Cirurgias: " YELLOW "%d" RESET "\n", ipc->shm_stats->total_operations);
    printf("Farmacia: " BLUE "%d" RESET " (Roturas: %d)\n", ipc->shm_stats->total_pharmacy_requests, ipc->shm_stats->stock_depletions);
    printf("PREOPs: " CYAN "%d" RESET "\n", ipc->shm_stats->total_preop_tests);
    printf("Transferidos: " RED "%d" RESET "\n", ipc->shm_stats->critical_transfers);
    printf(BLUE "====================" RESET "\n\n");
    pthread_mutex_unlock(&ipc->shm_stats->mutex);
}

void save_stats_snapshot(ipc_resources_t *ipc) {
    struct stat st = {0};
    if (stat("results", &st) == -1) mkdir("results", 0777);

    char filename[100];
    sprintf(filename, "results/stats_snapshot_%ld.txt", time(NULL));
    
    FILE *f = fopen(filename, "w");
    if (!f) return;

    pthread_mutex_lock(&ipc->shm_stats->mutex);
    fprintf(f, "=== SNAPSHOT DO SISTEMA ===\n");
    fprintf(f, "Tempo Simulação: %d ticks\n", ipc->shm_stats->simulation_time_units);
    fprintf(f, "Pacientes Urgentes: %d\n", ipc->shm_stats->total_emergency_patients);
    fprintf(f, "Consultas Normais: %d\n", ipc->shm_stats->total_appointments);
    fprintf(f, "Cirurgias Realizadas: %d\n", ipc->shm_stats->total_operations);
    fprintf(f, "Pedidos Farmacia: %d\n", ipc->shm_stats->total_pharmacy_requests);
    fprintf(f, "Roturas Stock: %d\n", ipc->shm_stats->stock_depletions);
    fprintf(f, "Reposicoes Auto: %d\n", ipc->shm_stats->auto_restocks);
    fprintf(f, "Analises PREOP: %d\n", ipc->shm_stats->total_preop_tests);
    fprintf(f, "Transferencias Criticas: %d\n", ipc->shm_stats->critical_transfers);
    pthread_mutex_unlock(&ipc->shm_stats->mutex);
    
    fclose(f);
    printf(BLUE "\n[STATS] Snapshot guardado em %s\n" RESET, filename);
}