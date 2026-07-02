#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scheduler.h"

/* -----------------------------------------------------------------------
 * Inicialização
 * ----------------------------------------------------------------------- */
void init_cpus(CPU cpus[], int cpu_count) {
    for (int i = 0; i < cpu_count; i++) {
        cpus[i].id        = i;
        cpus[i].task_id   = -1;
        cpus[i].active    = 0;
        cpus[i].idle_time = 0;
    }
}

/* -----------------------------------------------------------------------
 * Classificação preemptivo x cooperativo
 * ----------------------------------------------------------------------- */
int scheduler_is_preemptive(const char *algorithm) {
    if (strcmp(algorithm, "RR")       == 0) return 1;
    if (strcmp(algorithm, "SRTF")     == 0) return 1;
    if (strcmp(algorithm, "PRIOP")    == 0) return 1;
    if (strcmp(algorithm, "PRIOD")    == 0) return 1;
    if (strcmp(algorithm, "PRIOPENV") == 0) return 1; /* sinônimo de PRIOd */
    /* FCFS, SJF, PRIOC → cooperativos */
    return 0;
}

/* -----------------------------------------------------------------------
 * Critérios de desempate (req 4.3)
 * ----------------------------------------------------------------------- */
static int tiebreak(Task tasks[], int idx_a, int idx_b,
                    int current_idx, int *lottery_used) {
    /* 1) favorece quem já está nesta CPU (evita troca desnecessária) */
    if (idx_a == current_idx) return idx_a;
    if (idx_b == current_idx) return idx_b;

    /* 2) menor instante de ingresso */
    if (tasks[idx_a].arrival_time != tasks[idx_b].arrival_time)
        return (tasks[idx_a].arrival_time < tasks[idx_b].arrival_time) ? idx_a : idx_b;

    /* 3) menor duração total */
    if (tasks[idx_a].duration != tasks[idx_b].duration)
        return (tasks[idx_a].duration < tasks[idx_b].duration) ? idx_a : idx_b;

    /* 4) sorteio */
    if (lottery_used) *lottery_used = 1;
    return (rand() % 2 == 0) ? idx_a : idx_b;
}

/* Desempate específico do PRIOd/PRIOPEnv: 1º critério é prioridade estática */
static int tiebreak_prio_static(Task tasks[], int idx_a, int idx_b,
                                int current_idx, int *lottery_used) {
    if (tasks[idx_a].priority != tasks[idx_b].priority)
        return (tasks[idx_a].priority > tasks[idx_b].priority) ? idx_a : idx_b;
    return tiebreak(tasks, idx_a, idx_b, current_idx, lottery_used);
}

/* -----------------------------------------------------------------------
 * Algoritmos de escalonamento
 *
 * Observação: todos selecionam entre tarefas no estado READY. A diferença
 * entre preemptivo e cooperativo é tratada em assign_tasks() (que decide se
 * "expõe" ou não as tarefas RUNNING para reavaliação).
 * ----------------------------------------------------------------------- */

/* FCFS / RR compartilham o critério base "menor ingresso". A preempção do RR
 * é feita pelo quantum em assign_tasks(); a seleção em si é por ordem de
 * chegada, o que reproduz a fila circular quando combinada ao quantum. */
static int schedule_fcfs(Task tasks[], int task_count,
                         int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].arrival_time < tasks[best].arrival_time) {
            best = i;
        } else if (tasks[i].arrival_time == tasks[best].arrival_time) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/* SJF: menor duração TOTAL (não-preemptivo) */
static int schedule_sjf(Task tasks[], int task_count,
                        int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].duration < tasks[best].duration) {
            best = i;
        } else if (tasks[i].duration == tasks[best].duration) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/* SRTF: menor tempo RESTANTE (preemptivo) */
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

/* Prioridade estática (usado por PRIOc cooperativo e PRIOp preemptivo).
 * Maior valor de prioridade = mais prioritária. */
static int schedule_prio_static(Task tasks[], int task_count,
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

/* Prioridade dinâmica (PRIOd/PRIOPEnv): maior priority_dynamic */
static int schedule_prio_dynamic(Task tasks[], int task_count,
                                 int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].priority_dynamic > tasks[best].priority_dynamic) {
            best = i;
        } else if (tasks[i].priority_dynamic == tasks[best].priority_dynamic) {
            best = tiebreak_prio_static(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used) {
    if (strcmp(algorithm, "FCFS") == 0) {
        return schedule_fcfs(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "RR") == 0) {
        /* RR usa a mesma seleção base do FCFS; a preempção vem do quantum */
        return schedule_fcfs(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "SJF") == 0) {
        return schedule_sjf(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "SRTF") == 0) {
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOC") == 0) {
        return schedule_prio_static(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOP") == 0) {
        return schedule_prio_static(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOD") == 0 ||
               strcmp(algorithm, "PRIOPENV") == 0) {
        return schedule_prio_dynamic(tasks, task_count, current_task, lottery_used);
    } else {
        fprintf(stderr, "Aviso: algoritmo '%s' desconhecido. Usando SRTF.\n", algorithm);
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    }
}

/* -----------------------------------------------------------------------
 * Distribuição de tarefas às CPUs
 * ----------------------------------------------------------------------- */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick, int alpha) {
    (void)tick;

    int global_lottery  = 0;
    int is_preemptive   = scheduler_is_preemptive(algorithm);
    int is_prio_dynamic = (strcmp(algorithm, "PRIOD") == 0 ||
                           strcmp(algorithm, "PRIOPENV") == 0);

    /* Passo 0 (PRIOd/PRIOPEnv): envelhecimento das tarefas em READY */
    if (is_prio_dynamic) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == READY) {
                tasks[i].ticks_in_queue++;
                tasks[i].priority_dynamic = tasks[i].priority
                                           + alpha * tasks[i].ticks_in_queue;
            }
        }
    }

    /*
     * Passo 1: preempção por quantum.
     *
     * RR e demais preemptivos respeitam o quantum: ao esgotá-lo, a tarefa
     * volta a READY. Os COOPERATIVOS (FCFS, SJF, PRIOc) NÃO são preemptados
     * por quantum — a tarefa continua até terminar/bloquear/fazer E/S.
     */
    if (is_preemptive) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == RUNNING &&
                tasks[i].ticks_this_slice >= quantum) {
                tasks[i].state            = READY;
                tasks[i].cpu_id           = -1;
                tasks[i].ticks_this_slice = 0;
                if (is_prio_dynamic) {
                    tasks[i].ticks_in_queue   = 0;
                    tasks[i].priority_dynamic = tasks[i].priority;
                }
            }
        }
    }

    /* Passo 2: para cada CPU, escolhe a tarefa que executará */
    for (int c = 0; c < cpu_count; c++) {

        /*
         * [COOPERATIVO] Se a CPU já tem uma tarefa RUNNING, ela NÃO é
         * preemptada: mantemos a tarefa e passamos para a próxima CPU.
         */
        if (!is_preemptive && cpus[c].active && cpus[c].task_id != -1) {
            int still_running = 0;
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].id == cpus[c].task_id &&
                    tasks[i].state == RUNNING) {
                    still_running = 1;
                    break;
                }
            }
            if (still_running) continue; /* mantém a tarefa atual */
        }

        /* Índice da tarefa que estava nesta CPU (para o desempate) */
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

        /* Expõe temporariamente as tarefas RUNNING desta CPU como READY,
         * para que o escalonador possa reavaliá-las (preemptivos). */
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
            /* Sem tarefa: desliga a CPU (req 1.2) */
            cpus[c].task_id = -1;
            cpus[c].active  = 0;
        } else {
            int same_task = (cpus[c].task_id == tasks[chosen_idx].id);

            cpus[c].task_id          = tasks[chosen_idx].id;
            cpus[c].active           = 1;
            tasks[chosen_idx].state  = RUNNING;
            tasks[chosen_idx].cpu_id = c;

            if (!same_task) tasks[chosen_idx].ticks_this_slice = 0;

            if (is_prio_dynamic) {
                tasks[chosen_idx].ticks_in_queue   = 0;
                tasks[chosen_idx].priority_dynamic = tasks[chosen_idx].priority;
            }
        }
    }

    return global_lottery;
}
