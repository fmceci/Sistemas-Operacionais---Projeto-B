 #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "events.h"

/*
 * str_to_upper - converte uma string para maiúsculas in-place.
 *
 * Necessário para atender ao requisito 3.3.2: strings como "SRTF",
 * "srtf" e "SrTf" devem ser tratadas como equivalentes.
 */
static void str_to_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/*
 * load_config - abre e interpreta o arquivo de configuração.
 *
 * Formato esperado:
 *   Linha 1 (Projeto A): algoritmo;quantum;qtde_cpus
 *   Linha 1 (Projeto B): PRIOPEnv;quantum;qtde_cpus;alpha
 *   Linha 2+: id;cor;ingresso;duracao;prioridade[;lista_eventos]
 *
 * Parâmetros:
 *   filename   - caminho do arquivo de configuração
 *   config     - ponteiro para a estrutura Config a preencher
 *   tasks      - vetor de Task a preencher
 *   task_count - ponteiro para o contador de tarefas lidas
 *
 * Retorna 1 em sucesso, 0 em falha.
 */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", filename);
        return 0;
    }

    char line[MAX_LINE];

    /* --- Lê a primeira linha: parâmetros gerais do sistema --- */
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Erro: arquivo '%s' esta vazio.\n", filename);
        fclose(file);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';

    /* Algoritmo */
    char *token = strtok(line, ";");
    if (token != NULL) {
        strncpy(config->algorithm, token, MAX_ALGO - 1);
        config->algorithm[MAX_ALGO - 1] = '\0';
        str_to_upper(config->algorithm); /* req 3.3.2 */
    } else {
        strcpy(config->algorithm, "SRTF");
    }

    /* Quantum */
    token = strtok(NULL, ";");
    config->quantum = (token != NULL) ? atoi(token) : 1;
    if (config->quantum < 1) config->quantum = 1;

    /* Número de CPUs */
    token = strtok(NULL, ";");
    config->cpu_count = (token != NULL) ? atoi(token) : 2;
    if (config->cpu_count < 2) config->cpu_count = 2;

    /*
     * Alpha (Projeto B, req 1.1) — presente apenas quando o algoritmo
     * é PRIOPEnv. Para os demais, alpha fica em 0 (sem efeito).
     */
    token = strtok(NULL, ";");
    config->alpha = (token != NULL) ? atoi(token) : 0;
    if (config->alpha < 0) config->alpha = 0;

    /* --- Lê as linhas seguintes: uma tarefa por linha --- */
    *task_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && *task_count < MAX_TASKS) {
        line[strcspn(line, "\r\n")] = '\0';

        /* Ignora linhas vazias ou comentários */
        if (strlen(line) == 0 || line[0] == '#') continue;

        Task *t = &tasks[*task_count];

        /* id */
        token = strtok(line, ";");
        if (token == NULL) continue;
        t->id = atoi(token);

        /* cor */
        token = strtok(NULL, ";");
        if (token != NULL) {
            strncpy(t->color, token, 7);
            t->color[7] = '\0';
        } else {
            strcpy(t->color, "AAAAAA");
        }

        /* ingresso */
        token = strtok(NULL, ";");
        t->arrival_time = (token != NULL) ? atoi(token) : 0;

        /* duração */
        token = strtok(NULL, ";");
        t->duration = (token != NULL) ? atoi(token) : 1;
        if (t->duration < 1) t->duration = 1;
        t->remaining_time = t->duration;

        /* prioridade */
        token = strtok(NULL, ";");
        t->priority = (token != NULL) ? atoi(token) : 0;

        /*
         * Lista de eventos (Projeto B) — o campo pode estar ausente.
         * parse_events() aceita NULL e "-" como "sem eventos".
         */
        token = strtok(NULL, ";");
        parse_events(token, &t->event_list);

        /* --- Inicializa campos de controle --- */
        t->state              = NEW;
        t->cpu_id             = -1;
        t->start_time         = -1;
        t->finish_time        = -1;
        t->ticks_this_slice   = 0;

        /* Campos novos do Projeto B */
        t->priority_dynamic   = t->priority;
        t->ticks_in_queue     = 0;
        t->ticks_executed     = 0;
        t->mutex_waiting      = -1;

        (*task_count)++;
    }

    fclose(file);

    if (*task_count == 0) {
        fprintf(stderr, "Erro: arquivo '%s' nao contém tarefas.\n", filename);
        return 0;
    }

    return 1;
}
