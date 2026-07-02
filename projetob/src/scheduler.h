#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "config.h"

/*
 * CPU - representa um processador do sistema simulado.
 */
typedef struct {
    int id;        /* Identificador da CPU */
    int task_id;   /* Tarefa em execução (-1 = ociosa/desligada) */
    int active;    /* 1 = ligada; 0 = desligada */
    int idle_time; /* Total de ticks desligada */
} CPU;

void init_cpus(CPU cpus[], int cpu_count);

/*
 * scheduler_is_preemptive - informa se o algoritmo é preemptivo.
 *
 * Preemptivos : RR, SRTF, PRIOP (PRIOp), PRIOPENV/PRIOD (PRIOd)
 * Cooperativos: FCFS, SJF, PRIOC (PRIOc)
 *
 * Usado por assign_tasks() para NÃO tirar a CPU de uma tarefa em execução
 * nos algoritmos cooperativos (a tarefa só sai ao terminar, bloquear em
 * mutex ou iniciar E/S).
 */
int scheduler_is_preemptive(const char *algorithm);

/*
 * schedule - executa um passo do escalonador para uma única CPU.
 *
 * Algoritmos aceitos (case sensitive, já normalizados para maiúsculas):
 *   FCFS, RR, SJF, SRTF, PRIOC, PRIOP, PRIOD, PRIOPENV
 *
 * Retorna o índice da tarefa escolhida, ou -1 se não houver candidata.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used);

/*
 * assign_tasks - distribui tarefas entre todas as CPUs no tick atual.
 *
 * 'alpha' é usado apenas por PRIOd/PRIOPEnv (envelhecimento).
 * Retorna 1 se houve sorteio em alguma CPU, 0 caso contrário.
 */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick, int alpha);

#endif /* SCHEDULER_H */
