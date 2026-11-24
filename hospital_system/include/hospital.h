#ifndef HOSPITAL_H
#define HOSPITAL_H

// Estrutura para medicamentos (nome, stock, limite mínimo)
typedef struct {
    char name[50];
    int initial_stock;
    int min_threshold;
} med_config_t;

// Estrutura principal da Configuração
typedef struct {
    // Globais
    int time_unit_ms;
    int max_emergency_patients;
    int max_appointments;
    int max_surgeries_pending;
    
    // Triagem
    int triage_simultaneous_patients;
    int triage_critical_stability;
    int triage_emergency_duration;
    int triage_appointment_duration;
    
    // Blocos Operatórios
    int b01_min_duration;
    int b01_max_duration;
    int b02_min_duration;
    int b02_max_duration;
    int b03_min_duration;
    int b03_max_duration;
    int cleanup_min_time;
    int cleanup_max_time;
    int max_medical_teams;
    
    // Farmácia
    int pharmacy_prep_min;
    int pharmacy_prep_max;
    int auto_restock_enabled;
    int restock_multiplier;
    
    // Laboratórios
    int lab1_min;
    int lab1_max;
    int lab2_min;
    int lab2_max;
    int lab1_max_simultaneous;
    int lab2_max_simultaneous;

    // Array de medicamentos
    med_config_t meds[15];
    int med_count;

} system_config_t;

// Funções disponíveis
int load_config(const char *filename, system_config_t *config);

#endif