#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gantt.h"

/* -----------------------------------------------------------------------
 * Constantes de layout do terminal
 * ----------------------------------------------------------------------- */
#define TERM_CELL_W  4
#define LABEL_W      10

/* -----------------------------------------------------------------------
 * Constantes de layout do SVG (em pixels)
 * ----------------------------------------------------------------------- */
#define SVG_TICK_W      44
#define SVG_ROW_H       36
#define SVG_LABEL_W    115
#define SVG_MARGIN_TOP  62
#define SVG_LEGEND_H   320  /* expandida para incluir ícones do Projeto B */
#define SVG_GAP          4

/* -----------------------------------------------------------------------
 * Funções auxiliares
 * ----------------------------------------------------------------------- */

static void hex_to_rgb(const char *hex, int *r, int *g, int *b) {
    unsigned int ur = 0xAA, ug = 0xAA, ub = 0xCC;
    sscanf(hex, "%02x%02x%02x", &ur, &ug, &ub);
    *r = (int)ur; *g = (int)ug; *b = (int)ub;
}

/*
 * sort_tasks_by_id - preenche sorted[] com índices ordenados por ID crescente.
 * Requisito 2.5: menor ID fica mais próximo do eixo X (base do gráfico).
 * Ao exibir de cima para baixo, exibimos do maior ID para o menor.
 */
static void sort_tasks_by_id(Task tasks[], int task_count, int sorted[]) {
    for (int i = 0; i < task_count; i++) sorted[i] = i;
    for (int i = 0; i < task_count - 1; i++) {
        for (int j = i + 1; j < task_count; j++) {
            if (tasks[sorted[i]].id > tasks[sorted[j]].id) {
                int tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
            }
        }
    }
}

static int brightness(int r, int g, int b) {
    return (r * 299 + g * 587 + b * 114) / 1000;
}

/* -----------------------------------------------------------------------
 * Inicialização
 * ----------------------------------------------------------------------- */

void gantt_init(GanttHistory *history, int cpu_count, int task_count) {
    memset(history, 0, sizeof(*history));
    history->count      = 0;
    history->cpu_count  = cpu_count;
    history->task_count = task_count;
}

/* -----------------------------------------------------------------------
 * Registro de snapshot
 * ----------------------------------------------------------------------- */

void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used,
                  int mutex_lock_mask[], int mutex_unlock_mask[],
                  int mutex_blocked_mask[], int io_start_mask[],
                  int irq_mask[]) {

    if (history->count >= MAX_TICKS) return;

    GanttEntry *e = &history->entries[history->count];
    memset(e, 0, sizeof(*e));

    e->tick        = tick;
    e->lottery_tick = lottery_used;

    /* CPUs */
    for (int c = 0; c < cpu_count && c < MAX_CPUS; c++) {
        e->cpu_task[c]   = cpus[c].task_id;
        e->cpu_active[c] = cpus[c].active;
    }

    /* Tarefas */
    for (int i = 0; i < task_count && i < MAX_TASKS; i++) {
        e->task_state[i]     = tasks[i].state;
        e->task_remaining[i] = tasks[i].remaining_time;
        e->task_slice[i]     = tasks[i].ticks_this_slice;

        /* Chegada: estado READY com arrival_time == tick */
        e->task_arrived[i] = (tasks[i].state == READY &&
                               tasks[i].arrival_time == tick) ? 1 : 0;

        /* Fim: estado FINISHED com finish_time == tick */
        e->task_finished[i] = (tasks[i].state == FINISHED &&
                                tasks[i].finish_time == tick) ? 1 : 0;

        /* Projeto B — eventos de mutex e E/S */
        e->task_mutex_lock[i]    = (mutex_lock_mask    && mutex_lock_mask[i])    ? 1 : 0;
        e->task_mutex_unlock[i]  = (mutex_unlock_mask  && mutex_unlock_mask[i])  ? 1 : 0;
        e->task_mutex_blocked[i] = (mutex_blocked_mask && mutex_blocked_mask[i]) ? 1 : 0;
        e->task_io_start[i]      = (io_start_mask      && io_start_mask[i])      ? 1 : 0;
        e->task_irq[i]           = (irq_mask           && irq_mask[i])           ? 1 : 0;
    }

    history->count++;
}

/* -----------------------------------------------------------------------
 * Exibição no terminal
 * ----------------------------------------------------------------------- */

static void print_separator(int label_w, int ticks) {
    printf("  %*s+", label_w, "");
    for (int t = 0; t < ticks; t++) printf("----+");
    printf("\n");
}

void gantt_print_terminal(const GanttHistory *history,
                          Task tasks[], int task_count) {
    if (history->count == 0) return;

    int ticks     = history->count;
    int cpu_count = history->cpu_count;

    printf("\n\033[1m=== Grafico de Gantt ===\033[0m\n");

    /* Eixo de tempo */
    printf("  %*s ", LABEL_W, "Tempo");
    for (int t = 0; t < ticks; t++) {
        printf("\033[90m%-4d\033[0m", history->entries[t].tick);
    }
    printf("\n");
    print_separator(LABEL_W, ticks);

    int sorted[MAX_TASKS];
    sort_tasks_by_id(tasks, task_count, sorted);

    /* Linhas das tarefas — exibe do maior ID para o menor (req 2.5) */
    for (int row = task_count - 1; row >= 0; row--) {
        int tidx = sorted[row];
        int r, g, b;
        hex_to_rgb(tasks[tidx].color, &r, &g, &b);

        printf("  T%-*d |", LABEL_W - 2, tasks[tidx].id);

        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];

            /* Verifica em qual CPU a tarefa estava */
            int on_cpu = -1;
            for (int c = 0; c < cpu_count; c++) {
                if (e->cpu_task[c] == tasks[tidx].id && e->cpu_active[c]) {
                    on_cpu = c;
                    break;
                }
            }

            TaskState ts = e->task_state[tidx];

            if (on_cpu >= 0) {
                /* EXECUTANDO: bloco colorido com ID da CPU */
                printf("\033[48;2;%d;%d;%dm\033[%sm C%d \033[0m|",
                       r, g, b,
                       brightness(r,g,b) > 128 ? "30" : "97",
                       on_cpu);

            } else if (ts == SUSP_MUTEX) {
                /*
                 * SUSPENSA por mutex: fundo hachurado (diferente de E/S).
                 * No terminal, usa fundo roxo escuro + símbolo de cadeado.
                 * Req 2.9: diferenciação visual entre mutex e E/S.
                 */
                printf("\033[48;2;60;0;80m\033[35m \xF0\x9F\x94\x92 \033[0m|");

            } else if (ts == SUSP_IO) {
                /*
                 * SUSPENSA por E/S: fundo quadriculado.
                 * No terminal, usa fundo azul escuro + símbolo de disco.
                 * Req 3.8: diferenciação visual entre E/S e mutex.
                 */
                printf("\033[48;2;0;30;80m\033[34m IO  \033[0m|");

            } else if (ts == READY) {
                /* PRONTA: cinza pontilhado */
                printf("\033[48;2;55;55;75m\033[90m\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\033[0m|");

            } else if (ts == FINISHED) {
                /* FINALIZADA */
                printf("\033[48;2;15;15;15m    \033[0m|");

            } else {
                /* NEW: ainda não chegou */
                printf("    |");
            }
        }

        /* Ícones de chegada, fim e eventos ao lado direito */
        int arrived_tick  = -1;
        int finished_tick = -1;
        for (int t = 0; t < ticks; t++) {
            if (history->entries[t].task_arrived[tidx])  arrived_tick  = history->entries[t].tick;
            if (history->entries[t].task_finished[tidx]) finished_tick = history->entries[t].tick;
        }
        if (arrived_tick  >= 0) printf("  \033[1;32m\xE2\x86\x93t=%d\033[0m", arrived_tick);
        if (finished_tick >= 0) printf("  \033[1;33m\xE2\x9C\x93t=%d\033[0m", finished_tick);
        printf("\n");
    }

    print_separator(LABEL_W, ticks);

    /* Linhas de status das CPUs */
    for (int c = 0; c < cpu_count; c++) {
        printf("  CPU%-*d|", LABEL_W - 3, c);
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            if (!e->cpu_active[c]) {
                printf("\033[31m----\033[0m|");
            } else if (e->cpu_task[c] != -1) {
                printf("\033[36m T%-2d\033[0m|", e->cpu_task[c]);
            } else {
                printf("    |");
            }
        }
        printf("\n");
    }

    printf("\n");

    /* Legenda */
    printf("\033[1mLegenda:\033[0m  ");
    printf("\033[48;2;100;150;220m\033[30m Cx \033[0m Exec.(CPU x)   ");
    printf("\033[48;2;55;55;75m\033[90m\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\033[0m Pronto   ");
    printf("\033[48;2;60;0;80m\033[35m \xF0\x9F\x94\x92  \033[0m Susp.Mutex   ");
    printf("\033[48;2;0;30;80m\033[34m IO  \033[0m Susp.E/S\n");
    printf("           ");
    printf("\033[1;32m\xE2\x86\x93t=N\033[0m Chegada   ");
    printf("\033[1;33m\xE2\x9C\x93t=N\033[0m Fim   ");
    printf("\033[35m\xE2\x98\x85\033[0m Sorteio   ");
    printf("\033[31m----\033[0m CPU desligada\n\n");
}

/* -----------------------------------------------------------------------
 * Geração do arquivo SVG (req 2.4)
 * ----------------------------------------------------------------------- */

static void write_svg_defs(FILE *f) {
    fprintf(f,
        "  <defs>\n"
        "    <!-- CPU desligada: hachura diagonal -->\n"
        "    <pattern id='hatch_off' patternUnits='userSpaceOnUse' width='6' height='6'>\n"
        "      <rect width='6' height='6' fill='#333333'/>\n"
        "      <line x1='0' y1='6' x2='6' y2='0' stroke='#555555' stroke-width='1'/>\n"
        "    </pattern>\n"
        "    <!-- Tarefa pronta: pontilhado -->\n"
        "    <pattern id='dots_ready' patternUnits='userSpaceOnUse' width='4' height='4'>\n"
        "      <rect width='4' height='4' fill='#1a1a3a'/>\n"
        "      <circle cx='2' cy='2' r='0.8' fill='#4a4a6a'/>\n"
        "    </pattern>\n"
        "    <!-- Suspensao por mutex: hachura diagonal roxa (req 2.9) -->\n"
        "    <pattern id='hatch_mutex' patternUnits='userSpaceOnUse' width='6' height='6'>\n"
        "      <rect width='6' height='6' fill='#1e0030'/>\n"
        "      <line x1='0' y1='0' x2='6' y2='6' stroke='#7700bb' stroke-width='1.5'/>\n"
        "    </pattern>\n"
        "    <!-- Suspensao por E/S: quadriculado azul (req 3.8) -->\n"
        "    <pattern id='hatch_io' patternUnits='userSpaceOnUse' width='6' height='6'>\n"
        "      <rect width='6' height='6' fill='#001030'/>\n"
        "      <rect x='0' y='0' width='3' height='3' fill='#1144aa'/>\n"
        "      <rect x='3' y='3' width='3' height='3' fill='#1144aa'/>\n"
        "    </pattern>\n"
        "  </defs>\n"
    );
}

void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename) {
    if (history->count == 0) {
        fprintf(stderr, "Aviso: historico vazio, SVG nao gerado.\n");
        return;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n", filename);
        return;
    }

    int ticks     = history->count;
    int cpu_count = history->cpu_count;

    int width  = SVG_LABEL_W + ticks * SVG_TICK_W + 30;
    int height = SVG_MARGIN_TOP
               + task_count * SVG_ROW_H
               + cpu_count  * SVG_ROW_H
               + 20
               + SVG_LEGEND_H;

    fprintf(f,
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' "
        "font-family='monospace' font-size='12'>\n",
        width, height
    );

    write_svg_defs(f);

    fprintf(f, "  <rect width='%d' height='%d' fill='#1a1a2e'/>\n", width, height);

    fprintf(f,
        "  <text x='%d' y='22' fill='#e0e0f0' font-size='15' font-weight='bold' "
        "text-anchor='middle'>Grafico de Gantt - Simulador SO</text>\n",
        width / 2
    );

    /* Eixo de tempo */
    for (int t = 0; t < ticks; t++) {
        int x = SVG_LABEL_W + t * SVG_TICK_W;
        fprintf(f,
            "  <text x='%d' y='44' fill='#888899' text-anchor='middle' font-size='11'>%d</text>\n",
            x + SVG_TICK_W / 2, history->entries[t].tick
        );
        fprintf(f,
            "  <line x1='%d' y1='48' x2='%d' y2='%d' "
            "stroke='#2a2a4a' stroke-width='0.5'/>\n",
            x, x, SVG_MARGIN_TOP + (task_count + cpu_count) * SVG_ROW_H + 10
        );
    }

    int sorted[MAX_TASKS];
    sort_tasks_by_id(tasks, task_count, sorted);

    /* ---- Linhas das tarefas ---- */
    for (int row = 0; row < task_count; row++) {
        int display_row = task_count - 1 - row; /* req 2.5 */
        int tidx = sorted[row];
        int y    = SVG_MARGIN_TOP + display_row * SVG_ROW_H;
        int rc, gc, bc;
        hex_to_rgb(tasks[tidx].color, &rc, &gc, &bc);

        fprintf(f,
            "  <text x='%d' y='%d' fill='#ccccdd' text-anchor='end' font-size='12'>"
            "T%d</text>\n",
            SVG_LABEL_W - 6, y + SVG_ROW_H / 2 + 4, tasks[tidx].id
        );

        fprintf(f,
            "  <rect x='%d' y='%d' width='%d' height='%d' fill='#0e0e20' rx='2'/>\n",
            SVG_LABEL_W, y + SVG_GAP,
            ticks * SVG_TICK_W, SVG_ROW_H - SVG_GAP * 2
        );

        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            int x  = SVG_LABEL_W + t * SVG_TICK_W;
            int cw = SVG_TICK_W - 1;
            int ch = SVG_ROW_H - SVG_GAP * 2 - 2;
            int cy = y + SVG_GAP + 1;

            int on_cpu = -1;
            for (int c = 0; c < cpu_count; c++) {
                if (e->cpu_task[c] == tasks[tidx].id && e->cpu_active[c]) {
                    on_cpu = c; break;
                }
            }

            TaskState ts = e->task_state[tidx];

            if (on_cpu >= 0) {
                /* Executando: cor da tarefa */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='rgb(%d,%d,%d)' rx='2'/>\n",
                    x + 1, cy, cw, ch, rc, gc, bc
                );
                const char *tf = (brightness(rc,gc,bc) > 128) ? "#111111" : "#eeeeee";
                fprintf(f,
                    "  <text x='%d' y='%d' fill='%s' font-size='10' "
                    "text-anchor='middle' font-weight='bold'>C%d</text>\n",
                    x + cw / 2, cy + ch / 2 + 4, tf, on_cpu
                );

            } else if (ts == SUSP_MUTEX) {
                /*
                 * Suspensa por mutex: hachura diagonal roxa (req 2.9).
                 * Padrão diferente da E/S para clara distinção visual.
                 */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#hatch_mutex)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
                /* Ícone de cadeado */
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#cc44ff' font-size='9' "
                    "text-anchor='middle'>&#x1F512;</text>\n",
                    x + cw / 2, cy + ch / 2 + 3
                );

            } else if (ts == SUSP_IO) {
                /*
                 * Suspensa por E/S: padrão quadriculado azul (req 3.8).
                 * Padrão diferente do mutex para clara distinção visual.
                 */
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#hatch_io)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
                /* Ícone de E/S */
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#4488ff' font-size='8' "
                    "text-anchor='middle'>IO</text>\n",
                    x + cw / 2, cy + ch / 2 + 3
                );

            } else if (ts == READY) {
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#dots_ready)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
            } else if (ts == FINISHED) {
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='#080808' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
            }

            /* ---- Ícones de eventos (req 2.2, 2.8, 5.1) ---- */

            /* Chegada */
            if (e->task_arrived[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#00ee55' font-size='13' "
                    "text-anchor='middle'>&#x2193;</text>\n",
                    x + SVG_TICK_W / 2, cy - 1
                );
            }

            /* Fim */
            if (e->task_finished[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ffcc00' font-size='12' "
                    "text-anchor='middle'>&#x2713;</text>\n",
                    x + SVG_TICK_W / 2, cy + ch / 2 + 4
                );
            }

            /* Sorteio */
            if (e->lottery_tick && on_cpu >= 0) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ee44ff' font-size='9' "
                    "text-anchor='middle'>&#x2605;</text>\n",
                    x + cw - 3, cy + 9
                );
            }

            /* Lock de mutex (req 2.8) */
            if (e->task_mutex_lock[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ff88ff' font-size='8' "
                    "text-anchor='middle'>ML</text>\n",
                    x + cw / 2, cy + 2
                );
            }

            /* Unlock de mutex (req 2.8) */
            if (e->task_mutex_unlock[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#aaffaa' font-size='8' "
                    "text-anchor='middle'>MU</text>\n",
                    x + cw / 2, cy + 2
                );
            }

            /* Início de E/S (req 5.1) */
            if (e->task_io_start[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#44aaff' font-size='8' "
                    "text-anchor='middle'>IO&#x2193;</text>\n",
                    x + cw / 2, cy + 2
                );
            }

            /* IRQ — E/S concluída (req 3.4) */
            if (e->task_irq[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#ffaa00' font-size='8' "
                    "text-anchor='middle'>IRQ</text>\n",
                    x + cw / 2, cy + ch - 4
                );
            }
        }
    }

    /* ---- Linhas de status das CPUs ---- */
    int cpu_y_offset = SVG_MARGIN_TOP + task_count * SVG_ROW_H + 10;

    fprintf(f,
        "  <line x1='%d' y1='%d' x2='%d' y2='%d' "
        "stroke='#3a3a5a' stroke-width='1'/>\n",
        SVG_LABEL_W, cpu_y_offset - 4,
        SVG_LABEL_W + ticks * SVG_TICK_W, cpu_y_offset - 4
    );

    for (int c = 0; c < cpu_count; c++) {
        int y = cpu_y_offset + c * SVG_ROW_H;

        fprintf(f,
            "  <text x='%d' y='%d' fill='#aaaacc' text-anchor='end' font-size='11'>"
            "CPU%d</text>\n",
            SVG_LABEL_W - 6, y + SVG_ROW_H / 2 + 4, c
        );

        fprintf(f,
            "  <rect x='%d' y='%d' width='%d' height='%d' fill='#0a0a1a' rx='2'/>\n",
            SVG_LABEL_W, y + SVG_GAP,
            ticks * SVG_TICK_W, SVG_ROW_H - SVG_GAP * 2
        );

        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            int x  = SVG_LABEL_W + t * SVG_TICK_W;
            int cw = SVG_TICK_W - 1;
            int ch = SVG_ROW_H - SVG_GAP * 2 - 2;
            int cy = y + SVG_GAP + 1;

            if (!e->cpu_active[c]) {
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#hatch_off)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#cc3333' font-size='9' "
                    "text-anchor='middle'>OFF</text>\n",
                    x + cw / 2, cy + ch / 2 + 3
                );
            } else if (e->cpu_task[c] != -1) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#55bbee' font-size='10' "
                    "text-anchor='middle'>T%d</text>\n",
                    x + cw / 2, cy + ch / 2 + 3, e->cpu_task[c]
                );
            }
        }
    }

    /* ---- Legenda expandida (Projeto B) ---- */
    int ly = cpu_y_offset + cpu_count * SVG_ROW_H + 28;

    fprintf(f, "  <text x='12' y='%d' fill='#ffffff' font-size='13' "
               "font-weight='bold'>Legenda:</text>\n", ly);
    ly += 20;

    /* Executando */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='#3399ff' rx='2'/>\n"
        "  <text x='16' y='%d' fill='#ffffff' font-size='10' font-weight='bold' "
        "dominant-baseline='middle'>Cx</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Executando na CPU x (cor propria da tarefa)</text>\n",
        ly, ly + 7, ly + 7
    );
    ly += 20;

    /* Pronta */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='url(#dots_ready)' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Pronta (fila de prontos, aguardando CPU)</text>\n",
        ly, ly + 7
    );
    ly += 20;

    /* Suspensa por mutex */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='url(#hatch_mutex)' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Suspensa por mutex indisponivel (req 2.9)</text>\n",
        ly, ly + 7
    );
    ly += 20;

    /* Suspensa por E/S */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='url(#hatch_io)' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Suspensa por operacao de E/S (req 3.8)</text>\n",
        ly, ly + 7
    );
    ly += 20;

    /* Chegada */
    fprintf(f,
        "  <text x='16' y='%d' fill='#00ee55' font-size='14'>&#x2193;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Chegada da tarefa ao sistema</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* Fim */
    fprintf(f,
        "  <text x='16' y='%d' fill='#ffcc00' font-size='13'>&#x2713;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Tarefa concluida</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* Sorteio */
    fprintf(f,
        "  <text x='16' y='%d' fill='#ee44ff' font-size='10'>&#x2605;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Desempate por sorteio</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* ML */
    fprintf(f,
        "  <text x='16' y='%d' fill='#ff88ff' font-size='10' font-weight='bold'>ML</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Solicitacao de mutex (lock) — req 2.8</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* MU */
    fprintf(f,
        "  <text x='16' y='%d' fill='#aaffaa' font-size='10' font-weight='bold'>MU</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Liberacao de mutex (unlock) — req 2.8</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* IO */
    fprintf(f,
        "  <text x='16' y='%d' fill='#44aaff' font-size='10' font-weight='bold'>IO&#x2193;</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "Inicio de operacao de E/S — req 5.1</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* IRQ */
    fprintf(f,
        "  <text x='16' y='%d' fill='#ffaa00' font-size='10' font-weight='bold'>IRQ</text>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "E/S concluida (IRQ gerada) — req 3.4</text>\n",
        ly + 10, ly + 7
    );
    ly += 20;

    /* CPU desligada */
    fprintf(f,
        "  <rect x='12' y='%d' width='24' height='14' fill='url(#hatch_off)' rx='2'/>\n"
        "  <text x='42' y='%d' fill='#ccccdd' font-size='12'>"
        "CPU desligada (sem tarefa disponivel)</text>\n",
        ly, ly + 7
    );

    fprintf(f, "</svg>\n");
    fclose(f);

    printf("Grafico de Gantt SVG salvo em '%s'.\n", filename);
}
