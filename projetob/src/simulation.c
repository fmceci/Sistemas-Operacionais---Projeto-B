#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "simulation.h"

/*
 * [CORREÇÃO BUG 1] Limite de segurança contra loop infinito / deadlock.
 *
 * Se a simulação passar STALL_LIMIT ticks CONSECUTIVOS sem NENHUM progresso
 * (nenhuma CPU executou e nenhuma E/S está pendente para acordar tarefas),
 * consideramos que houve deadlock (ex.: mutex nunca liberado) e encerramos.
 */
#define STALL_LIMIT 1000

/* -----------------------------------------------------------------------
 * Entrada auxiliar
 * ----------------------------------------------------------------------- */
void limpar_buffer(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

int ler_inteiro_com_padrao(const char *mensagem, int valor_padrao) {
    char buffer[32];
    printf(mensagem, valor_padrao);
    printf(": ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) return valor_padrao;
    if (buffer[0] == '\n') return valor_padrao;
    return atoi(buffer);
}

/* -----------------------------------------------------------------------
 * Inicialização
 * ----------------------------------------------------------------------- */
void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count) {
    memcpy(&sim->config, config, sizeof(Config));

    sim->task_count = task_count;
    for (int i = 0; i < task_count; i++) {
        sim->tasks[i] = tasks[i];
        /*
         * [CORREÇÃO BUG 4] A cópia acima é RASA para o ponteiro de eventos.
         * Fazemos uma CÓPIA PROFUNDA para que 'sim' seja dono da sua própria
         * memória de eventos e não haja double-free com o vetor original.
         */
        event_list_init(&sim->tasks[i].event_list);
        event_list_copy(&sim->tasks[i].event_list, &tasks[i].event_list);
    }

    sim->clock = 0;

    init_cpus(sim->cpus, config->cpu_count);
    gantt_init(&sim->history, config->cpu_count, task_count);

    mutex_table_init(&sim->mutex_table);
    io_queue_init(&sim->io_queue);
}

/*
 * simulation_free_events - libera a memória dinâmica das listas de eventos.
 * Deve ser chamada quando a simulação termina.
 */
void simulation_free_events(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        event_list_free(&sim->tasks[i].event_list);
    }
}

/* -----------------------------------------------------------------------
 * Funções internas de um tick
 * ----------------------------------------------------------------------- */
static void check_irq(SimulationState *sim, int irq_mask[]) {
    int done[MAX_TASKS];
    int n = io_check_irq(&sim->io_queue, sim->clock, done, MAX_TASKS);

    for (int k = 0; k < n; k++) {
        int tid = done[k];
        for (int i = 0; i < sim->task_count; i++) {
            if (sim->tasks[i].id == tid) {
                sim->tasks[i].state         = READY;
                sim->tasks[i].mutex_waiting = -1;
                irq_mask[i]                 = 1;
                break;
            }
        }
    }
}

static void check_arrivals(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state == NEW &&
            sim->tasks[i].arrival_time == sim->clock) {
            sim->tasks[i].state = READY;
            printf("  [NOVA TAREFA] T%d chegou ao sistema no tick %d.\n",
                   sim->tasks[i].id, sim->clock);
        }
    }
}

/*
 * execute_tick - executa 1 tick em cada CPU ativa.
 * Retorna quantas CPUs efetivamente executaram uma tarefa (para a
 * detecção de stall/deadlock).
 */
static int execute_tick(SimulationState *sim) {
    int cpus_que_executaram = 0;

    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (!sim->cpus[c].active || sim->cpus[c].task_id == -1) {
            sim->cpus[c].idle_time++;
            continue;
        }

        for (int i = 0; i < sim->task_count; i++) {
            if (sim->tasks[i].id != sim->cpus[c].task_id) continue;

            if (sim->tasks[i].start_time == -1) {
                sim->tasks[i].start_time = sim->clock;
            }

            sim->tasks[i].remaining_time--;
            sim->tasks[i].ticks_this_slice++;
            sim->tasks[i].ticks_executed++;
            cpus_que_executaram++;

            if (sim->tasks[i].remaining_time <= 0) {
                sim->tasks[i].state            = FINISHED;
                sim->tasks[i].finish_time      = sim->clock;
                sim->tasks[i].cpu_id           = -1;
                sim->tasks[i].ticks_this_slice = 0;

                sim->cpus[c].task_id = -1;
                sim->cpus[c].active  = 0;

                printf("  [CONCLUIDA]  T%d terminou no tick %d.\n",
                       sim->tasks[i].id, sim->clock);
            }
            break;
        }
    }

    return cpus_que_executaram;
}

static void check_events(SimulationState *sim,
                         int mutex_lock_mask[], int mutex_unlock_mask[],
                         int mutex_blocked_mask[], int io_start_mask[]) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state != RUNNING &&
            sim->tasks[i].state != FINISHED) continue;

        EventList *el = &sim->tasks[i].event_list;

        for (int j = 0; j < el->count; j++) {
            Event *ev = &el->list[j];

            if (ev->rel_tick != sim->tasks[i].ticks_executed) continue;

            if (ev->type == EVT_MUTEX_LOCK) {
                mutex_lock_mask[i] = 1;
                int blocked = mutex_lock(&sim->mutex_table,
                                         ev->mutex_id, sim->tasks[i].id);
                if (blocked) {
                    sim->tasks[i].state            = SUSP_MUTEX;
                    sim->tasks[i].mutex_waiting    = ev->mutex_id;
                    sim->tasks[i].cpu_id           = -1;
                    sim->tasks[i].ticks_this_slice = 0;

                    for (int c = 0; c < sim->config.cpu_count; c++) {
                        if (sim->cpus[c].task_id == sim->tasks[i].id) {
                            sim->cpus[c].task_id = -1;
                            sim->cpus[c].active  = 0;
                        }
                    }
                    mutex_blocked_mask[i] = 1;
                }

            } else if (ev->type == EVT_MUTEX_UNLOCK) {
                mutex_unlock_mask[i] = 1;
                int woken = mutex_unlock(&sim->mutex_table,
                                          ev->mutex_id, sim->tasks[i].id);
                if (woken >= 0) {
                    for (int k = 0; k < sim->task_count; k++) {
                        if (sim->tasks[k].id == woken) {
                            sim->tasks[k].state         = READY;
                            sim->tasks[k].mutex_waiting = -1;
                            break;
                        }
                    }
                }

            } else if (ev->type == EVT_IO) {
                io_start_mask[i] = 1;

                sim->tasks[i].state            = SUSP_IO;
                sim->tasks[i].cpu_id           = -1;
                sim->tasks[i].ticks_this_slice = 0;

                for (int c = 0; c < sim->config.cpu_count; c++) {
                    if (sim->cpus[c].task_id == sim->tasks[i].id) {
                        sim->cpus[c].task_id = -1;
                        sim->cpus[c].active  = 0;
                    }
                }

                io_schedule(&sim->io_queue, sim->tasks[i].id,
                            sim->clock + ev->duration);
            }
        }
    }
}

static int has_pending_tasks(SimulationState *sim) {
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state != FINISHED) return 1;
    }
    return 0;
}

/* Há alguma E/S pendente que ainda vai acordar alguma tarefa? */
static int has_pending_io(SimulationState *sim) {
    return sim->io_queue.count > 0;
}

static void print_cpu_status(SimulationState *sim) {
    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (sim->cpus[c].active) {
            printf("  CPU %d  -> T%d\n", c, sim->cpus[c].task_id);
        } else {
            printf("  CPU %d  -> desligada (ocioso: %d ticks)\n",
                   c, sim->cpus[c].idle_time);
        }
    }
}

/* -----------------------------------------------------------------------
 * Passo único de simulação
 * ----------------------------------------------------------------------- */
int simulation_step(SimulationState *sim) {
    if (!has_pending_tasks(sim)) return 0;

    printf("\n--- Tick %d ---\n", sim->clock);

    int irq_mask[MAX_TASKS];
    int mutex_lock_mask[MAX_TASKS];
    int mutex_unlock_mask[MAX_TASKS];
    int mutex_blocked_mask[MAX_TASKS];
    int io_start_mask[MAX_TASKS];
    memset(irq_mask,           0, sizeof(irq_mask));
    memset(mutex_lock_mask,    0, sizeof(mutex_lock_mask));
    memset(mutex_unlock_mask,  0, sizeof(mutex_unlock_mask));
    memset(mutex_blocked_mask, 0, sizeof(mutex_blocked_mask));
    memset(io_start_mask,      0, sizeof(io_start_mask));

    check_irq(sim, irq_mask);
    check_arrivals(sim);

    int lottery = assign_tasks(sim->config.algorithm,
                               sim->tasks, sim->task_count,
                               sim->cpus, sim->config.cpu_count,
                               sim->config.quantum,
                               sim->clock,
                               sim->config.alpha);
    if (lottery) {
        printf("  [SORTEIO]    Desempate aleatorio ocorreu neste tick.\n");
    }

    int executaram = execute_tick(sim);
    print_cpu_status(sim);

    check_events(sim, mutex_lock_mask, mutex_unlock_mask,
                 mutex_blocked_mask, io_start_mask);

    gantt_record(&sim->history, sim->clock,
                 sim->cpus, sim->config.cpu_count,
                 sim->tasks, sim->task_count,
                 lottery,
                 mutex_lock_mask, mutex_unlock_mask,
                 mutex_blocked_mask, io_start_mask,
                 irq_mask);

    sim->clock++;

    /*
     * [CORREÇÃO BUG 1] Detecção de deadlock / stall.
     *
     * Se neste tick nenhuma CPU executou (executaram == 0) e não há E/S
     * pendente que possa acordar tarefas, incrementamos o contador de stall.
     * Qualquer progresso zera o contador. Ao atingir STALL_LIMIT, abortamos.
     */
    if (executaram == 0 && !has_pending_io(sim)) {
        sim->stall_ticks++;
    } else {
        sim->stall_ticks = 0;
    }

    if (sim->stall_ticks >= STALL_LIMIT) {
        fprintf(stderr,
            "\n[DEADLOCK DETECTADO] Nenhum progresso apos %d ticks consecutivos.\n"
            "Provavel causa: mutex adquirido e nunca liberado (verifique os\n"
            "eventos MU no arquivo de entrada). Encerrando a simulacao.\n",
            STALL_LIMIT);
        return 0; /* encerra de forma controlada em vez de travar */
    }

    return has_pending_tasks(sim);
}

/* -----------------------------------------------------------------------
 * Execução completa
 * ----------------------------------------------------------------------- */
void simulation_run_complete(SimulationState *sim) {
    printf("=== Modo de Execucao Completa ===\n");

    while (simulation_step(sim)) { /* vazio */ }

    printf("\n=== Simulacao concluida no tick %d ===\n\n", sim->clock);

    printf("Tempo ocioso por CPU:\n");
    for (int c = 0; c < sim->config.cpu_count; c++) {
        printf("  CPU %d: %d tick(s) ociosa\n", c, sim->cpus[c].idle_time);
    }

    printf("\nResumo das tarefas:\n");
    for (int i = 0; i < sim->task_count; i++) {
        printf("  T%d | inicio: %d | fim: %d | turnaround: %d\n",
               sim->tasks[i].id,
               sim->tasks[i].start_time,
               sim->tasks[i].finish_time,
               sim->tasks[i].finish_time - sim->tasks[i].arrival_time);
    }

    gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
    gantt_save_svg(&sim->history, sim->tasks, sim->task_count, "gantt.svg");

    simulation_free_events(sim); /* [BUG 4] libera memória dos eventos */
}

/* -----------------------------------------------------------------------
 * Modo passo-a-passo
 * ----------------------------------------------------------------------- */
static void restore_snapshot(SimulationState *sim, int snapshot_index) {
    if (snapshot_index < 0 || snapshot_index >= sim->history.count) return;

    const GanttEntry *e = &sim->history.entries[snapshot_index];
    sim->clock = e->tick;

    for (int i = 0; i < sim->task_count; i++) {
        sim->tasks[i].state            = e->task_state[i];
        sim->tasks[i].remaining_time   = e->task_remaining[i];
        sim->tasks[i].ticks_this_slice = e->task_slice[i];
        sim->tasks[i].ticks_executed   = 0;
        sim->tasks[i].mutex_waiting    = -1;
        sim->tasks[i].priority_dynamic = sim->tasks[i].priority;
        sim->tasks[i].ticks_in_queue   = 0;
        /* NOTA: event_list NÃO é tocada aqui — os eventos são estáticos e
         * permanecem alocados; só os campos de controle são restaurados. */
    }

    for (int c = 0; c < sim->config.cpu_count; c++) {
        sim->cpus[c].task_id = e->cpu_task[c];
        sim->cpus[c].active  = e->cpu_active[c];
    }

    for (int i = 0; i < sim->task_count; i++) {
        sim->tasks[i].cpu_id = -1;
        if (sim->tasks[i].state != RUNNING)
            sim->tasks[i].ticks_this_slice = 0;
    }

    for (int c = 0; c < sim->config.cpu_count; c++) {
        int tid = sim->cpus[c].task_id;
        if (sim->cpus[c].active && tid != -1) {
            for (int i = 0; i < sim->task_count; i++) {
                if (sim->tasks[i].id == tid) {
                    sim->tasks[i].cpu_id = c;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].state == RUNNING && sim->tasks[i].cpu_id == -1) {
            sim->tasks[i].state = READY;
            sim->tasks[i].ticks_this_slice = 0;
        }
    }
}

static void prompt_modify_task(SimulationState *sim) {
    int id;
    printf("Digite o ID da tarefa a modificar (ou -1 para cancelar): ");
    if (scanf("%d", &id) != 1) { limpar_buffer(); return; }
    limpar_buffer();
    if (id == -1) return;

    int idx = -1;
    for (int i = 0; i < sim->task_count; i++) {
        if (sim->tasks[i].id == id) { idx = i; break; }
    }
    if (idx == -1) { printf("Tarefa T%d nao encontrada.\n", id); return; }

    const char *estados[] = { "NEW", "READY", "RUNNING",
                               "SUSP_MUTEX", "SUSP_IO", "FINISHED" };
    printf("Estado atual de T%d: %s | Restante: %d\n",
           id, estados[sim->tasks[idx].state], sim->tasks[idx].remaining_time);

    printf("Novo estado (0=NEW,1=READY,2=RUNNING,3=SUSP_MUTEX,4=SUSP_IO,5=FINISHED,-1=manter): ");
    int new_state;
    if (scanf("%d", &new_state) != 1) { limpar_buffer(); return; }
    limpar_buffer();

    if (new_state >= 0 && new_state <= 5) {
        sim->tasks[idx].state = (TaskState)new_state;

        if (new_state == FINISHED) {
            sim->tasks[idx].remaining_time   = 0;
            sim->tasks[idx].cpu_id           = -1;
            sim->tasks[idx].ticks_this_slice = 0;
            if (sim->tasks[idx].finish_time == -1)
                sim->tasks[idx].finish_time = sim->clock;
        }

        if (sim->tasks[idx].state == RUNNING && sim->tasks[idx].cpu_id == -1) {
            sim->tasks[idx].state = READY;
            sim->tasks[idx].ticks_this_slice = 0;
        }

        if (sim->tasks[idx].state == READY ||
            sim->tasks[idx].state == NEW   ||
            sim->tasks[idx].state == SUSP_MUTEX ||
            sim->tasks[idx].state == SUSP_IO) {
            sim->tasks[idx].cpu_id           = -1;
            sim->tasks[idx].ticks_this_slice = 0;
        }

        printf("Estado de T%d alterado para %s.\n", id, estados[new_state]);
    }

    if (sim->tasks[idx].state != FINISHED) {
        printf("Novo tempo restante (atual=%d, -1=manter): ",
               sim->tasks[idx].remaining_time);
        int new_rem;
        if (scanf("%d", &new_rem) == 1 && new_rem >= 0) {
            sim->tasks[idx].remaining_time = new_rem;
            printf("Tempo restante de T%d alterado para %d.\n", id, new_rem);
        }
        limpar_buffer();
    }
}

static void inspect_state(SimulationState *sim) {
    printf("\n--- Estado do sistema no tick %d ---\n", sim->clock);
    const char *estados[] = { "NEW","READY","RUNNING","SUSP_MUTEX","SUSP_IO","FINISHED" };

    printf("Tarefas:\n");
    for (int i = 0; i < sim->task_count; i++) {
        int si = (int)sim->tasks[i].state;
        if (si < 0 || si > 5) si = 0;
        printf("  T%-3d | %-10s | Rest:%3d | Prio:%2d | PrioDyn:%3d | "
               "CPU:%2d | Slice:%d/%d | TExec:%d | MutxWait:%d | Eventos:%d\n",
               sim->tasks[i].id,
               estados[si],
               sim->tasks[i].remaining_time,
               sim->tasks[i].priority,
               sim->tasks[i].priority_dynamic,
               sim->tasks[i].cpu_id,
               sim->tasks[i].ticks_this_slice,
               sim->config.quantum,
               sim->tasks[i].ticks_executed,
               sim->tasks[i].mutex_waiting,
               sim->tasks[i].event_list.count);
    }

    printf("CPUs:\n");
    for (int c = 0; c < sim->config.cpu_count; c++) {
        if (sim->cpus[c].active) {
            printf("  CPU %d -> T%d\n", c, sim->cpus[c].task_id);
        } else {
            printf("  CPU %d -> desligada (%d ticks ociosa)\n",
                   c, sim->cpus[c].idle_time);
        }
    }

    printf("Mutexes (%d):\n", sim->mutex_table.count);
    for (int m = 0; m < sim->mutex_table.count; m++) {
        Mutex *mx = &sim->mutex_table.mutexes[m];
        printf("  M%d | dono: %s | fila(%d):",
               mx->id,
               mx->owner_id == -1 ? "livre" : "ocupado",
               mx->queue_len);
        for (int q = 0; q < mx->queue_len; q++)
            printf(" T%d", mx->wait_queue[q]);
        printf("\n");
    }

    printf("E/S ativas (%d):\n", sim->io_queue.count);
    for (int k = 0; k < MAX_IO_OPS; k++) {
        if (sim->io_queue.ops[k].active) {
            printf("  T%d -> termina no tick %d\n",
                   sim->io_queue.ops[k].task_id,
                   sim->io_queue.ops[k].finish_tick);
        }
    }
    printf("---\n");
}

void simulation_run_step_by_step(SimulationState *sim) {
    printf("=== Modo Passo-a-Passo ===\n");
    printf("Comandos: [n]ext | [b]ack | [m]odify | [i]nspect | [q]uit\n\n");

    int snapshot_pos = -1;
    int running = 1;

    while (running) {
        printf("\nTick atual: %d | Historico: %d entradas\n",
               sim->clock, sim->history.count);
        printf("Comandos: [n]ext | [b]ack | [m]odify | [i]nspect | [q]uit\n");
        printf("Digite o comando desejado: ");

        char cmd[8];
        if (scanf("%7s", cmd) != 1) break;
        limpar_buffer();

        if (cmd[0] == 'n') {
            if (snapshot_pos >= 0) {
                sim->history.count = snapshot_pos + 1;
                restore_snapshot(sim, snapshot_pos);
                sim->clock = sim->history.entries[snapshot_pos].tick + 1;
                snapshot_pos = -1;
            }

            if (!simulation_step(sim)) {
                printf("\n[SIMULACAO CONCLUIDA]\n");
                printf("\nResumo das tarefas:\n");
                for (int i = 0; i < sim->task_count; i++) {
                    printf("  T%d | inicio: %d | fim: %d | turnaround: %d\n",
                           sim->tasks[i].id,
                           sim->tasks[i].start_time,
                           sim->tasks[i].finish_time,
                           sim->tasks[i].finish_time - sim->tasks[i].arrival_time);
                }
                gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
                running = 0;
            } else {
                gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
            }

        } else if (cmd[0] == 'b') {
            int target = (snapshot_pos < 0)
                         ? sim->history.count - 2
                         : snapshot_pos - 1;

            if (target < 0) {
                printf("Nao ha historico para retroceder.\n");
            } else {
                snapshot_pos = target;
                restore_snapshot(sim, snapshot_pos);
                printf("Retrocedeu para tick %d.\n",
                       sim->history.entries[snapshot_pos].tick);

                int old_count = sim->history.count;
                sim->history.count = snapshot_pos + 1;
                gantt_print_terminal(&sim->history, sim->tasks, sim->task_count);
                sim->history.count = old_count;
            }

        } else if (cmd[0] == 'm') {
            prompt_modify_task(sim);
        } else if (cmd[0] == 'i') {
            inspect_state(sim);
        } else if (cmd[0] == 'q') {
            running = 0;
        } else {
            printf("Comando desconhecido. Use: n, b, m, i, q\n");
        }
    }

    printf("\nTempo ocioso por CPU:\n");
    for (int c = 0; c < sim->config.cpu_count; c++) {
        printf("  CPU %d: %d tick(s) desligada\n", c, sim->cpus[c].idle_time);
    }

    gantt_save_svg(&sim->history, sim->tasks, sim->task_count, "gantt.svg");
    printf("Simulacao encerrada.\n");

    simulation_free_events(sim); /* [BUG 4] libera memória dos eventos */
}
