#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/stat.h> 
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h> 
#include "../include/hospital.h"
#include "../include/ipc.h"
#include "../include/sync.h" // <--- IMPORTANTE


// --- Inicializar Stock ---
void init_stock(pharmacy_shm_t *stock) {
    memset(stock, 0, sizeof(pharmacy_shm_t));
    init_mutex(&stock->global_mutex); // Usa a do sync_utils

    strcpy(stock->medications[0].name, "ANALGESICO_A");
    stock->medications[0].current_stock = 1000;
    stock->medications[0].threshold = 100;
    init_mutex(&stock->medications[0].mutex);
    
    strcpy(stock->medications[1].name, "ANTIBIOTICO_B");
    stock->medications[1].current_stock = 800;
    stock->medications[1].threshold = 150;
    init_mutex(&stock->medications[1].mutex);

    strcpy(stock->medications[2].name, "ANESTESICO_C");
    stock->medications[2].current_stock = 500;
    stock->medications[2].threshold = 100;
    init_mutex(&stock->medications[2].mutex);

    for(int i=3; i<15; i++) {
        init_mutex(&stock->medications[i].mutex);
    }
}

int create_ipc_resources(ipc_resources_t *ipc, system_config_t *config) {
    (void)config; 

    // MQs
    if ((ipc->mq_urgent = msgget(ftok("Makefile", MQ_KEY_URGENT), IPC_CREAT | 0666)) == -1) return -1;
    if ((ipc->mq_normal = msgget(ftok("Makefile", MQ_KEY_NORMAL), IPC_CREAT | 0666)) == -1) return -1;
    if ((ipc->mq_resp = msgget(ftok("Makefile", MQ_KEY_RESP), IPC_CREAT | 0666)) == -1) return -1;

    // SHMs
    if ((ipc->shm_stats_id = shmget(ftok("Makefile", SHM_KEY_STATS), sizeof(global_statistics_t), IPC_CREAT | 0666)) == -1) return -1;
    ipc->shm_stats = (global_statistics_t*) shmat(ipc->shm_stats_id, NULL, 0);
    memset(ipc->shm_stats, 0, sizeof(global_statistics_t));
    init_mutex(&ipc->shm_stats->mutex);
    
    if ((ipc->shm_stock_id = shmget(ftok("Makefile", SHM_KEY_STOCK), sizeof(pharmacy_shm_t), IPC_CREAT | 0666)) == -1) return -1;
    ipc->shm_stock = (pharmacy_shm_t*) shmat(ipc->shm_stock_id, NULL, 0);
    init_stock(ipc->shm_stock);

    if ((ipc->shm_bo_id = shmget(ftok("Makefile", SHM_KEY_BO), sizeof(surgery_block_shm_t), IPC_CREAT | 0666)) == -1) return -1;
    ipc->shm_bo = (surgery_block_shm_t*) shmat(ipc->shm_bo_id, NULL, 0);
    memset(ipc->shm_bo, 0, sizeof(surgery_block_shm_t));
    init_mutex(&ipc->shm_bo->teams_mutex);
    for(int i=0; i<3; i++) init_mutex(&ipc->shm_bo->rooms[i].mutex);

    if ((ipc->shm_labs_id = shmget(ftok("Makefile", SHM_KEY_LABS), sizeof(lab_queue_shm_t), IPC_CREAT | 0666)) == -1) return -1;
    ipc->shm_labs = (lab_queue_shm_t*) shmat(ipc->shm_labs_id, NULL, 0);
    memset(ipc->shm_labs, 0, sizeof(lab_queue_shm_t));
    init_mutex(&ipc->shm_labs->lab1_mutex);
    init_mutex(&ipc->shm_labs->lab2_mutex);

    if ((ipc->shm_log_id = shmget(ftok("Makefile", SHM_KEY_LOG), sizeof(critical_log_shm_t), IPC_CREAT | 0666)) == -1) return -1;
    ipc->shm_log = (critical_log_shm_t*) shmat(ipc->shm_log_id, NULL, 0);
    memset(ipc->shm_log, 0, sizeof(critical_log_shm_t));
    init_mutex(&ipc->shm_log->mutex);

    // Semáforos
    sem_unlink(SEM_TEAMS); sem_unlink(SEM_BO1); sem_unlink(SEM_BO2); sem_unlink(SEM_BO3);
    sem_unlink(SEM_LAB1); sem_unlink(SEM_LAB2); sem_unlink(SEM_PHARM);

    sem_t *s;
    if ((s = sem_open(SEM_TEAMS, O_CREAT, 0644, 2)) != SEM_FAILED) sem_close(s);
    if ((s = sem_open(SEM_BO1, O_CREAT, 0644, 1)) != SEM_FAILED) sem_close(s);
    if ((s = sem_open(SEM_BO2, O_CREAT, 0644, 1)) != SEM_FAILED) sem_close(s);
    if ((s = sem_open(SEM_BO3, O_CREAT, 0644, 1)) != SEM_FAILED) sem_close(s);
    if ((s = sem_open(SEM_LAB1, O_CREAT, 0644, 1)) != SEM_FAILED) sem_close(s);
    if ((s = sem_open(SEM_LAB2, O_CREAT, 0644, 1)) != SEM_FAILED) sem_close(s);
    if ((s = sem_open(SEM_PHARM, O_CREAT, 0644, 1)) != SEM_FAILED) sem_close(s);

    // Pipes
    unlink(PIPE_INPUT); unlink(PIPE_TRIAGE); unlink(PIPE_SURGERY); unlink(PIPE_PHARMACY); unlink(PIPE_LAB);
    mkfifo(PIPE_INPUT, 0666); mkfifo(PIPE_TRIAGE, 0666); mkfifo(PIPE_SURGERY, 0666); mkfifo(PIPE_PHARMACY, 0666); mkfifo(PIPE_LAB, 0666);

    printf("[IPC] Recursos criados com sucesso.\n");
    return 0;
}

void cleanup_ipc_resources(ipc_resources_t *ipc) {
    msgctl(ipc->mq_urgent, IPC_RMID, NULL);
    msgctl(ipc->mq_normal, IPC_RMID, NULL);
    msgctl(ipc->mq_resp, IPC_RMID, NULL);
    
    if (ipc->shm_stats) { shmdt(ipc->shm_stats); shmctl(ipc->shm_stats_id, IPC_RMID, NULL); }
    if (ipc->shm_stock) { shmdt(ipc->shm_stock); shmctl(ipc->shm_stock_id, IPC_RMID, NULL); }
    if (ipc->shm_bo) { shmdt(ipc->shm_bo); shmctl(ipc->shm_bo_id, IPC_RMID, NULL); }
    if (ipc->shm_labs) { shmdt(ipc->shm_labs); shmctl(ipc->shm_labs_id, IPC_RMID, NULL); }
    if (ipc->shm_log) { shmdt(ipc->shm_log); shmctl(ipc->shm_log_id, IPC_RMID, NULL); }

    sem_unlink(SEM_TEAMS); sem_unlink(SEM_BO1); sem_unlink(SEM_BO2); sem_unlink(SEM_BO3);
    sem_unlink(SEM_LAB1); sem_unlink(SEM_LAB2); sem_unlink(SEM_PHARM);

    unlink(PIPE_INPUT); unlink(PIPE_TRIAGE); unlink(PIPE_SURGERY); unlink(PIPE_PHARMACY); unlink(PIPE_LAB);
    printf("[IPC] A limpar recursos...\n");
}