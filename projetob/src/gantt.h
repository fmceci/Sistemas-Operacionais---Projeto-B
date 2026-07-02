#ifndef GANTT_H
#define GANTT_H

#include "task.h"
#include "scheduler.h"
#include "config.h"

/* * O "rolo de filme" da nossa câmera tem um limite. 
 * O MAX_TICKS define o limite máximo de tempo (iterações) que podemos gravar.
 * 65536 é um limite de segurança para a memória do programa não estourar.
 */
#define MAX_TICKS 65536

/* =======================================================================
 * ESTRUTURA DE DADOS: O "Snapshot" (A Fotografia)
 * ======================================================================= */

/*
 * GanttEntry - Representa o estado exato de TODO o sistema em 1 único "tick" (instante).
 * * Imagine que o tempo parou. Esta estrutura anota exatamente quem está rodando,
 * quem está esperando e o que acabou de acontecer neste milissegundo.
 * É graças a esta "fotografia" que o sistema consegue gerar gráficos ou 
 * permitir que o usuário avance/retroceda no tempo (req 1.5.2).
 */
typedef struct {
    int       tick;                        /* Qual é o "número" desta foto? (Ex: instante t=15) */
    
    /* --- Informações das CPUs --- */
    int       cpu_task[MAX_CPUS];          /* Qual o ID da tarefa rodando em cada CPU agora? (-1 se estiver vazia/ociosa) */
    int       cpu_active[MAX_CPUS];        /* A CPU de número 'x' está ligada neste instante? (1 = Sim, 0 = Não) */
    
    /* --- Informações das Tarefas --- */
    TaskState task_state[MAX_TASKS];       /* A tarefa está PRONTA, EXECUTANDO, ou BLOQUEADA neste tick? */
    int       task_remaining[MAX_TASKS];   /* Quanto tempo falta para a tarefa terminar? */
    int       task_slice[MAX_TASKS];       /* Fatias de tempo (quantum) já usadas pela tarefa (útil para Round-Robin) */
    
    /* --- Eventos Marcantes (O que aconteceu AGORA?) --- */
    int       lottery_tick;                /* 1 se o escalonador precisou fazer um sorteio neste tick para desempatar */
    int       task_arrived[MAX_TASKS];     /* 1 se a tarefa ACABOU de nascer/chegar (NEW -> READY) neste exato tick */
    int       task_finished[MAX_TASKS];    /* 1 se a tarefa ACABOU de ser finalizada neste exato tick */

    /* * [Projeto B] - Eventos de Sincronização (Mutex)
     * Estes vetores guardam 1 ou 0 para indicar se a tarefa fez alguma destas ações.
     * Isso será usado para desenhar cadeados (lock) e destrancas (unlock) no gráfico visual.
     */
    int       task_mutex_lock[MAX_TASKS];   /* 1 se a tarefa conseguiu trancar o mutex agora */
    int       task_mutex_unlock[MAX_TASKS]; /* 1 se a tarefa destrancou/liberou o mutex agora */
    int       task_mutex_blocked[MAX_TASKS];/* 1 se a tarefa tentou trancar, mas já estava em uso, e foi bloqueada */

    /* * [Projeto B] - Eventos de Entrada e Saída (E/S - I/O)
     * Estes vetores guardam 1 ou 0 para desenharmos os ícones de disco/leitura no gráfico.
     */
    int       task_io_start[MAX_TASKS];     /* 1 se a tarefa parou de usar a CPU para iniciar um pedido de E/S (ex: ler pendrive) */
    int       task_irq[MAX_TASKS];          /* 1 se o dispositivo de E/S avisou o sistema que terminou (Interrupção/IRQ gerada) */
} GanttEntry;


/* =======================================================================
 * ESTRUTURA DE DADOS: O "Álbum de Fotos" (O Histórico)
 * ======================================================================= */

/*
 * GanttHistory - Guarda o filme inteiro da simulação.
 * É este "álbum" que entregamos para a função que desenha o gráfico no final.
 */
typedef struct {
    GanttEntry entries[MAX_TICKS]; /* O vetor (array) gigante guardando cada "fotografia" do sistema */
    int        count;              /* Quantas "fotos" tiramos até agora? (Avança de 1 em 1 a cada tick) */
    int        cpu_count;          /* Configuração do sistema: Quantas CPUs existem no total */
    int        task_count;         /* Configuração do sistema: Quantas tarefas existem no total */
} GanttHistory;


/* =======================================================================
 * FUNÇÕES: O "Controle" da nossa Câmera e Revelação das Fotos
 * ======================================================================= */

/*
 * gantt_init: 
 * Liga a câmera e coloca um filme vazio. Prepara o histórico zerando tudo,
 * anotando apenas o número de CPUs e tarefas que vão participar da simulação.
 */
void gantt_init(GanttHistory *history, int cpu_count, int task_count);

/*
 * gantt_record:
 * O ato de "tirar a foto". Chamamos essa função a cada tick do relógio do sistema.
 * Ela olha para o estado atual (cpus, tasks) e salva isso em uma nova posição no 'history'.
 * * Parâmetros explicados:
 * - history a cpu_count: Dados básicos de onde salvar e quem estava lá.
 * - lottery_used: Informa se o escalonador apelou pra sorte neste tick.
 * - Os arrays "mask" (máscaras): São listas de "Verdadeiro/Falso" (1 ou 0).
 * Exemplo: Se a tarefa 3 pediu E/S agora, o io_start_mask[3] será 1.
 * Dica: Se não houve eventos de um tipo em um tick, o código pode mandar NULL (vazio).
 */
void gantt_record(GanttHistory *history, int tick,
                  CPU cpus[], int cpu_count,
                  Task tasks[], int task_count,
                  int lottery_used,
                  int mutex_lock_mask[], int mutex_unlock_mask[],
                  int mutex_blocked_mask[], int io_start_mask[],
                  int irq_mask[]);

/*
 * gantt_print_terminal:
 * Imprime o "álbum de fotos" (histórico) na tela do terminal escuro (Prompt/Console).
 * Usa códigos de cor do terminal (ANSI) para desenhar o Gráfico de Gantt em formato texto.
 */
void gantt_print_terminal(const GanttHistory *history,
                          Task tasks[], int task_count);

/*
 * gantt_save_svg:
 * Exporta o "álbum de fotos" para uma imagem profissional real (formato vetorial .svg).
 * Você pode abrir o arquivo gerado (ex: "grafico.svg") no Chrome/Edge/Firefox e ver
 * o Gráfico de Gantt bonitinho, com cores sólidas, hachuras e ícones.
 */
void gantt_save_svg(const GanttHistory *history, Task tasks[],
                    int task_count, const char *filename);

#endif /* GANTT_H */
