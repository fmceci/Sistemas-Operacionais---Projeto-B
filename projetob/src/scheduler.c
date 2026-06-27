#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scheduler.h"

/*
 * init_cpus - inicializa todas as CPUs como ociosas e desligadas.
 */
void init_cpus(CPU cpus[], int cpu_count) {
    for (int i = 0; i < cpu_count; i++) {
        cpus[i].id        = i;
        cpus[i].task_id   = -1;
        cpus[i].active    = 0;
        cpus[i].idle_time = 0;
    }
}

/* -----------------------------------------------------------------------
 * Funções auxiliares de comparação para os critérios de desempate
 *
 * PRIOP / SRTF (req 4.3):
 *   1. Tarefa que já está executando nesta CPU (evita troca desnecessária)
 *   2. Menor instante de ingresso (quem chegou antes tem prioridade)
 *   3. Menor duração total da tarefa
 *   4. Sorteio (aleatório; ativa flag lottery_used)
 *
 * PRIOPEnv (req 1.3 do Projeto B):
 *   1. Maior prioridade estática (priority, não dinâmica)
 *   2. Tarefa que já está executando nesta CPU
 *   3. Menor instante de ingresso
 *   4. Menor duração total
 *   5. Sorteio
 * ----------------------------------------------------------------------- */

/*
 * tiebreak - decide entre dois candidatos usando os critérios de desempate
 * definidos no requisito 4.3.
 */
static int tiebreak(Task tasks[], int idx_a, int idx_b,
                    int current_idx, int *lottery_used) {
    /* Critério 1: favorece quem já está executando nesta CPU */
    if (idx_a == current_idx) return idx_a;
    if (idx_b == current_idx) return idx_b;

    /* Critério 2: menor instante de ingresso */
    if (tasks[idx_a].arrival_time != tasks[idx_b].arrival_time)
        return (tasks[idx_a].arrival_time < tasks[idx_b].arrival_time) ? idx_a : idx_b;

    /* Critério 3: menor duração total */
    if (tasks[idx_a].duration != tasks[idx_b].duration)
        return (tasks[idx_a].duration < tasks[idx_b].duration) ? idx_a : idx_b;

    /* Critério 4: sorteio */
    if (lottery_used) *lottery_used = 1;
    return (rand() % 2 == 0) ? idx_a : idx_b;
}

/*
 * tiebreak_priopenv - desempate específico para PRIOPEnv (req 1.3 do Proj B).
 *
 * Difere do tiebreak padrão porque o primeiro critério é a prioridade
 * ESTÁTICA (não a dinâmica), conforme especificado no enunciado.
 * Os demais critérios seguem a ordem do req 4.3.
 */
static int tiebreak_priopenv(Task tasks[], int idx_a, int idx_b,
                              int current_idx, int *lottery_used) {
    /* Critério 1 (PRIOPEnv): maior prioridade estática */
    if (tasks[idx_a].priority != tasks[idx_b].priority)
        return (tasks[idx_a].priority > tasks[idx_b].priority) ? idx_a : idx_b;

    /* Critérios 2-5: igual ao tiebreak padrão (req 4.3) */
    return tiebreak(tasks, idx_a, idx_b, current_idx, lottery_used);
}

/* -----------------------------------------------------------------------
 * Algoritmos de escalonamento
 * ----------------------------------------------------------------------- */

/*
 * schedule_srtf - seleciona a tarefa READY com menor tempo restante (SRTF).
 */
static int schedule_srtf(Task tasks[], int task_count,
                         int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].remaining_time < tasks[best].remaining_time) {
            best = i;
        } else if (tasks[i].remaining_time == tasks[best].remaining_time) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/*
 * schedule_priop - seleciona a tarefa READY com maior prioridade estática.
 *
 * PRIOP (Prioridade Preemptivo): maior valor de prioridade = mais prioritária.
 * Conforme requisito 4.4, o 1º critério de desempate é a prioridade estática,
 * seguido pelos demais critérios do req 4.3.
 */
static int schedule_priop(Task tasks[], int task_count,
                          int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].priority > tasks[best].priority) {
            best = i;
        } else if (tasks[i].priority == tasks[best].priority) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/*
 * schedule_priopenv - seleciona a tarefa READY com maior PRIORIDADE DINÂMICA.
 *
 * Projeto B, requisito 1: Prioridade preemptiva com envelhecimento.
 *
 * priority_dynamic é atualizado em assign_tasks() a cada tick que a tarefa
 * espera na fila. Aqui apenas selecionamos quem tem o maior valor dinâmico.
 *
 * Em caso de empate no valor dinâmico, aplica tiebreak_priopenv() que usa
 * prioridade estática como primeiro critério (conforme req 1.3 do Proj B).
 */
static int schedule_priopenv(Task tasks[], int task_count,
                              int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].priority_dynamic > tasks[best].priority_dynamic) {
            best = i;
        } else if (tasks[i].priority_dynamic == tasks[best].priority_dynamic) {
            best = tiebreak_priopenv(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/*
 * schedule - ponto de entrada do escalonador para uma única CPU.
 *
 * Despacha para o algoritmo correto conforme a string 'algorithm'.
 * Algoritmos desconhecidos fazem fallback para SRTF com aviso.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used) {
    if (strcmp(algorithm, "SRTF") == 0) {
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOP") == 0) {
        return schedule_priop(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOPENV") == 0) {
        return schedule_priopenv(tasks, task_count, current_task, lottery_used);
    } else {
        fprintf(stderr, "Aviso: algoritmo '%s' desconhecido. Usando SRTF.\n", algorithm);
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    }
}

/*
 * assign_tasks - distribui tarefas entre todas as CPUs para o tick atual.
 *
 * Projeto B: recebe o parâmetro 'alpha' para o envelhecimento do PRIOPEnv.
 * Para SRTF e PRIOP, alpha é ignorado (passado como 0).
 *
 * Algoritmo:
 *   0. [PRIOPEnv] Atualiza prioridade dinâmica das tarefas READY.
 *   1. Verifica quais tarefas RUNNING esgotaram o quantum; essas voltam a READY.
 *   2. Para cada CPU, o escalonador escolhe a melhor tarefa READY disponível.
 *   3. CPUs sem tarefa disponível são desligadas (req 1.2).
 */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick, int alpha) {
    (void)tick; /* reservado para uso futuro */

    int global_lottery = 0;

    /*
     * Passo 0 (PRIOPEnv): aplica envelhecimento nas tarefas que estão
     * na fila de prontos (READY). A cada tick de espera, a prioridade
     * dinâmica sobe em 'alpha', evitando inanição (req 1 do Proj B).
     *
     * Tarefas RUNNING têm ticks_in_queue e priority_dynamic zerados
     * quando ganham a CPU (feito no Passo 2 abaixo).
     */
    if (strcmp(algorithm, "PRIOPENV") == 0) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == READY) {
                tasks[i].ticks_in_queue++;
                tasks[i].priority_dynamic = tasks[i].priority
                                           + alpha * tasks[i].ticks_in_queue;
            }
        }
    }

    /*
     * Passo 1: libera tarefas RUNNING cujo quantum esgotou.
     * Tarefas que ainda têm slice disponível ficam RUNNING (para o critério 1
     * de desempate: favorecer quem já está executando).
     */
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state == RUNNING) {
            if (tasks[i].ticks_this_slice >= quantum) {
                tasks[i].state            = READY;
                tasks[i].cpu_id           = -1;
                tasks[i].ticks_this_slice = 0;
                /* Ao voltar para READY, a contagem de fila reinicia */
                if (strcmp(algorithm, "PRIOPENV") == 0) {
                    tasks[i].ticks_in_queue   = 0;
                    tasks[i].priority_dynamic = tasks[i].priority;
                }
            }
        }
    }

    /*
     * Passo 2: para cada CPU, decide qual tarefa vai executar neste tick.
     */
    for (int c = 0; c < cpu_count; c++) {
        /* Identifica o índice da tarefa que estava nesta CPU (desempate) */
        int current_idx = -1;
        if (cpus[c].task_id != -1) {
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].id == cpus[c].task_id &&
                    (tasks[i].state == READY || tasks[i].state == RUNNING)) {
                    current_idx = i;
                    break;
                }
            }
        }

        /* Expõe temporariamente as tarefas RUNNING desta CPU como READY */
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == RUNNING && tasks[i].cpu_id == cpus[c].id) {
                tasks[i].state = READY;
            }
        }

        int lottery_used = 0;
        int chosen_idx   = schedule(algorithm, tasks, task_count,
                                    current_idx, &lottery_used);

        if (lottery_used) global_lottery = 1;

        if (chosen_idx == -1) {
            /* Sem tarefa disponível: desliga a CPU (req 1.2) */
            cpus[c].task_id = -1;
            cpus[c].active  = 0;
        } else {
            int same_task = (cpus[c].task_id == tasks[chosen_idx].id);

            cpus[c].task_id          = tasks[chosen_idx].id;
            cpus[c].active           = 1;
            tasks[chosen_idx].state  = RUNNING;
            tasks[chosen_idx].cpu_id = c;

            if (!same_task) {
                tasks[chosen_idx].ticks_this_slice = 0;
            }

            /*
             * Ao ganhar a CPU, zera o contador de envelhecimento.
             * A prioridade dinâmica volta para a estática.
             */
            if (strcmp(algorithm, "PRIOPENV") == 0) {
                tasks[chosen_idx].ticks_in_queue   = 0;
                tasks[chosen_idx].priority_dynamic = tasks[chosen_idx].priority;
            }
        }
    }

    return global_lottery;
}
