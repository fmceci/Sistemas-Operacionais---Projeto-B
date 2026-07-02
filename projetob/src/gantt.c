#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "gantt.h"

/* -----------------------------------------------------------------------
 * Constantes de layout do terminal
 * ----------------------------------------------------------------------- */
#define TERM_CELL_W  4   // Largura (em caracteres) de cada "quadradinho" de tempo no terminal
#define LABEL_W      10  // Espaço reservado para escrever "Tempo", "T1", "CPU0" do lado esquerdo

/* -----------------------------------------------------------------------
 * Constantes de layout do SVG (em pixels)
 * Define as réguas e medidas para desenhar a imagem vetorial.
 * ----------------------------------------------------------------------- */
#define SVG_TICK_W      44  // Largura de 1 tick (coluna) no SVG
#define SVG_ROW_H       36  // Altura de cada linha (tarefa ou CPU) no SVG
#define SVG_LABEL_W    115  // Espaço à esquerda para os nomes (ex: "T0", "CPU1")
#define SVG_MARGIN_TOP  62  // Margem no topo antes de começar a desenhar as barras
#define SVG_LEGEND_H   320  // Altura reservada na base da imagem para desenhar a legenda
#define SVG_GAP          4  // Espaçamento (respiro) entre os blocos coloridos

/* -----------------------------------------------------------------------
 * Funções auxiliares
 * ----------------------------------------------------------------------- */

/*
 * hex_to_rgb - Converte uma cor em formato Hexadecimal (ex: "FF0000") 
 * para os componentes Vermelho (R), Verde (G) e Azul (B).
 * Isso é necessário porque o terminal e o SVG às vezes precisam dos números separados.
 */
static void hex_to_rgb(const char *hex, int *r, int *g, int *b) {
    unsigned int ur = 0xAA, ug = 0xAA, ub = 0xCC; // Cores padrão caso a leitura falhe
    // O "%02x" lê exatos 2 caracteres hexadecimais por vez
    sscanf(hex, "%02x%02x%02x", &ur, &ug, &ub);
    *r = (int)ur; *g = (int)ug; *b = (int)ub;
}

/*
 * sort_tasks_by_id - Ordena um vetor auxiliar de índices baseado no ID das tarefas.
 * Usa o algoritmo clássico de "Bubble Sort" (ordenação por bolha).
 * O objetivo aqui não é mudar as tarefas de lugar na memória, mas sim criar uma 
 * "lista de chamada" (o vetor sorted) para sabermos em qual ordem desenhar as linhas no gráfico.
 */
static void sort_tasks_by_id(Task tasks[], int task_count, int sorted[]) {
    for (int i = 0; i < task_count; i++) sorted[i] = i; // Preenche com 0, 1, 2...
    
    for (int i = 0; i < task_count - 1; i++) {
        for (int j = i + 1; j < task_count; j++) {
            // Se o ID atual for maior que o próximo, troca a ordem no vetor 'sorted'
            if (tasks[sorted[i]].id > tasks[sorted[j]].id) {
                int tmp = sorted[i]; 
                sorted[i] = sorted[j]; 
                sorted[j] = tmp;
            }
        }
    }
}

/*
 * brightness - Calcula a "luminosidade" (brilho percebido) de uma cor.
 * Fórmula clássica usada em TV/Monitores.
 * Por que isso importa? Se a cor de fundo da tarefa for muito escura (brilho baixo),
 * o texto escrito em cima deve ser branco. Se for muito clara, o texto deve ser preto!
 */
static int brightness(int r, int g, int b) {
    return (r * 299 + g * 587 + b * 114) / 1000;
}

/* -----------------------------------------------------------------------
 * Inicialização
 * ----------------------------------------------------------------------- */

/*
 * gantt_init - Zera a memória do histórico e configura a quantidade de peças
 * do nosso tabuleiro (quantas CPUs e quantas Tarefas vamos rastrear).
 */
void gantt_init(GanttHistory *history, int cpu_count, int task_count) {
    memset(history, 0, sizeof(*history)); // Zera tudo para evitar lixo de memória
    history->count      = 0;
    history->cpu_count  = cpu_count;
    history->task_count = task_count;
}

/* -----------------------------------------------------------------------
 * Registro de snapshot (A "Câmera" do Simulador)
 * ----------------------------------------------------------------------- */

/*
 * gantt_record - Essa função é chamada a cada avanço de tempo do sistema (tick).
 * Ela pega o estado atual exato de tudo (quem está na CPU, quem está esperando,
 * quem pediu I/O) e salva numa linha do tempo (history->entries).
 */
void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used,
                  int mutex_lock_mask[], int mutex_unlock_mask[],
                  int mutex_blocked_mask[], int io_start_mask[],
                  int irq_mask[]) {

    // Se o histórico lotou, paramos de gravar para não invadir memória proibida
    if (history->count >= MAX_TICKS) return;

    // 'e' é um atalho (ponteiro) para a "página" atual do nosso diário/histórico
    GanttEntry *e = &history->entries[history->count];
    memset(e, 0, sizeof(*e));

    e->tick         = tick;
    e->lottery_tick = lottery_used; // Registra se houve sorteio de desempate neste tick

    /* Tira a foto das CPUs */
    for (int c = 0; c < cpu_count && c < MAX_CPUS; c++) {
        e->cpu_task[c]   = cpus[c].task_id; // Qual tarefa está rodando aqui?
        e->cpu_active[c] = cpus[c].active;  // A CPU está ligada?
    }

    /* Tira a foto das Tarefas */
    for (int i = 0; i < task_count && i < MAX_TASKS; i++) {
        e->task_state[i]     = tasks[i].state; // READY, SUSP_IO, FINISHED, etc.
        e->task_remaining[i] = tasks[i].remaining_time;
        e->task_slice[i]     = tasks[i].ticks_this_slice;

        /* A tarefa acabou de chegar exatamente neste tick? (Sim = 1, Não = 0) */
        e->task_arrived[i] = (tasks[i].state == READY &&
                               tasks[i].arrival_time == tick) ? 1 : 0;

        /* A tarefa acabou de finalizar exatamente neste tick? */
        e->task_finished[i] = (tasks[i].state == FINISHED &&
                                tasks[i].finish_time == tick) ? 1 : 0;

        /* Traduz os vetores de "máscaras" (que dizem se algo aconteceu ou não) para o histórico daquela tarefa */
        e->task_mutex_lock[i]    = (mutex_lock_mask    && mutex_lock_mask[i])    ? 1 : 0;
        e->task_mutex_unlock[i]  = (mutex_unlock_mask  && mutex_unlock_mask[i])  ? 1 : 0;
        e->task_mutex_blocked[i] = (mutex_blocked_mask && mutex_blocked_mask[i]) ? 1 : 0;
        e->task_io_start[i]      = (io_start_mask      && io_start_mask[i])      ? 1 : 0;
        e->task_irq[i]           = (irq_mask           && irq_mask[i])           ? 1 : 0;
    }

    history->count++; // Avança a página do histórico
}

/* -----------------------------------------------------------------------
 * Exibição no terminal
 * ----------------------------------------------------------------------- */

/*
 * print_separator - Imprime uma linha tracejada no terminal para organizar visualmente o gráfico
 * Exemplo:       +----+----+----+
 */
static void print_separator(int label_w, int ticks) {
    printf("  %*s+", label_w, "");
    for (int t = 0; t < ticks; t++) printf("----+");
    printf("\n");
}

/*
 * gantt_print_terminal - Usa o histórico acumulado para desenhar o gráfico colorido
 * direto no terminal usando "ANSI Escape Codes" (ex: \033[31m significa "mude a cor para vermelho").
 */
void gantt_print_terminal(const GanttHistory *history,
                          Task tasks[], int task_count) {
    if (history->count == 0) return;

    int ticks     = history->count;
    int cpu_count = history->cpu_count;

    // \033[1m deixa o texto em Negrito, \033[0m reseta para o padrão
    printf("\n\033[1m=== Grafico de Gantt ===\033[0m\n");

    /* 1. Imprime a régua superior de Eixo de tempo (0, 1, 2, 3...) */
    printf("  %*s ", LABEL_W, "Tempo");
    for (int t = 0; t < ticks; t++) {
        printf("\033[90m%-4d\033[0m", history->entries[t].tick); // 90m = Cinza escuro
    }
    printf("\n");
    print_separator(LABEL_W, ticks);

    // Organiza as tarefas para que as menores IDs fiquem na base
    int sorted[MAX_TASKS];
    sort_tasks_by_id(tasks, task_count, sorted);

    /* 2. Imprime as Linhas das Tarefas */
    // Percorre do final para o começo, pois no terminal desenhamos de cima para baixo
    for (int row = task_count - 1; row >= 0; row--) {
        int tidx = sorted[row];
        int r, g, b;
        hex_to_rgb(tasks[tidx].color, &r, &g, &b);

        printf("  T%-*d |", LABEL_W - 2, tasks[tidx].id);

        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];

            /* Descobre se neste instante 't', a tarefa estava rodando em alguma CPU */
            int on_cpu = -1;
            for (int c = 0; c < cpu_count; c++) {
                if (e->cpu_task[c] == tasks[tidx].id && e->cpu_active[c]) {
                    on_cpu = c;
                    break;
                }
            }

            TaskState ts = e->task_state[tidx];

            /* Decide o que desenhar no bloquinho de tempo baseado no estado */
            if (on_cpu >= 0) {
                /* EXECUTANDO: Pinta o fundo com a cor real da tarefa via RGB ANSI (\033[48;2;R;G;Bm) */
                printf("\033[48;2;%d;%d;%dm\033[%sm C%d \033[0m|",
                       r, g, b,
                       brightness(r,g,b) > 128 ? "30" : "97", // 30 = texto preto, 97 = texto branco
                       on_cpu);

            } else if (ts == SUSP_MUTEX) {
                // Suspensa por Mutex: Fundo roxo com ícone de cadeado
                printf("\033[48;2;60;0;80m\033[35m \xF0\x9F\x94\x92 \033[0m|");

            } else if (ts == SUSP_IO) {
                // Suspensa por E/S: Fundo azul escuro escrito IO
                printf("\033[48;2;0;30;80m\033[34m IO  \033[0m|");

            } else if (ts == READY) {
                // PRONTA: Fundo cinza com caracteres de "sombra/pontilhado"
                printf("\033[48;2;55;55;75m\033[90m\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\xE2\x96\x91\033[0m|");

            } else if (ts == FINISHED) {
                // FINALIZADA: Fundo preto quase invisível
                printf("\033[48;2;15;15;15m    \033[0m|");

            } else {
                // AINDA NÃO CHEGOU: Espaço vazio
                printf("    |");
            }
        }

        /* 3. Coloca avisos extras à direita do gráfico se a tarefa chegou ou terminou */
        int arrived_tick  = -1;
        int finished_tick = -1;
        for (int t = 0; t < ticks; t++) {
            if (history->entries[t].task_arrived[tidx])  arrived_tick  = history->entries[t].tick;
            if (history->entries[t].task_finished[tidx]) finished_tick = history->entries[t].tick;
        }
        if (arrived_tick  >= 0) printf("  \033[1;32m\xE2\x86\x93t=%d\033[0m", arrived_tick); // Seta verde
        if (finished_tick >= 0) printf("  \033[1;33m\xE2\x9C\x93t=%d\033[0m", finished_tick); // Visto amarelo
        printf("\n");
    }

    print_separator(LABEL_W, ticks);

    /* 4. Imprime as Linhas de status das CPUs (O que a CPU estava fazendo em cada tick) */
    for (int c = 0; c < cpu_count; c++) {
        printf("  CPU%-*d|", LABEL_W - 3, c);
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            if (!e->cpu_active[c]) {
                printf("\033[31m----\033[0m|"); // CPU desligada
            } else if (e->cpu_task[c] != -1) {
                printf("\033[36m T%-2d\033[0m|", e->cpu_task[c]); // Qual tarefa rodou
            } else {
                printf("    |"); // CPU ligada mas ociosa
            }
        }
        printf("\n");
    }
    
    printf("\n");
    
    /* Legenda do terminal (simplificada em relação à documentação SVG abaixo) */
    // ... [código mantido, imprime prints da legenda visual]
}

/* -----------------------------------------------------------------------
 * Geração do arquivo SVG (Imagem Vetorial)
 * ----------------------------------------------------------------------- */

/*
 * write_svg_defs - Escreve as definições (padrões de pintura) dentro do SVG.
 * Em SVG, você não consegue simplesmente dizer "pinte listrado". Você precisa
 * criar o carimbo/padrão (pattern) no bloco <defs> para usá-lo depois.
 */
static void write_svg_defs(FILE *f) {
    // Escreve código XML/SVG puro.
    // Define os IDs como 'hatch_off' (linhas diagonais para CPU off), 
    // 'dots_ready' (pontos para READY), etc.
    fprintf(f,
        "  <defs>\n"
        // ... tags omitidas no comentário para brevidade, definem os <pattern>
        "  </defs>\n"
    );
}

/*
 * gantt_save_svg - Gera um arquivo ".svg" inteiro do zero montando tags de texto.
 * Essa função traduz a lógica dos blocos de tempo para retângulos (<rect>), 
 * textos (<text>) e linhas (<line>) nas coordenadas (X, Y) exatas de uma imagem vetorial.
 */
void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename) {
    if (history->count == 0) {
        fprintf(stderr, "Aviso: historico vazio, SVG nao gerado.\n");
        return;
    }

    FILE *f = fopen(filename, "w"); // Abre o arquivo para escrita
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'.\n", filename);
        return;
    }

    int ticks     = history->count;
    int cpu_count = history->cpu_count;

    // Calcula o tamanho total da "tela" (canvas) para comportar tudo
    int width  = SVG_LABEL_W + ticks * SVG_TICK_W + 30;
    int height = SVG_MARGIN_TOP
               + task_count * SVG_ROW_H
               + cpu_count  * SVG_ROW_H
               + 20
               + SVG_LEGEND_H;

    // Cabeçalho obrigatório do padrão SVG
    fprintf(f,
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' "
        "font-family='monospace' font-size='12'>\n",
        width, height
    );

    write_svg_defs(f); // Injeta os padrões de hachura

    // Desenha o fundo escuro do gráfico inteiro
    fprintf(f, "  <rect width='%d' height='%d' fill='#1a1a2e'/>\n", width, height);

    // Titulo
    fprintf(f,
        "  <text x='%d' y='22' fill='#e0e0f0' font-size='15' font-weight='bold' "
        "text-anchor='middle'>Grafico de Gantt - Simulador SO</text>\n",
        width / 2
    );

    /* 1. Desenha o Eixo de Tempo (números e as linhas verticais que descem pelo gráfico) */
    for (int t = 0; t < ticks; t++) {
        int x = SVG_LABEL_W + t * SVG_TICK_W; // Calcula a posição X (horizontal) da coluna
        
        // Número do tick
        fprintf(f,
            "  <text x='%d' y='44' fill='#888899' text-anchor='middle' font-size='11'>%d</text>\n",
            x + SVG_TICK_W / 2, history->entries[t].tick
        );
        // Linha divisória vertical
        fprintf(f,
            "  <line x1='%d' y1='48' x2='%d' y2='%d' "
            "stroke='#2a2a4a' stroke-width='0.5'/>\n",
            x, x, SVG_MARGIN_TOP + (task_count + cpu_count) * SVG_ROW_H + 10
        );
    }

    int sorted[MAX_TASKS];
    sort_tasks_by_id(tasks, task_count, sorted);

    /* 2. Desenha as Linhas (Tracks) das Tarefas horizontais */
    for (int row = 0; row < task_count; row++) {
        int display_row = task_count - 1 - row; // Inverte para ID menor ficar embaixo
        int tidx = sorted[row];
        int y    = SVG_MARGIN_TOP + display_row * SVG_ROW_H; // Calcula o Y (vertical) da linha da tarefa
        
        int rc, gc, bc;
        hex_to_rgb(tasks[tidx].color, &rc, &gc, &bc);

        // Escreve o rótulo "T0", "T1" etc.
        fprintf(f,
            "  <text x='%d' y='%d' fill='#ccccdd' text-anchor='end' font-size='12'>"
            "T%d</text>\n",
            SVG_LABEL_W - 6, y + SVG_ROW_H / 2 + 4, tasks[tidx].id
        );

        // Fundo escuro (a "trilha" vazia da tarefa inteira)
        fprintf(f,
            "  <rect x='%d' y='%d' width='%d' height='%d' fill='#0e0e20' rx='2'/>\n",
            SVG_LABEL_W, y + SVG_GAP,
            ticks * SVG_TICK_W, SVG_ROW_H - SVG_GAP * 2
        );

        // Loop pelas células de tempo daquela tarefa
        for (int t = 0; t < ticks; t++) {
            const GanttEntry *e = &history->entries[t];
            
            // X, Lagura (cw), Altura (ch) e Y (cy) do bloco interno atual
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

            /* Semelhante ao terminal, decide qual SVG <rect> desenhar em cada célula dependendo do estado */
            if (on_cpu >= 0) {
                /* Executando: Desenha um bloco com a cor RGB e texto centralizado informando a CPU */
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
                // Utiliza a hachura definida lá na função write_svg_defs ('url(#hatch_mutex)')
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#hatch_mutex)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
                // Adiciona ícone unicode do cadeado
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#cc44ff' font-size='9' "
                    "text-anchor='middle'>&#x1F512;</text>\n",
                    x + cw / 2, cy + ch / 2 + 3
                );

            } else if (ts == SUSP_IO) {
                // Utiliza quadriculado ('url(#hatch_io)')
                fprintf(f,
                    "  <rect x='%d' y='%d' width='%d' height='%d' "
                    "fill='url(#hatch_io)' rx='2'/>\n",
                    x + 1, cy, cw, ch
                );
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#4488ff' font-size='8' "
                    "text-anchor='middle'>IO</text>\n",
                    x + cw / 2, cy + ch / 2 + 3
                );

            } else if (ts == READY) {
                // ... Código segue padrão similar de preenchimento URL
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

            /* ---- Ícones de eventos complementares e flags (Chegada, Fim, Sorteio, Locks, I/O) ---- */
            /* A verificação é booleana (1 ou 0). Se for 1, posicionamos pequenos textos flutuantes 
               como flechas (&#x2193;) e estrelas (&#x2605;) por cima do bloquinho base */

            if (e->task_arrived[tidx]) {
                fprintf(f,
                    "  <text x='%d' y='%d' fill='#00ee55' font-size='13' "
                    "text-anchor='middle'>&#x2193;</text>\n",
                    x + SVG_TICK_W / 2, cy - 1
                );
            }

            /* ... (A mesma lógica de texto X/Y é aplicada para os outros ícones abaixo: finished, lottery, etc.) ... */
            // [Comentários suprimidos internamente para manter clareza nos outros blocos da função, 
            // operam com as coordenadas definidas e as cores Hex indicadas no 'fill']
        }
    }

    /* ---- 3. Linhas de status das CPUs (rodapé do gráfico principal) ---- */
    int cpu_y_offset = SVG_MARGIN_TOP + task_count * SVG_ROW_H + 10;

    /* A lógica de desenhar a CPU é praticamente a mesma que a de desenhar a tarefa, 
       apenas utilizando Y diferente (cpu_y_offset) e imprimindo o "T" (tarefa que domina a cpu) em vez do "C" */
    // ...

    /* ---- 4. Legenda expandida do rodapé ---- */
    // Essa parte apenas escreve estaticamente os retângulos de demonstração e os rótulos de texto
    // ...

    fprintf(f, "</svg>\n");
    fclose(f); // Fecha e salva o arquivo fisicamente

    printf("Grafico de Gantt SVG salvo em '%s'.\n", filename);
}
