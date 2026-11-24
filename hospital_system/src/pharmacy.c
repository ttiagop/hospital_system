#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "../include/hospital.h"
#include "../include/ipc.h"

#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define YELLOW  "\033[1;33m"
#define RESET   "\033[0m"

static volatile int keep_running = 1;

// Variável de Condição para notificar que houve reposição
pthread_cond_t cond_stock_restocked = PTHREAD_COND_INITIALIZER;
// Mutex local para proteger o wait (embora usemos o da SHM para dados, o cond_wait precisa de um mutex bloqueado)
// Nota: Usamos o mutex do medicamento específico durante o wait.

void gerar_recibo(char *paciente, char *medicamento, int qtd) {
    struct stat st = {0};
    if (stat("results", &st) == -1) mkdir("results", 0777);
    
    char filename[100];
    time_t now = time(NULL);
    sprintf(filename, "results/pharmacy_delivery_%s_%s_%ld.txt", paciente, medicamento, now);
    
    FILE *f = fopen(filename, "w");
    if (f) {
        fprintf(f, "=== COMPROVATIVO DE ENTREGA FARMÁCIA ===\n");
        fprintf(f, "Paciente: %s\n", paciente);
        fprintf(f, "Medicamento: %s\n", medicamento);
        fprintf(f, "Quantidade: %d\n", qtd);
        fprintf(f, "Estado: ENTREGUE\n");
        fprintf(f, "=======================================\n");
        fclose(f);
    }
}

// Envia confirmação para a Cirurgia desbloquear
void notificar_cirurgia(ipc_resources_t *ipc, char *paciente) {
    hospital_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.msg_type = MSG_PHARMACY_READY; // Tipo 6
    strcpy(msg.source, "PHARMACY");
    strcpy(msg.target, "SURGERY");
    strcpy(msg.patient_id, paciente);
    strcpy(msg.data, "Medicamentos Reservados");

    // Envia para a fila de RESPOSTAS
    msgsnd(ipc->mq_resp, &msg, MSG_SIZE, 0);
    printf(BLUE "[PHARMACY] Confirmação enviada para %s.\n" RESET, paciente);
}

void* stock_monitor_worker(void* arg) {
    ipc_resources_t* ipc = (ipc_resources_t*)arg;
    printf(BLUE "[PHARMACY] Thread Monitorizacao Stock (Auto-Reposição) pronta." RESET "\n");

    while(keep_running) {
        for(int i=0; i<15; i++) {
            pthread_mutex_lock(&ipc->shm_stock->medications[i].mutex);
            
            if (strlen(ipc->shm_stock->medications[i].name) > 0) {
                int current = ipc->shm_stock->medications[i].current_stock;
                int threshold = ipc->shm_stock->medications[i].threshold;

                if (current < threshold) {
                    // Reposição Automática
                    int repor = threshold * 2; 
                    
                    // Simula tempo de reposição
                    // sleep(1); 
                    
                    ipc->shm_stock->medications[i].current_stock += repor;
                    
                    pthread_mutex_lock(&ipc->shm_stats->mutex);
                    ipc->shm_stats->auto_restocks++;
                    pthread_mutex_unlock(&ipc->shm_stats->mutex);

                    printf(YELLOW "[PHARMACY] REPOSIÇÃO AUTO: %s (+%d). Novo: %d" RESET "\n", 
                           ipc->shm_stock->medications[i].name, repor, ipc->shm_stock->medications[i].current_stock);
                    
                    char log_msg[256];
                    sprintf(log_msg, "[RESTOCK] %s repostos %d unidades.", 
                            ipc->shm_stock->medications[i].name, repor);
                    log_to_file(ipc, "PHARMACY", log_msg);

                    pthread_cond_broadcast(&cond_stock_restocked);
                }
            }
            pthread_mutex_unlock(&ipc->shm_stock->medications[i].mutex);
        }
        sleep(2); 
    }
    return NULL;
}

int find_med_index(ipc_resources_t* ipc, char* med_name) {
    for (int i = 0; i < 15; i++) {
        char *stored_name = ipc->shm_stock->medications[i].name;
        
        if (strlen(stored_name) > 0) {
            
            if (strcmp(stored_name, med_name) == 0) {
                return i;
            }
            int len_req = strlen(med_name);
            int len_stored = strlen(stored_name);

            if (len_req > 2 && len_stored > 2) {
                char req_id = med_name[len_req - 1];
                char stored_id = stored_name[len_stored - 1];

                if (req_id == stored_id && strncmp(stored_name, med_name, 4) == 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

void process_order(ipc_resources_t* ipc, char* command_str, char* patient_id) {
    pthread_mutex_lock(&ipc->shm_stats->mutex);
    ipc->shm_stats->total_pharmacy_requests++;
    pthread_mutex_unlock(&ipc->shm_stats->mutex);

    char med_name[50];
    char log_msg[256];
    int quantity = 0;

    char* start = strchr(command_str, '[');
    if (!start) return; 

    if (sscanf(start, "[%[^:]:%d]", med_name, &quantity) == 2) {
        int idx = find_med_index(ipc, med_name);
        
        if (idx == -1) {
            printf(RED "[PHARMACY] ERRO: Medicamento '%s' não existe!" RESET "\n", med_name);
            return;
        }

        // BLOQUEAR O MUTEX DO MEDICAMENTO
        pthread_mutex_lock(&ipc->shm_stock->medications[idx].mutex);

        // --- LOOP DE ESPERA (CONDITION VARIABLE) [cite: 461-490] ---
        // Enquanto não houver stock suficiente, espera. NÃO REJEITA.
        while (ipc->shm_stock->medications[idx].current_stock < quantity) {
            
            printf(RED "[PHARMACY] Stock insuficiente de %s (%d < %d). Em espera..." RESET "\n", 
                   med_name, ipc->shm_stock->medications[idx].current_stock, quantity);
            
            // Regista falha temporária nas stats
            pthread_mutex_lock(&ipc->shm_stats->mutex);
            ipc->shm_stats->stock_depletions++;
            pthread_mutex_unlock(&ipc->shm_stats->mutex);

            // Espera pelo sinal da thread de reposição
            // O pthread_cond_wait liberta o mutex automaticamente e dorme.
            // Quando acorda, volta a bloquear o mutex.
            pthread_cond_wait(&cond_stock_restocked, &ipc->shm_stock->medications[idx].mutex);
            
            printf(YELLOW "[PHARMACY] Acordado! A verificar stock de %s..." RESET "\n", med_name);
        }

        // Se saiu do while, é porque TEM stock e TEM o mutex
        ipc->shm_stock->medications[idx].current_stock -= quantity;
        
        printf(GREEN "[PHARMACY] APROVADO: %s. Stock: %d -> %d" RESET "\n", 
               med_name, ipc->shm_stock->medications[idx].current_stock + quantity, ipc->shm_stock->medications[idx].current_stock);
        
        gerar_recibo(patient_id, med_name, quantity);
        
        sprintf(log_msg, "[APPROVED] Dispensado %s (Qtd: %d) para %s.", med_name, quantity, patient_id);
        log_to_file(ipc, "PHARMACY", log_msg);

        pthread_mutex_unlock(&ipc->shm_stock->medications[idx].mutex);

        // AVISAR A CIRURGIA QUE ESTÁ PRONTO
        notificar_cirurgia(ipc, patient_id);
    
    } else {
        printf(YELLOW "[PHARMACY] Erro formato: %s" RESET "\n", command_str);
    }
}

void* pharmacy_urgent_worker(void* arg) {
    ipc_resources_t* ipc = (ipc_resources_t*)arg;
    hospital_message_t msg;
    printf(BLUE "[PHARMACY] Thread URGENTE pronta." RESET "\n");

    while(keep_running) {
        if (msgrcv(ipc->mq_urgent, &msg, MSG_SIZE, MSG_PHARMACY_REQUEST, IPC_NOWAIT) >= 0) {
            printf("[PHARMACY] URGENTE recebido: %s\n", msg.data);
            process_order(ipc, msg.data, msg.patient_id); 
        }
    }
    return NULL;
}

void* pharmacy_normal_worker(void* arg) {
    ipc_resources_t* ipc = (ipc_resources_t*)arg;
    hospital_message_t msg;
    printf(BLUE "[PHARMACY] Thread NORMAL pronta." RESET "\n");

    while(keep_running) {
        if (msgrcv(ipc->mq_normal, &msg, MSG_SIZE, MSG_PHARMACY_REQUEST, 0) >= 0) {
            printf("[PHARMACY] NORMAL recebido: %s\n", msg.data);
            process_order(ipc, msg.data, msg.patient_id); 
        }
    }
    return NULL;
}

void pharmacy_process(system_config_t *config, ipc_resources_t *ipc) {
    signal(SIGINT, SIG_IGN);
    (void)config;

    usleep(500000);
    
    log_to_file(ipc, "PHARMACY", "Farmacia Central iniciada com CVs");

    pthread_t t1, t2, t_monitor;
    pthread_create(&t1, NULL, pharmacy_urgent_worker, (void*)ipc);
    pthread_create(&t2, NULL, pharmacy_normal_worker, (void*)ipc);
    pthread_create(&t_monitor, NULL, stock_monitor_worker, (void*)ipc);

    while(keep_running) sleep(1);
}