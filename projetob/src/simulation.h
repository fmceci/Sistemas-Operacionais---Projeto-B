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
 *
 * Campos novos do Projeto B marcados com [PB].
 */
typedef struct {
    Task         tasks[MAX_TASKS]; /* Vetor de TCBs                            */
    int          task_count;       /* Número de tarefas carregadas             */
    CPU          cpus[MAX_CPUS];   /* Vetor de CPUs                            */
    Config       config;           /* Configurações gerais                     */
    int          clock;            /* Relógio global atual (em ticks)          */
    GanttHistory history;          /* Histórico completo para Gantt e retroceder*/

    /* [PB] Mutex */
    MutexTable   mutex_table;      /* Tabela de mutexes da simulação           */

    /* [PB] E/S */
    IOQueue      io_queue;         /* Fila de operações de E/S em andamento    */
} SimulationState;

/* -----------------------------------------------------------------------
 * Funções auxiliares de entrada
 * ----------------------------------------------------------------------- */

void limpar_buffer(void);
int  ler_inteiro_com_padrao(const char *mensagem, int valor_padrao);

/* -----------------------------------------------------------------------
 * Controle da simulação
 * ----------------------------------------------------------------------- */

void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count);

/*
 * simulation_step - avança a simulação em exatamente um tick.
 *
 * Sequência do Projeto B (expandida do Projeto A):
 *   1. Verifica IRQs de E/S que terminaram (acorda tarefas → READY)
 *   2. Verifica chegadas de tarefas (NEW → READY)
 *   3. Escalonador distribui tarefas entre CPUs (inclui PRIOPEnv)
 *   4. Executa 1 tick (decrementa remaining_time, ticks_executed++)
 *   5. Processa eventos de mutex e E/S das tarefas em execução
 *   6. Registra snapshot no histórico do Gantt
 *   7. Avança o relógio
 *
 * Retorna 1 se ainda há tarefas pendentes, 0 se a simulação terminou.
 */
int simulation_step(SimulationState *sim);

void simulation_run_complete(SimulationState *sim);
void simulation_run_step_by_step(SimulationState *sim);

#endif /* SIMULATION_H */
