#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "config.h"

/* =======================================================================
 * ESTRUTURA DO HARDWARE (CPU)
 * ======================================================================= */

/*
 * CPU — Representa uma unidade de processamento física ou lógica.
 * * Analogia: É uma "cadeira" onde um processo pode sentar para trabalhar.
 */
typedef struct {
    int id;        /* Número de identificação da CPU (ex: CPU 0, CPU 1) */
    int task_id;   /* ID da tarefa que está sentada na CPU agora (-1 se estiver vazia) */
    int active;    /* 1 se a CPU tem alguém trabalhando, 0 se ela estiver desligada/ociosa */
    int idle_time; /* Contador de quanto tempo a CPU ficou "coçando" (sem tarefa) */
} CPU;

/* Inicializa as cadeiras (CPUs) do sistema, deixando todas vazias no início */
void init_cpus(CPU cpus[], int cpu_count);

/* =======================================================================
 * CONTRATOS DO ESCALONADOR (Funções de Decisão)
 * ======================================================================= */

/*
 * scheduler_is_preemptive
 * Verifica se o algoritmo escolhido é "agressivo".
 * * Entenda a diferença:
 * - Algoritmo Preemptivo: O SO pode expulsar uma tarefa da CPU na força (por limite de tempo).
 * - Algoritmo Cooperativo: O SO pergunta "você já acabou?" e a tarefa só sai se quiser.
 * * Isso é fundamental para decidir se podemos interromper um processo em execução.
 */
int scheduler_is_preemptive(const char *algorithm);

/*
 * schedule
 * Esta função é o "cérebro" que toma a decisão de quem é o próximo a trabalhar.
 * * Parâmetros:
 * - algorithm: Qual regra usar (FCFS, RR, SJF, etc.)?
 * - tasks[]: A lista de todas as tarefas disponíveis no sistema.
 * - current_task: A tarefa que já está na CPU (usado para evitar trocas inúteis).
 * - lottery_used: Ponteiro para avisar se foi necessário um "sorteio" (empate).
 * * Retorna: O índice da tarefa escolhida, ou -1 se não houver ninguém pronto.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used);

/*
 * assign_tasks
 * Esta é a função principal que gerencia o fluxo de trabalho. 
 * Ela percorre todas as CPUs e todas as tarefas a cada "tick" de relógio (tempo).
 * * - Ela verifica quem deve sair da CPU (por fim de tempo ou bloqueio).
 * - Ela verifica quem deve entrar na CPU (seguindo a regra do algoritmo).
 * - Aplica a lógica de "envelhecimento" (alpha) caso seja prioridade dinâmica.
 * * Retorna: 1 se houve sorteio em algum momento, 0 se tudo correu sem empates.
 */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick, int alpha);

#endif /* SCHEDULER_H */
