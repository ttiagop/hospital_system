#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include "../include/hospital.h"
#include "../include/ipc.h"

#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define YELLOW  "\033[1;33m"
#define RESET   "\033[0m"

// Extrai listas [ITEM1,ITEM2] (STATIC ADICIONADO)
static void extract_list(char *src, char *label, char *dest) {
    char *ptr = strstr(src, label);
    if (ptr) {
        char *start = strchr(ptr, '[');
        char *end = strchr(ptr, ']');
        if (start && end && end > start) {
            int len = end - (start + 1);
            if (len >= 99) len = 99; 
            strncpy(dest, start + 1, len);
            dest[len] = '\0';
        } else {
            strcpy(dest, ""); 
        }
    } else {
        strcpy(dest, "");
    }
}

// Extrai inteiros (STATIC ADICIONADO)
static int extract_int(char *src, char *label, int default_val) {
    char *ptr = strstr(src, label);
    if (ptr) {
        return atoi(ptr + strlen(label));
    }
    return default_val;
}

void triage_process(system_config_t *config, ipc_resources_t *ipc) {
    signal(SIGINT, SIG_IGN);
    // (void)config; // Se não usares config, descomenta esta linha para calar warning
    char buffer[512];
    int fd;
    
    printf(BLUE "[TRIAGE] PID %d pronto. A ler formato avançado..." RESET "\n", getpid());

    fd = open(PIPE_TRIAGE, O_RDONLY);
    if (fd == -1) {
        perror(RED "[TRIAGE] Erro fatal pipe" RESET);
        exit(1);
    }

    while(1) {
        ssize_t bytes = read(fd, buffer, sizeof(buffer)-1);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            if (buffer[bytes-1] == '\n') buffer[bytes-1] = '\0';

            printf("[TRIAGE] Recebido: %s\n", buffer);

            char nome[50] = {0};
            int gravidade = 4; // Default para NORMAL se não especificado
            int estabilidade = 100;
            char testes_str[100] = {0};
            char meds_str[100] = {0};

            // 1. Extração Inteligente
            sscanf(buffer, "%*s %s", nome);
            estabilidade = extract_int(buffer, "stability:", 100);
            extract_list(buffer, "tests:", testes_str);
            extract_list(buffer, "meds:", meds_str);

            // Lógica de Prioridade:
            if (strstr(buffer, "triage:")) {
                gravidade = extract_int(buffer, "triage:", 4);
            }
            else if (strstr(buffer, "urgency: HIGH")) {
                gravidade = 2; // Urgente
            }
            else if (strstr(buffer, "urgency: LOW")) {
                gravidade = 4; // Normal
            }
            else if (strncmp(buffer, "EMERGENCY", 9) == 0) {
                gravidade = 1;
            }

            if (strlen(nome) > 0) {
                // Verificar Capacidade
                pthread_mutex_lock(&ipc->shm_stats->mutex);
                int total_now = ipc->shm_stats->total_emergency_patients - ipc->shm_stats->completed_emergencies; 
                if (total_now >= config->max_emergency_patients) {
                      pthread_mutex_unlock(&ipc->shm_stats->mutex);
                      printf(RED "[TRIAGE] REJEITADO: %s (Hospital Cheio)" RESET "\n", nome);
                      continue; 
                }
                pthread_mutex_unlock(&ipc->shm_stats->mutex);

                // Preencher Mensagem
                hospital_message_t msg;
                memset(&msg, 0, sizeof(msg));

                msg.timestamp = time(NULL);
                
                strcpy(msg.source, "TRIAGE");
                strcpy(msg.patient_id, nome);
                msg.msg_priority = gravidade;
                msg.initial_stability = estabilidade;
                
                strcpy(msg.requested_tests, testes_str);
                strcpy(msg.requested_meds, meds_str);

                if (strstr(buffer, "type:NEURO")) {
                    strcpy(msg.specialty, "NEURO");
                }
                else if (strstr(buffer, "type:GERAL") || strstr(testes_str, "HEMO") || strstr(testes_str, "GLIC")) {
                    strcpy(msg.specialty, "GERAL");
                }
                else {
                    strcpy(msg.specialty, "ORTHO");
                }
                       
                sprintf(msg.data, "Pac: %s | Stab: %d", nome, estabilidade);

                // Envio para Filas
                if (gravidade <= 2) { 
                    msg.msg_type = MSG_NEW_EMERGENCY;
                    msgsnd(ipc->mq_urgent, &msg, MSG_SIZE, 0); 
                    
                    pthread_mutex_lock(&ipc->shm_stats->mutex);
                    ipc->shm_stats->total_emergency_patients++;
                    pthread_mutex_unlock(&ipc->shm_stats->mutex);

                    printf(RED "[TRIAGE] %s -> URGENTE (Grav:%d)" RESET "\n", nome, gravidade);
                } else {
                    msg.msg_type = MSG_NEW_APPOINTMENT;
                    msgsnd(ipc->mq_normal, &msg, MSG_SIZE, 0);                    
                    
                    pthread_mutex_lock(&ipc->shm_stats->mutex);
                    ipc->shm_stats->total_appointments++;
                    pthread_mutex_unlock(&ipc->shm_stats->mutex);

                    printf(GREEN "[TRIAGE] %s -> NORMAL (Grav:%d)" RESET "\n", nome, gravidade);
                }
                
                char log_entry[256];
                sprintf(log_entry, "[ADMIT] %s Admitido. Grav: %d", nome, gravidade);
                log_to_file(ipc, "TRIAGE", log_entry);

            } else {
                printf(YELLOW "[TRIAGE] Erro: Nome não encontrado." RESET "\n");
            }
        } 
        else if (bytes == 0) break;
    }
    close(fd);
    printf(BLUE "[TRIAGE] A encerrar." RESET "\n");
}