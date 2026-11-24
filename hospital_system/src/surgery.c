#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h> // Necessário para difftime e time
#include "../include/hospital.h"
#include "../include/ipc.h"

#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define YELLOW  "\033[1;33m"
#define RESET   "\033[0m"

// Semáforos globais
sem_t *sem_teams;
sem_t *sem_bo1;
sem_t *sem_bo2;
sem_t *sem_bo3;

// Estrutura para passar argumentos à thread
typedef struct {
    ipc_resources_t *ipc;
    hospital_message_t msg; 
} surgery_args_t;

// --- Funções Auxiliares ---

void solicitar_medicamento(ipc_resources_t *ipc, char *paciente, char *meds_list) {
    if (strlen(meds_list) < 2) return; 

    hospital_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.msg_type = MSG_PHARMACY_REQUEST;
    msg.msg_priority = 5; 
    strcpy(msg.source, "SURGERY");
    strcpy(msg.patient_id, paciente);
    strcpy(msg.requested_meds, meds_list); 
    sprintf(msg.data, "REQ_SURGERY items: %s", meds_list);
    
    msgsnd(ipc->mq_normal, &msg, MSG_SIZE, 0);
    printf(YELLOW "[SURGERY] Pediu medicamentos para %s" RESET "\n", paciente);
}

void solicitar_analise(ipc_resources_t *ipc, char *paciente, char *tests_list) {
    if (strlen(tests_list) < 2) return;

    hospital_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.msg_type = MSG_LAB_REQUEST; 
    msg.msg_priority = 5;
    strcpy(msg.source, "SURGERY");
    strcpy(msg.patient_id, paciente);
    strcpy(msg.requested_tests, tests_list); 
    sprintf(msg.data, "REQ_SURGERY %s", tests_list);
    
    msgsnd(ipc->mq_normal, &msg, MSG_SIZE, 0);
    printf(YELLOW "[SURGERY] Pediu analises para %s" RESET "\n", paciente);
}

// --- WORKER: A Thread que faz a Cirurgia ---
void* surgery_worker(void* args) {
    surgery_args_t *data = (surgery_args_t*)args;
    ipc_resources_t *ipc = data->ipc;
    hospital_message_t msg = data->msg;
    char log_msg[256];

    hospital_message_t resp; 
    memset(&resp, 0, sizeof(resp));

    // 1. Solicitar Recursos
    int needs_meds = (strlen(msg.requested_meds) > 2);
    int needs_labs = (strlen(msg.requested_tests) > 2);

    if (needs_meds) solicitar_medicamento(ipc, msg.patient_id, msg.requested_meds);
    if (needs_labs) solicitar_analise(ipc, msg.patient_id, msg.requested_tests);

    // 2. Esperar Respostas 
    if (needs_labs) {
        msgrcv(ipc->mq_resp, &resp, MSG_SIZE, MSG_LAB_RESULTS_READY, 0);
    }

    if (needs_meds) {
        msgrcv(ipc->mq_resp, &resp, MSG_SIZE, MSG_PHARMACY_READY, 0);
    }

    // 3. Escolha da Sala
    sem_t *sala_alvo = sem_bo1; 
    char *nome_sala = "BO1-Cardio";
    int duracao = 5; 

    if (strstr(msg.specialty, "ORTHO")) {
        sala_alvo = sem_bo2; nome_sala = "BO2-Orto"; duracao = 3;
    } else if (strstr(msg.specialty, "NEURO")) {
        sala_alvo = sem_bo3; nome_sala = "BO3-Neuro"; duracao = 8;
    }

    // --- CÁLCULO DA SAÚDE ---
    time_t now = time(NULL);
    double tempo_espera = difftime(now, msg.timestamp); 
    int estabilidade_atual = msg.initial_stability - (int)tempo_espera;
    if (estabilidade_atual < 0) estabilidade_atual = 0;
    // ------------------------

    // 4. Ocupar Sala e Equipa
    printf(YELLOW "[THREAD %s] A aguardar vaga em %s..." RESET "\n", msg.patient_id, nome_sala);
    sem_wait(sala_alvo);
    
    printf(YELLOW "[THREAD %s] Sala OK. Aguarda equipa..." RESET "\n", msg.patient_id);
    sem_wait(sem_teams);
    
    // 5. Operar
    printf(RED "[THREAD %s] A OPERAR em %s (Tempo: %ds)..." RESET "\n", msg.patient_id, nome_sala, duracao);
    
    sprintf(log_msg, "[START] Cirurgia %s iniciada: %s (Stab: %d)", nome_sala, msg.patient_id, estabilidade_atual);
    log_to_file(ipc, "SURGERY", log_msg);
    
    sleep(duracao); 

    // ==========================================================
    // NOVA PARTE: ATUALIZAR ESTATÍSTICAS
    // ==========================================================
    pthread_mutex_lock(&ipc->shm_stats->mutex);
    
    ipc->shm_stats->total_operations++;
    ipc->shm_stats->completed_surgeries++;
    
    // Contagem específica por bloco
    if (sala_alvo == sem_bo1) ipc->shm_stats->total_surgeries_bo1++;
    else if (sala_alvo == sem_bo2) ipc->shm_stats->total_surgeries_bo2++;
    else if (sala_alvo == sem_bo3) ipc->shm_stats->total_surgeries_bo3++;

    pthread_mutex_unlock(&ipc->shm_stats->mutex);
    // ==========================================================

    // 6. Libertar
    sem_post(sem_teams);
    printf(GREEN "[THREAD %s] Equipa libertada." RESET "\n", msg.patient_id);
    sleep(1); 
    sem_post(sala_alvo);

    printf(GREEN "[THREAD %s] Alta e thread terminada." RESET "\n", msg.patient_id);

    free(data);
    return NULL;
}

// --- Processo Principal (Despachante) ---
void surgery_process(system_config_t *config, ipc_resources_t *ipc) {
    signal(SIGINT, SIG_IGN);
    (void)config;

    log_to_file(ipc, "SURGERY", "Blocos Operatorios iniciados");
    printf(BLUE "[SURGERY] Gestor iniciado. A ligar semáforos..." RESET "\n");

    sem_teams = sem_open(SEM_TEAMS, 0);
    sem_bo1 = sem_open(SEM_BO1, 0); 
    sem_bo2 = sem_open(SEM_BO2, 0); 
    sem_bo3 = sem_open(SEM_BO3, 0);

    if (sem_teams == SEM_FAILED || sem_bo1 == SEM_FAILED) {
        perror(RED "[SURGERY] Erro semáforos" RESET);
        exit(1);
    }

    hospital_message_t msg_buffer;

    while(1) {
        int patient_found = 0;
        
        // Prioridade às filas (Urgente > Cirurgia > Normal)
        if (msgrcv(ipc->mq_urgent, &msg_buffer, MSG_SIZE, MSG_NEW_EMERGENCY, IPC_NOWAIT) >= 0) {
            patient_found = 1;
        } 
        else if (msgrcv(ipc->mq_urgent, &msg_buffer, MSG_SIZE, MSG_NEW_SURGERY, IPC_NOWAIT) >= 0) {
            patient_found = 1;
        }
        else if (msgrcv(ipc->mq_normal, &msg_buffer, MSG_SIZE, MSG_NEW_APPOINTMENT, IPC_NOWAIT) >= 0) {
            patient_found = 1;
        }

        if (patient_found) {
            printf(BLUE "[SURGERY] Paciente %s recebido. A criar thread..." RESET "\n", msg_buffer.patient_id);

            // 1. Alocar memória para passar os dados à thread
            surgery_args_t *args = malloc(sizeof(surgery_args_t));
            args->ipc = ipc;
            args->msg = msg_buffer; // Copia a mensagem inteira

            // 2. Criar a thread
            pthread_t tid;
            if (pthread_create(&tid, NULL, surgery_worker, (void*)args) != 0) {
                perror("Erro ao criar thread");
                free(args);
            } else {
                pthread_detach(tid);
            }
        } else {
            usleep(100000); // 100ms sleep para não queimar CPU
        }
    }
}