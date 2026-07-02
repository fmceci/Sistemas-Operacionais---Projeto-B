#ifndef SIMULATION_H
#define SIMULATION_H

#include "task.h"
#include "config.h"
#include "scheduler.h"
#include "gantt.h"
#include "mutex.h"
#include "io.h"

/*
 * SimulationState - encapsula todo o estado mutável da simulação.
 */
typedef struct {
    Task         tasks[MAX_TASKS];
    int          task_count;
    CPU          cpus[MAX_CPUS];
    Config       config;
    int          clock;
    GanttHistory history;

    MutexTable   mutex_table;   /* [PB] Mutex */
    IOQueue      io_queue;      /* [PB] E/S   */

    /* [CORREÇÃO BUG 1] Contador de ticks sem progresso (detecção de deadlock) */
    int          stall_ticks;
} SimulationState;

void limpar_buffer(void);
int  ler_inteiro_com_padrao(const char *mensagem, int valor_padrao);

void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count);

/* Libera a memória dinâmica das listas de eventos (chamada ao final). */
void simulation_free_events(SimulationState *sim);

int  simulation_step(SimulationState *sim);
void simulation_run_complete(SimulationState *sim);
void simulation_run_step_by_step(SimulationState *sim);

#endif /* SIMULATION_H */
