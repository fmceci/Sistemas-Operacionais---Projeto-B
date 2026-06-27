#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"
#include "config.h"

/*
 * CPU - representa um processador do sistema simulado.
 *
 * Conforme o enunciado, o sistema possui múltiplos processadores.
 * O escalonador deve minimizar a ociosidade: se não há tarefa READY,
 * o processador é desligado (requisito 1.2).
 */
typedef struct {
    int id;        /* Identificador da CPU (0 até cpu_count-1) */
    int task_id;   /* ID da tarefa em execução (-1 = CPU ociosa/desligada) */
    int active;    /* 1 = CPU ligada; 0 = CPU desligada (sem tarefa pronta) */
    int idle_time; /* Total de ticks em que esta CPU ficou desligada        */
} CPU;

/*
 * init_cpus - inicializa o vetor de CPUs com os valores padrão (todas desligadas).
 */
void init_cpus(CPU cpus[], int cpu_count);

/*
 * schedule - executa um passo do escalonador para uma única CPU.
 *
 * Seleciona a melhor tarefa READY conforme o algoritmo configurado,
 * aplicando os critérios de desempate dos requisitos 4.3 e 4.4.
 *
 * Parâmetros:
 *   algorithm    - string do algoritmo ("SRTF", "PRIOP" ou "PRIOPENV")
 *   tasks        - vetor de todas as tarefas
 *   task_count   - número de tarefas
 *   current_task - índice da tarefa que estava nesta CPU (-1 = nenhuma)
 *   lottery_used - ponteiro para flag de sorteio (saída)
 *
 * Retorna o índice da tarefa escolhida, ou -1 se não houver candidata.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used);

/*
 * assign_tasks - distribui tarefas entre todas as CPUs para o tick atual.
 *
 * Liga/desliga CPUs conforme disponibilidade de tarefas prontas (req 1.2).
 * Respeita o quantum configurado: se a tarefa esgotou seu slice, é preemptada.
 *
 * Parâmetros adicionais do Projeto B:
 *   alpha - fator de envelhecimento para PRIOPEnv (0 para outros algoritmos)
 *
 * Retorna 1 se houve sorteio em alguma CPU, 0 caso contrário.
 */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick, int alpha);

#endif /* SCHEDULER_H */
