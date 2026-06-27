#ifndef GANTT_H
#define GANTT_H

#include "task.h"
#include "scheduler.h"
#include "config.h"

/* Número máximo de ticks que o histórico do Gantt pode armazenar */
#define MAX_TICKS 1024

/*
 * GanttEntry - snapshot do estado completo do sistema em um único tick.
 *
 * Armazena: qual tarefa estava em cada CPU, estado e tempo restante de
 * cada tarefa, e flags de eventos especiais (chegada, fim, sorteio,
 * mutex, E/S, IRQ).
 *
 * Usado para avançar/retroceder a simulação (req 1.5.2) e para gerar
 * o gráfico de Gantt com todos os elementos visuais obrigatórios.
 *
 * Campos novos do Projeto B marcados com [PB].
 */
typedef struct {
    int       tick;                        /* Instante de tempo deste snapshot   */
    int       cpu_task[MAX_CPUS];          /* ID da tarefa em cada CPU (-1=off)  */
    int       cpu_active[MAX_CPUS];        /* 1 se CPU estava ligada neste tick  */
    TaskState task_state[MAX_TASKS];       /* Estado de cada tarefa neste tick   */
    int       task_remaining[MAX_TASKS];   /* Tempo restante de cada tarefa      */
    int       lottery_tick;                /* 1 se houve desempate por sorteio   */
    int       task_arrived[MAX_TASKS];     /* 1 se chegou (NEW→READY) neste tick */
    int       task_finished[MAX_TASKS];    /* 1 se terminou exatamente neste tick*/
    int       task_slice[MAX_TASKS];       /* ticks_this_slice de cada tarefa    */

    /* [PB] Eventos de mutex — para ícones no Gantt (req 2.8) */
    int       task_mutex_lock[MAX_TASKS];   /* 1 se fez lock de mutex neste tick  */
    int       task_mutex_unlock[MAX_TASKS]; /* 1 se fez unlock de mutex neste tick*/
    int       task_mutex_blocked[MAX_TASKS];/* 1 se foi bloqueada por mutex       */

    /* [PB] Eventos de E/S — para ícones no Gantt (req 3 / 5.1) */
    int       task_io_start[MAX_TASKS];     /* 1 se iniciou E/S neste tick        */
    int       task_irq[MAX_TASKS];          /* 1 se recebeu IRQ (E/S terminou)    */
} GanttEntry;

/*
 * GanttHistory - histórico completo da simulação, tick a tick.
 */
typedef struct {
    GanttEntry entries[MAX_TICKS]; /* Um snapshot por tick        */
    int        count;              /* Número de entradas gravadas */
    int        cpu_count;          /* Número de CPUs              */
    int        task_count;         /* Número de tarefas           */
} GanttHistory;

/* -----------------------------------------------------------------------
 * API pública
 * ----------------------------------------------------------------------- */

void gantt_init(GanttHistory *history, int cpu_count, int task_count);

/*
 * gantt_record - registra o estado do sistema no tick atual.
 *
 * Parâmetros novos do Projeto B:
 *   mutex_lock_mask    - bitmask (por índice de tarefa) dos locks ocorridos
 *   mutex_unlock_mask  - bitmask dos unlocks ocorridos
 *   mutex_blocked_mask - bitmask das tarefas bloqueadas por mutex
 *   io_start_mask      - bitmask das tarefas que iniciaram E/S
 *   irq_mask           - bitmask das tarefas que receberam IRQ
 *
 * Todos os masks podem ser NULL/zero quando não há eventos do tipo.
 */
void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used,
                  int mutex_lock_mask[], int mutex_unlock_mask[],
                  int mutex_blocked_mask[], int io_start_mask[],
                  int irq_mask[]);

void gantt_print_terminal(const GanttHistory *history,
                          Task tasks[], int task_count);

void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename);

#endif /* GANTT_H */
