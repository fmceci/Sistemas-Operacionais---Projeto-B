#ifndef TASK_H
#define TASK_H

#include "events.h"

/* Número máximo de tarefas suportadas na simulação */
#define MAX_TASKS 64

/*
 * TaskState - estados possíveis de uma tarefa no sistema operacional.
 */
typedef enum {
    NEW        = 0,
    READY      = 1,
    RUNNING    = 2,
    SUSP_MUTEX = 3,  /* suspensa por mutex indisponível */
    SUSP_IO    = 4,  /* suspensa por operação de E/S    */
    FINISHED   = 5
} TaskState;

/*
 * Task - Task Control Block (TCB).
 *
 * Armazena todas as informações de uma tarefa antes, durante e após
 * a simulação (requisito 1.3).
 */
typedef struct {
    /* --- Dados estáticos (lidos do arquivo de configuração) --- */
    int        id;               /* Identificador único da tarefa */
    char       color[8];         /* Cor RRGGBB hex, ex: "FF0000" */
    int        arrival_time;     /* Instante de ingresso */
    int        duration;         /* Duração total */
    int        priority;         /* Prioridade estática (maior = mais prioritário) */

    /* --- Campos de controle de execução (dinâmicos) --- */
    int        remaining_time;   /* Tempo restante para terminar */
    TaskState  state;            /* Estado atual */
    int        cpu_id;           /* CPU onde executa (-1 = nenhuma) */
    int        start_time;       /* Tick do primeiro início de execução */
    int        finish_time;      /* Tick de término */
    int        ticks_this_slice; /* Ticks no slice atual (quantum) */

    /* --- Campos do Projeto B --- */
    int        priority_dynamic; /* prioridade dinâmica (PRIOd/PRIOPEnv) */
    int        ticks_in_queue;   /* ticks acumulados na fila de prontos */
    int        ticks_executed;   /* ticks efetivamente executados */
    int        mutex_waiting;    /* mutex aguardado (-1 se nenhum) */

    /*
     * [CORREÇÃO BUG 4] Lista de eventos agora é DINÂMICA (ver events.h).
     * Contém ponteiro interno alocado com malloc/realloc; deve ser copiada
     * profundamente (event_list_copy) e liberada (event_list_free).
     */
    EventList  event_list;
} Task;

#endif /* TASK_H */
