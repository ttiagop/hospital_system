#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <sys/msg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "../include/hospital.h"
#include "../include/ipc.h"
#include "../include/stats.h" 

// Cores
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

ipc_resources_t ipc;
pid_t pid_triage, pid_surgery, pid_pharmacy, pid_lab, pid_time;
pthread_t input_thread;
int fd_triage_pipe = -1;
static volatile int system_running = 1;

// Protótipos
void time_process(system_config_t *config, ipc_resources_t *ipc);
void start_patient_thread(char *command, system_config_t *config, ipc_resources_t *ipc);
void triage_process(system_config_t *config, ipc_resources_t *ipc);
void surgery_process(system_config_t *config, ipc_resources_t *ipc);
void pharmacy_process(system_config_t *config, ipc_resources_t *ipc);
void laboratory_process(system_config_t *config, ipc_resources_t *ipc);

void sigchld_handler(int signum) {
    (void)signum;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (system_running) {
            char msg[100];
            if (WIFEXITED(status)) {
                sprintf(msg, "Processo filho %d terminou (Exit Code: %d)", pid, WEXITSTATUS(status));
                fprintf(stderr, RED "[SISTEMA] %s\n" RESET, msg);
            } else if (WIFSIGNALED(status)) {
                sprintf(msg, "Processo filho %d morto por sinal %d", pid, WTERMSIG(status));
                fprintf(stderr, RED "[SISTEMA] %s\n" RESET, msg);
            }
        }
    }
}

// Handlers Sinais - Agora chamam o stats_manager
void snapshot_handler(int signum) {
    (void)signum;
    save_stats_snapshot(&ipc); // Função movida para stats_manager.c
}

void print_stats_handler(int signum) {
    (void)signum;
    print_stats_console(&ipc); // Função movida para stats_manager.c
}

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
        } else { strcpy(dest, ""); }
    } else { strcpy(dest, ""); }
}

static void extract_value(char *src, char *label, char *dest) {
    char *ptr = strstr(src, label);
    if (ptr) sscanf(ptr + strlen(label), "%s", dest);
}

void send_to_pharmacy(char* raw_command) {
    hospital_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_type = MSG_PHARMACY_REQUEST; 
    msg.msg_priority = 3; 
    strcpy(msg.source, "MAIN");
    strcpy(msg.target, "PHARMACY");
    strncpy(msg.data, raw_command, 511);
    if (msgsnd(ipc.mq_normal, &msg, MSG_SIZE, 0) == -1) perror(RED "[MAIN] Erro envio Farmacia" RESET);
    else printf(GREEN "[MAIN] Enviado para Farmacia." RESET "\n");
}

void* command_monitor_worker(void* arg) {
    system_config_t *config = (system_config_t*)arg;
    printf(BLUE "[MAIN] Monitor de comandos ativo." RESET "\n");
    int fd = open(PIPE_INPUT, O_RDWR);
    if (fd == -1) return NULL;

    char buffer[512];
    while (system_running) {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            if (buffer[bytes_read-1] == '\n') buffer[bytes_read-1] = '\0';
            printf("[MAIN] Cmd: '%s'\n", buffer);

            if (strncmp(buffer, "EMERGENCY", 9) == 0) start_patient_thread(buffer, config, &ipc);
            else if (strncmp(buffer, "SURGERY", 7) == 0) {
                hospital_message_t msg;
                memset(&msg, 0, sizeof(msg));
                char nome[50], tipo[50], testes[100], meds[100];
                sscanf(buffer, "%*s %s", nome);
                extract_value(buffer, "type:", tipo);
                extract_list(buffer, "tests:", testes);
                extract_list(buffer, "meds:", meds);
                
                msg.msg_type = MSG_NEW_SURGERY;
                msg.msg_priority = 1;
                strcpy(msg.source, "MAIN");
                strcpy(msg.target, "SURGERY");
                strcpy(msg.patient_id, nome);
                strcpy(msg.specialty, tipo);
                strcpy(msg.requested_tests, testes);
                strcpy(msg.requested_meds, meds);
                sprintf(msg.data, "Cirurgia %s", nome);
                
                msgsnd(ipc.mq_urgent, &msg, MSG_SIZE, 0);
                printf(GREEN "[MAIN] Cirurgia enviada." RESET "\n");
            }
            else if (strncmp(buffer, "PHARM", 5) == 0) send_to_pharmacy(buffer);
            else if (strncmp(buffer, "LAB", 3) == 0) {
                hospital_message_t msg;
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_LAB_REQUEST; 
                strcpy(msg.source, "MAIN");
                snprintf(msg.data, sizeof(msg.data), "%s", buffer);
                msgsnd(ipc.mq_normal, &msg, MSG_SIZE, 0);
                printf(GREEN "[MAIN] Enviado para Lab." RESET "\n");
            }
            else if (strncmp(buffer, "STATUS", 6) == 0) {
                printf(CYAN "[MAIN] STATUS recebido.\n" RESET);
                print_stats_handler(0);
            }
            else if (strncmp(buffer, "RESTOCK", 7) == 0) {
                printf(YELLOW "[MAIN] RESTOCK recebido (registo manual).\n" RESET);
            }
            else if (strncmp(buffer, "TRIAGE", 6) == 0 || strncmp(buffer, "APPOINTMENT", 11) == 0) {
                if (fd_triage_pipe != -1) {
                    int len = strlen(buffer);
                    if (buffer[len-1] != '\n') { strcat(buffer, "\n"); len++; }
                    if (write(fd_triage_pipe, buffer, len) == -1) {
                        perror("[MAIN] Erro crítico ao escrever no pipe da triagem");
                    }
                    printf(GREEN "[MAIN] Encaminhado para Triagem." RESET "\n");
                }
            }
        } 
        else if (bytes_read == 0) sleep(1);
    }
    close(fd);
    return NULL;
}

void sigint_handler(int signum) {
    (void)signum;
    printf("\n" RED "[SISTEMA] A encerrar..." RESET "\n");
    system_running = 0;
    if (fd_triage_pipe != -1) close(fd_triage_pipe);
    if (pid_triage > 0) kill(pid_triage, SIGTERM);
    if (pid_surgery > 0) kill(pid_surgery, SIGTERM);
    if (pid_pharmacy > 0) kill(pid_pharmacy, SIGTERM);
    if (pid_lab > 0) kill(pid_lab, SIGTERM);
    if (pid_time > 0) kill(pid_time, SIGTERM);
    for(int i=0; i<5; i++) wait(NULL);
    cleanup_ipc_resources(&ipc);
    exit(0);
}

int main() {
    system_config_t config;
    signal(SIGINT, sigint_handler);
    signal(SIGUSR1, print_stats_handler);
    signal(SIGUSR2, snapshot_handler);
    signal(SIGCHLD, sigchld_handler);

    printf(BLUE "=== SISTEMA HOSPITALAR ===" RESET "\n");
    printf(BLUE "[SISTEMA] PID: %d" RESET "\n", getpid());

    if (load_config("config/config.txt", &config) == -1) return 1;
    if (create_ipc_resources(&ipc, &config) == -1) return 1;

    pid_triage = fork(); if (pid_triage == 0) { triage_process(&config, &ipc); exit(0); }
    pid_surgery = fork(); if (pid_surgery == 0) { surgery_process(&config, &ipc); exit(0); }
    pid_pharmacy = fork(); if (pid_pharmacy == 0) { pharmacy_process(&config, &ipc); exit(0); }
    pid_lab = fork(); if (pid_lab == 0) { laboratory_process(&config, &ipc); exit(0); }
    pid_time = fork(); if (pid_time == 0) { time_process(&config, &ipc); exit(0); }

    if ((fd_triage_pipe = open(PIPE_TRIAGE, O_WRONLY)) == -1) perror(RED "Erro pipe triagem" RESET);
    
    if (pthread_create(&input_thread, NULL, command_monitor_worker, (void*)&config) != 0) kill(getpid(), SIGINT);

    while(system_running) sleep(1);
    return 0;
}