#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h> 
#include <time.h>
#include "../include/hospital.h"
#include "../include/ipc.h"

// Cores
#define BLUE    "\033[1;34m"
#define RESET   "\033[0m"
#define RED     "\033[1;31m"

// Função para gerar o ficheiro de resultados
void generate_lab_report(char *patient_id, char *test_type) {
    struct stat st = {0};
    if (stat("results", &st) == -1) mkdir("results", 0777);

    time_t now = time(NULL);
    char filename[256];
    sprintf(filename, "results/lab_results_%s_%ld.txt", patient_id, now);

    FILE *f = fopen(filename, "w");
    if (f) {
        fprintf(f, "RELATORIO DE ANALISES LABORATORIAIS\n");
        fprintf(f, "===================================\n");
        fprintf(f, "Paciente: %s\n", patient_id);
        fprintf(f, "Data/Hora: %s", ctime(&now));
        fprintf(f, "Tipo de Exame: %s\n", test_type);
        fprintf(f, "Estado: CONCLUIDO\n");
        fprintf(f, "Responsavel: SISTEMA_AUTO_LAB\n");
        fprintf(f, "===================================\n");
        fclose(f);
    }
}

void laboratory_process(system_config_t *config, ipc_resources_t *ipc) {
    (void)config; 
    signal(SIGINT, SIG_IGN); 

    printf(BLUE "[LAB] Iniciado.\n" RESET);

    sem_t *sem_mach1 = sem_open(SEM_LAB1, 0);
    sem_t *sem_mach2 = sem_open(SEM_LAB2, 0);

    if (sem_mach1 == SEM_FAILED || sem_mach2 == SEM_FAILED) {
        perror(RED "[LAB] Erro fatal: Semáforos não encontrados" RESET);
        return; 
    }

    printf(BLUE "[LAB] Worker ativo. Aguardando pedidos...\n" RESET);

    hospital_message_t req, resp;

    while (1) {
        if (msgrcv(ipc->mq_normal, &req, MSG_SIZE, MSG_LAB_REQUEST, 0) != -1) {
            
            printf("[LAB] Pedido recebido para %s: %s\n", req.patient_id, req.data);

            sem_t *use_machine = sem_mach1;
            int machine_id = 1;

            // Tenta usar a Máquina 1, se ocupada, espera pela 2
            if (sem_trywait(sem_mach1) != 0) {
                sem_wait(sem_mach2); 
                use_machine = sem_mach2;
                machine_id = 2;
            }

            printf("[LAB%d] A analisar sangue de %s...\n", machine_id, req.patient_id);
            
            usleep(200000); // Simula o tempo de análise

            sem_post(use_machine);

            // ==========================================================
            // NOVA PARTE: ATUALIZAR ESTATÍSTICAS
            // ==========================================================
            pthread_mutex_lock(&ipc->shm_stats->mutex);
            
            // 1. Contar uso da máquina específica
            if (machine_id == 1) {
                ipc->shm_stats->total_lab_tests_lab1++;
            } else {
                ipc->shm_stats->total_lab_tests_lab2++;
            }

            // 2. Verificar se é PREOP (Pré-operatório)
            // Procura a palavra "PREOP" na string de dados
            if (strstr(req.data, "PREOP") != NULL) {
                ipc->shm_stats->total_preop_tests++;
            }

            pthread_mutex_unlock(&ipc->shm_stats->mutex);
            // ==========================================================

            generate_lab_report(req.patient_id, req.data);

            memset(&resp, 0, sizeof(resp));
            resp.msg_type = MSG_LAB_RESULTS_READY; 
            strcpy(resp.source, "LAB");
            strcpy(resp.target, req.source); 
            strcpy(resp.patient_id, req.patient_id);
            
            snprintf(resp.data, sizeof(resp.data), "Resultados OK: %.230s", req.data);

            msgsnd(ipc->mq_resp, &resp, MSG_SIZE, 0);
            
            printf(BLUE "[LAB] Resposta enviada para %s via MQ_RESP\n" RESET, req.patient_id);
        }
    }
}