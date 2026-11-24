#ifndef IPC_H
#define IPC_H

#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>   
#include <sys/shm.h>   
#include <sys/sem.h>   
#include <sys/msg.h>   
#include <semaphore.h> 
#include "hospital.h"  

// ==========================================================
// 1. Definições de Chaves e Constantes
// ==========================================================

#define MSG_SIZE (sizeof(hospital_message_t) - sizeof(long))

#define MQ_KEY_URGENT   'U'
#define MQ_KEY_NORMAL   'N'
#define MQ_KEY_RESP     'R'

#define SHM_KEY_STATS   '1'
#define SHM_KEY_BO      '2'
#define SHM_KEY_STOCK   '3'
#define SHM_KEY_LABS    '4'
#define SHM_KEY_LOG     '5'

// Nomes dos Semáforos (POSIX)
#define SEM_BO1         "/sem_surgery_bo1"
#define SEM_BO2         "/sem_surgery_bo2"
#define SEM_BO3         "/sem_surgery_bo3"
#define SEM_TEAMS       "/sem_medical_teams"
#define SEM_LAB1        "/sem_lab1_equipment"
#define SEM_LAB2        "/sem_lab2_equipment"
#define SEM_PHARM       "/sem_pharmacy_access"

// Nomes dos Pipes
#define PIPE_INPUT      "/tmp/input_pipe"
#define PIPE_TRIAGE     "/tmp/triage_pipe"
#define PIPE_SURGERY    "/tmp/surgery_pipe"
#define PIPE_PHARMACY   "/tmp/pharmacy_pipe"
#define PIPE_LAB        "/tmp/lab_pipe"

// Tipos de Mensagem
#define MSG_NEW_EMERGENCY      1
#define MSG_NEW_APPOINTMENT    2
#define MSG_NEW_SURGERY        3
#define MSG_PHARMACY_REQUEST   4
#define MSG_LAB_REQUEST        5
#define MSG_PHARMACY_READY     6
#define MSG_LAB_RESULTS_READY  7
#define MSG_CRITICAL_STATUS    8
#define MSG_TRANSFER_PATIENT   9
#define MSG_REJECT_PATIENT     10

// ==========================================================
// 2. Estruturas de Dados
// ==========================================================

// Estrutura da Mensagem
typedef struct {
    long msg_type;          
    int msg_priority;       
    char source[20];        
    char target[20];        
    char patient_id[15];
    
    char specialty[20];

    int operation_id;
    time_t timestamp;
    char data[512];  
    
    int initial_stability;
    char requested_tests[100];
    char requested_meds[100];
    
} hospital_message_t;

#define MSG_SIZE (sizeof(hospital_message_t) - sizeof(long))

// SHM1: Estatísticas Globais
typedef struct {
    pthread_mutex_t mutex;
    pthread_mutexattr_t mutex_attr;
    // Triagem
    int total_emergency_patients;
    int total_appointments;
    double total_emergency_wait_time;
    double total_appointment_wait_time;
    int completed_emergencies;
    int completed_appointments;
    int critical_transfers;
    int rejected_patients;
    // Blocos Operatórios
    int total_surgeries_bo1;
    int total_surgeries_bo2;
    int total_surgeries_bo3;
    double total_surgery_wait_time;
    int completed_surgeries;
    int cancelled_surgeries;
    double bo1_utilization_time;
    double bo2_utilization_time;
    double bo3_utilization_time;
    // Farmácia
    int total_pharmacy_requests;
    int urgent_requests;
    int normal_requests;
    double total_pharmacy_response_time;
    int stock_depletions;
    int auto_restocks;
    // Laboratórios
    int total_lab_tests_lab1;
    int total_lab_tests_lab2;
    int total_preop_tests;
    double total_lab_turnaround_time;
    int urgent_lab_tests;
    // Globais
    int total_operations;
    int system_errors;
    time_t system_start_time;
    int simulation_time_units;
} global_statistics_t;

// SHM2: Estado Blocos Operatórios
typedef struct {
    int room_id;
    int status;                
    char current_patient[15];
    int surgery_start_time;
    int estimated_end_time;
    pthread_mutex_t mutex;
} surgery_room_t;

typedef struct {
    surgery_room_t rooms[3];
    int medical_teams_available; 
    pthread_mutex_t teams_mutex;
} surgery_block_shm_t;

// SHM3: Stock Farmácia
typedef struct {
    char name[30];
    int current_stock;
    int reserved;
    int threshold;
    int max_capacity;
    pthread_mutex_t mutex;
} medication_stock_t;

typedef struct {
    medication_stock_t medications[15];
    int total_active_requests;
    pthread_mutex_t global_mutex;
} pharmacy_shm_t;

// SHM4: Filas Laboratórios
typedef struct {
    char request_id[20];
    char patient_id[15];
    int test_type;
    int priority;
    int status;           
    time_t request_time;
    time_t completion_time;
} lab_request_entry_t;

typedef struct {
    lab_request_entry_t queue_lab1[50]; 
    lab_request_entry_t queue_lab2[50]; 
    int lab1_count;
    int lab2_count;
    int lab1_available_slots;
    int lab2_available_slots;
    pthread_mutex_t lab1_mutex;
    pthread_mutex_t lab2_mutex;
} lab_queue_shm_t;

// SHM5: Log Eventos Críticos
typedef struct {
    time_t timestamp;
    char event_type[30];
    char component[20];
    char description[256];
    int severity;         
} critical_event_t;

typedef struct {
    critical_event_t events[1000]; 
    int event_count;
    int current_index;
    pthread_mutex_t mutex;
} critical_log_shm_t;

// ==========================================================
// 3. Estrutura Agregadora
// ==========================================================

typedef struct {
    int mq_urgent;
    int mq_normal;
    int mq_resp;
    
    int shm_stats_id;
    int shm_bo_id;
    int shm_stock_id;
    int shm_labs_id;
    int shm_log_id;
    
    global_statistics_t *shm_stats;
    surgery_block_shm_t *shm_bo;
    pharmacy_shm_t *shm_stock;
    lab_queue_shm_t *shm_labs;
    critical_log_shm_t *shm_log;
} ipc_resources_t;

// Protótipos
int create_ipc_resources(ipc_resources_t *ipc, system_config_t *config);
void cleanup_ipc_resources(ipc_resources_t *ipc);

void triage_process(system_config_t *config, ipc_resources_t *ipc);
void surgery_process(system_config_t *config, ipc_resources_t *ipc);
void pharmacy_process(system_config_t *config, ipc_resources_t *ipc);
void laboratory_process(system_config_t *config, ipc_resources_t *ipc);

void log_to_file(ipc_resources_t *ipc, const char *component, const char *message);

#endif // IPC_H