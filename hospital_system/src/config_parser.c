#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // Necessário para isspace
#include "../include/hospital.h"

// Função robusta para limpar espaços no início e fim
char* trim_space(char *str) {
    char *end;

    // Limpar espaços à esquerda
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) return str; // String vazia

    // Limpar espaços à direita
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Colocar o terminador nulo
    end[1] = '\0';

    return str;
}

int load_config(const char *filename, system_config_t *config) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir config.txt");
        return -1;
    }

    // INICIALIZAR A ZERO para evitar lixo de memória se a leitura falhar
    memset(config, 0, sizeof(system_config_t));

    char line[256];
    config->med_count = 0;

    while (fgets(line, sizeof(line), file)) {
        // Cortar comentários
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *delim = strchr(line, '=');
        if (!delim) continue; // Ignora linhas sem =

        *delim = '\0'; // Separa chave e valor
        
        char *key = trim_space(line);
        char *val = trim_space(delim + 1);

        if (strlen(key) == 0) continue;

        // --- Mapeamento ---
        // Globais
        if (strcmp(key, "TIME_UNIT_MS") == 0) config->time_unit_ms = atoi(val);
        else if (strcmp(key, "MAX_EMERGENCY_PATIENTS") == 0) config->max_emergency_patients = atoi(val);
        else if (strcmp(key, "MAX_APPOINTMENTS") == 0) config->max_appointments = atoi(val);
        else if (strcmp(key, "MAX_SURGERIES_PENDING") == 0) config->max_surgeries_pending = atoi(val);
        
        // Triagem
        else if (strcmp(key, "TRIAGE_SIMULTANEOUS_PATIENTS") == 0) config->triage_simultaneous_patients = atoi(val);
        else if (strcmp(key, "TRIAGE_CRITICAL_STABILITY") == 0) config->triage_critical_stability = atoi(val);
        else if (strcmp(key, "TRIAGE_EMERGENCY_DURATION") == 0) config->triage_emergency_duration = atoi(val);
        else if (strcmp(key, "TRIAGE_APPOINTMENT_DURATION") == 0) config->triage_appointment_duration = atoi(val);

        // Farmácia
        else if (strcmp(key, "PHARMACY_PREPARATION_TIME_MIN") == 0) config->pharmacy_prep_min = atoi(val);
        else if (strcmp(key, "PHARMACY_PREPARATION_TIME_MAX") == 0) config->pharmacy_prep_max = atoi(val);
        else if (strcmp(key, "AUTO_RESTOCK_ENABLED") == 0) config->auto_restock_enabled = atoi(val);
        else if (strcmp(key, "RESTOCK_QUANTITY_MULTIPLIER") == 0) config->restock_multiplier = atoi(val);

        // Laboratórios
        else if (strcmp(key, "LAB1_TEST_MIN_DURATION") == 0) config->lab1_min = atoi(val);
        else if (strcmp(key, "LAB1_TEST_MAX_DURATION") == 0) config->lab1_max = atoi(val);
        else if (strcmp(key, "LAB2_TEST_MIN_DURATION") == 0) config->lab2_min = atoi(val);
        else if (strcmp(key, "LAB2_TEST_MAX_DURATION") == 0) config->lab2_max = atoi(val);
        else if (strcmp(key, "MAX_SIMULTANEOUS_TESTS_LAB1") == 0) config->lab1_max_simultaneous = atoi(val);
        else if (strcmp(key, "MAX_SIMULTANEOUS_TESTS_LAB2") == 0) config->lab2_max_simultaneous = atoi(val);

        // Blocos Operatórios
        else if (strcmp(key, "B01_MIN_DURATION") == 0) config->b01_min_duration = atoi(val);
        else if (strcmp(key, "B01_MAX_DURATION") == 0) config->b01_max_duration = atoi(val);
        else if (strcmp(key, "B02_MIN_DURATION") == 0) config->b02_min_duration = atoi(val);
        else if (strcmp(key, "B02_MAX_DURATION") == 0) config->b02_max_duration = atoi(val);
        else if (strcmp(key, "B03_MIN_DURATION") == 0) config->b03_min_duration = atoi(val);
        else if (strcmp(key, "B03_MAX_DURATION") == 0) config->b03_max_duration = atoi(val);
        else if (strcmp(key, "CLEANUP_MIN_TIME") == 0) config->cleanup_min_time = atoi(val);
        else if (strcmp(key, "CLEANUP_MAX_TIME") == 0) config->cleanup_max_time = atoi(val);
        else if (strcmp(key, "MAX_MEDICAL_TEAMS") == 0) config->max_medical_teams = atoi(val);

        // Medicamentos (Formato: NOME = STOCK:THRESHOLD)
        // Se contiver ':' no valor, assumimos que é um medicamento
        else if (strchr(val, ':')) {
            if (config->med_count < 15) {
                int stock, threshold;
                if (sscanf(val, "%d:%d", &stock, &threshold) == 2) {
                    strncpy(config->meds[config->med_count].name, key, 49);
                    config->meds[config->med_count].initial_stock = stock;
                    config->meds[config->med_count].min_threshold = threshold;
                    config->med_count++;
                }
            }
        }
    }

    fclose(file);
    return 0;
}