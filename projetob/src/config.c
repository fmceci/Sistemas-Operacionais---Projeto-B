#define _GNU_SOURCE   /* habilita getline() em algumas plataformas */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "events.h"

/*
 * str_to_upper - converte uma string para maiúsculas in-place (req 3.3.2).
 */
static void str_to_upper(char *s) {
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/*
 * extrair_id_numerico - extrai o primeiro número presente no token.
 *
 * [CORREÇÃO BUG 1] O ID pode vir como "t03", "T10", "3" etc. Ignoramos
 * qualquer prefixo não numérico e lemos os dígitos.
 */
static int extrair_id_numerico(const char *token) {
    if (token == NULL) return 0;
    while (*token && !isdigit((unsigned char)*token)) token++;
    return atoi(token);
}

int load_config(const char *filename, Config *config, Task tasks[], int *task_count) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", filename);
        return 0;
    }

    /*
     * [CORREÇÃO BUG 4] Leitura de linhas com tamanho ILIMITADO via getline.
     * 'line' é (re)alocado automaticamente por getline conforme necessário,
     * então linhas com milhares de eventos não são truncadas.
     */
    char  *line = NULL;
    size_t cap  = 0;

    /* --- Primeira linha: parâmetros gerais do sistema --- */
    if (getline(&line, &cap, file) < 0) {
        fprintf(stderr, "Erro: arquivo '%s' esta vazio.\n", filename);
        free(line);
        fclose(file);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';

    /* Algoritmo */
    char *token = strtok(line, ";");
    if (token != NULL) {
        strncpy(config->algorithm, token, MAX_ALGO - 1);
        config->algorithm[MAX_ALGO - 1] = '\0';
        str_to_upper(config->algorithm);
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

    /* Alpha (envelhecimento; usado por PRIOd/PRIOPEnv) */
    token = strtok(NULL, ";");
    config->alpha = (token != NULL) ? atoi(token) : 0;
    if (config->alpha < 0) config->alpha = 0;

    /* --- Linhas seguintes: uma tarefa por linha --- */
    *task_count = 0;
    while (getline(&line, &cap, file) >= 0 && *task_count < MAX_TASKS) {
        line[strcspn(line, "\r\n")] = '\0';

        /* Ignora linhas vazias ou comentários */
        if (strlen(line) == 0 || line[0] == '#') continue;

        Task *t = &tasks[*task_count];
        event_list_init(&t->event_list); /* garante ponteiro nulo antes de parsear */

        /*
         * [CORREÇÃO BUG 1] Fazemos o parsing "à mão" dos 5 primeiros campos
         * (id;cor;ingresso;duracao;prioridade) localizando os ';'. Assim, todo
         * o RESTANTE da linha (que contém os eventos, possivelmente com muitos
         * ';' internos) é capturado de uma só vez e passado a parse_events().
         *
         * Não usamos strtok aqui para não "perder" os separadores da cauda.
         */
        char *p = line;
        char *campos[5] = {0};
        int   nc = 0;

        /* separa os 5 primeiros campos */
        campos[nc++] = p;
        while (nc < 5) {
            char *sep = strchr(p, ';');
            if (sep == NULL) { p = NULL; break; }
            *sep = '\0';
            p = sep + 1;
            campos[nc++] = p;
        }

        /* Se a linha não tem os 5 campos mínimos, ignora */
        if (nc < 5) {
            event_list_free(&t->event_list);
            continue;
        }

        /* A cauda (eventos) é tudo que sobra após o 5º ';' */
        char *cauda_eventos = p; /* pode ser NULL se não houver eventos */

        /* id (aceita prefixo textual como "t03") */
        t->id = extrair_id_numerico(campos[0]);

        /* cor */
        if (campos[1] != NULL && campos[1][0] != '\0') {
            strncpy(t->color, campos[1], 7);
            t->color[7] = '\0';
        } else {
            strcpy(t->color, "AAAAAA");
        }

        /* ingresso */
        t->arrival_time = atoi(campos[2]);

        /* duração */
        t->duration = atoi(campos[3]);
        if (t->duration < 1) t->duration = 1;
        t->remaining_time = t->duration;

        /* prioridade */
        t->priority = atoi(campos[4]);

        /* eventos (toda a cauda de uma vez) */
        parse_events(cauda_eventos, &t->event_list);

        /* --- Inicializa campos de controle --- */
        t->state              = NEW;
        t->cpu_id             = -1;
        t->start_time         = -1;
        t->finish_time        = -1;
        t->ticks_this_slice   = 0;

        /* Campos do Projeto B */
        t->priority_dynamic   = t->priority;
        t->ticks_in_queue     = 0;
        t->ticks_executed     = 0;
        t->mutex_waiting      = -1;

        (*task_count)++;
    }

    free(line);
    fclose(file);

    if (*task_count == 0) {
        fprintf(stderr, "Erro: arquivo '%s' nao contém tarefas.\n", filename);
        return 0;
    }

    return 1;
}
