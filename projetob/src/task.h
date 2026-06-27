#ifndef TASK_H
#define TASK_H

#include "events.h"

/* Número máximo de tarefas suportadas na simulação */
#define MAX_TASKS 64

/*
 * TaskState - estados possíveis de uma tarefa no sistema operacional.
 *
 * NEW         : Tarefa criada, aguardando o instante de ingresso.
 * READY       : Tarefa pronta para executar, aguardando CPU.
 * RUNNING     : Tarefa em execução em alguma CPU.
 * SUSP_MUTEX  : Tarefa suspensa aguardando um mutex (Projeto B, req 2).
 * SUSP_IO     : Tarefa suspensa realizando operação de E/S (Projeto B, req 3).
 * FINISHED    : Tarefa finalizou sua execução.
 *
 * Dois estados de suspensão distintos são necessários para diferenciação
 * visual no gráfico de Gantt (req 2.9 / 3.8).
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
 * a simulação (requisito 1.3 do enunciado).
 *
 * Campos novos do Projeto B marcados com [PB].
 */
typedef struct {
    /* --- Dados estáticos (lidos do arquivo de configuração) --- */
    int        id;               /* Identificador único da tarefa */
    char       color[8];         /* Cor no formato RRGGBB hex, ex: "FF0000" */
    int        arrival_time;     /* Instante de ingresso da tarefa */
    int        duration;         /* Duração total (tempo de execução puro) */
    int        priority;         /* Prioridade estática (maior = mais prioritário) */

    /* --- Campos de controle de execução (dinâmicos) --- */
    int        remaining_time;   /* Tempo restante para terminar */
    TaskState  state;            /* Estado atual da tarefa */
    int        cpu_id;           /* ID da CPU onde está executando (-1 = nenhuma) */
    int        start_time;       /* Tick em que começou a executar pela 1ª vez */
    int        finish_time;      /* Tick em que terminou */
    int        ticks_this_slice; /* Ticks executados no slice atual (para quantum) */

    /* --- Campos novos do Projeto B --- */

    /* [PB] Envelhecimento (PRIOPEnv, req 1):
     * priority_dynamic é recalculado a cada tick que a tarefa fica em READY.
     * Fórmula: priority_dynamic = priority + alpha * ticks_in_queue
     * É zerado (= priority) quando a tarefa ganha a CPU.                   */
    int        priority_dynamic; /* prioridade dinâmica (começa = priority)  */
    int        ticks_in_queue;   /* ticks acumulados na fila de prontos       */

    /* [PB] Tempo de execução relativo — necessário para mapear instantes
     * relativos dos eventos para ticks absolutos do relógio global.
     * Incrementado a cada tick em que a tarefa está RUNNING.                */
    int        ticks_executed;   /* ticks efetivamente executados acumulados  */

    /* [PB] Mutex — qual mutex a tarefa está aguardando (-1 = nenhum) */
    int        mutex_waiting;    /* ID do mutex aguardado; -1 se não aguarda  */

    /* [PB] Lista de eventos parseados (substituiu o campo char events[]) */
    EventList  event_list;       /* eventos da tarefa (ML, MU, IO)            */
} Task;

#endif /* TASK_H */
