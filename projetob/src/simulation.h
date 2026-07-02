#ifndef SIMULATION_H
#define SIMULATION_H

#include "task.h"
#include "config.h"
#include "scheduler.h"
#include "gantt.h"
#include "mutex.h"
#include "io.h"

/*
 * SimulationState:
 * estrutura que reúne todo o estado mutável da simulação.
 *
 * Em vez de espalhar variáveis globais pelo programa, concentramos aqui:
 * - as tarefas;
 * - as CPUs;
 * - a configuração geral;
 * - o relógio da simulação;
 * - o histórico usado para o gráfico de Gantt;
 * - a gestão de mutexes;
 * - a fila de operações de E/S;
 * - o contador usado para detectar travamento.
 */
typedef struct {
    Task         tasks[MAX_TASKS];   /* Vetor com todas as tarefas da simulação. */
    int          task_count;         /* Quantidade real de tarefas carregadas. */

    CPU          cpus[MAX_CPUS];     /* Estado atual de cada CPU. */
    Config       config;             /* Parâmetros globais da execução. */

    int          clock;              /* Relógio lógico da simulação, em ticks. */
    GanttHistory history;            /* Histórico de execução para visualização. */

    /*
     * Estruturas auxiliares para sincronização e E/S.
     * Elas armazenam informações sobre bloqueios, desbloqueios
     * e operações assíncronas em andamento.
     */
    MutexTable   mutex_table;        /* Tabela de mutexes usados pelas tarefas. */
    IOQueue      io_queue;            /* Fila de operações de entrada/saída. */

    /*
     * Contador de ticks consecutivos sem progresso.
     * Serve para identificar possível deadlock:
     * se nada executa por tempo demais, a simulação é encerrada.
     */
    int          stall_ticks;
} SimulationState;

/*
 * Funções utilitárias para leitura de entrada.
 * São usadas na interface interativa da simulação.
 */
void limpar_buffer(void);
int  ler_inteiro_com_padrao(const char *mensagem, int valor_padrao);

/*
 * Inicializa toda a estrutura da simulação:
 * copia a configuração, carrega as tarefas e prepara os subsistemas.
 */
void simulation_init(SimulationState *sim, Config *config,
                     Task tasks[], int task_count);

/*
 * Libera a memória dinâmica associada aos eventos das tarefas.
 * Deve ser chamada ao final da execução para evitar vazamentos.
 */
void simulation_free_events(SimulationState *sim);

/*
 * Executa apenas um tick da simulação.
 * Retorna diferente de zero enquanto ainda houver tarefas pendentes.
 */
int  simulation_step(SimulationState *sim);

/*
 * Executa a simulação inteira até o término automático.
 */
void simulation_run_complete(SimulationState *sim);

/*
 * Executa a simulação em modo interativo, permitindo avançar,
 * voltar, inspecionar e modificar o estado.
 */
void simulation_run_step_by_step(SimulationState *sim);

#endif /* SIMULATION_H */
