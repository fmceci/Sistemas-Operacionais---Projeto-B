#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "task.h"
#include "config.h"
#include "simulation.h"
#include "scheduler.h"
#include "gantt.h"

/*
 * main - ponto de entrada do simulador de SO multitarefa.
 *
 * Uso:
 *   ./simulador [arquivo_config] [modo]
 *
 *   arquivo_config : caminho do arquivo de configuração (padrão: entrada.txt)
 *   modo           : "completo" ou "passo" (se omitido, exibe menu interativo)
 *
 * Projeto B: adicionada opção 3 (PRIOPEnv) no menu de algoritmos.
 */
int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif



    srand((unsigned int)time(NULL));

    const char *config_file = "entrada.txt";
    if (argc >= 2) config_file = argv[1];

    Config config;
    Task   tasks[MAX_TASKS];
    int    task_count = 0;

    if (!load_config(config_file, &config, tasks, &task_count)) {
        fprintf(stderr, "Falha ao carregar configuracao. Encerrando.\n");
        return 1;
    }

    printf("\n========================================\n");
    printf("  Simulador de SO Multitarefa - v0.5B\n");
    printf("========================================\n\n");

    printf("Configuracoes carregadas de '%s':\n", config_file);
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n", config.cpu_count);
    if (config.alpha > 0)
        printf("  Alpha     : %d (envelhecimento PRIOPEnv)\n\n", config.alpha);
    else
        printf("\n");

    /* Menu de seleção de algoritmo */
    printf("Selecione o algoritmo (ENTER para manter %s):\n", config.algorithm);
    printf("  1 - SRTF     (Shortest Remaining Time First)\n");
    printf("  2 - PRIOP    (Prioridade Preemptivo)\n");
    printf("  3 - PRIOPEnv (Prioridade Preemptivo com Envelhecimento) [Projeto B]\n");
    printf("Opcao: ");

    char buffer[32];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '\n') {
        int opcao = atoi(buffer);
        switch (opcao) {
            case 1: strcpy(config.algorithm, "SRTF");     break;
            case 2: strcpy(config.algorithm, "PRIOP");    break;
            case 3: strcpy(config.algorithm, "PRIOPENV"); break;
            default:
                printf("Opcao invalida. Mantendo: %s\n", config.algorithm);
        }
    }

    /* Se o usuário escolheu PRIOPEnv e o alpha ainda é 0, pede o valor */
    if (strcmp(config.algorithm, "PRIOPENV") == 0 && config.alpha == 0) {
        config.alpha = ler_inteiro_com_padrao(
            "Alpha para envelhecimento (ENTER para manter %d)", 1);
        if (config.alpha < 0) config.alpha = 0;
    }

    config.quantum = ler_inteiro_com_padrao(
        "Quantum (ENTER para manter %d)", config.quantum);
    if (config.quantum < 1) config.quantum = 1;

    config.cpu_count = ler_inteiro_com_padrao(
        "Numero de CPUs (ENTER para manter %d)", config.cpu_count);
    if (config.cpu_count < 2) {
        printf("Minimo de 2 CPUs. Ajustando para 2.\n");
        config.cpu_count = 2;
    }
    if (config.cpu_count > MAX_CPUS) {
        printf("Maximo de %d CPUs. Ajustando.\n", MAX_CPUS);
        config.cpu_count = MAX_CPUS;
    }

    printf("\n--- Configuracoes finais ---\n");
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n", config.cpu_count);
    if (strcmp(config.algorithm, "PRIOPENV") == 0)
        printf("  Alpha     : %d\n", config.alpha);
    printf("\n");

    /* Exibe tarefas carregadas */
    printf("Tarefas carregadas (%d):\n", task_count);
    for (int i = 0; i < task_count; i++) {
        printf("  T%-3d | Cor: #%s | Ingresso: %2d | Duracao: %2d | Prio: %2d | Eventos: %d\n",
               tasks[i].id,
               tasks[i].color,
               tasks[i].arrival_time,
               tasks[i].duration,
               tasks[i].priority,
               tasks[i].event_list.count);
    }
    printf("\n");

    SimulationState *sim = malloc(sizeof(SimulationState));
    if (!sim) {
        fprintf(stderr, "Erro: sem memoria.\n"); return 1;
    }
    simulation_init(sim, &config, tasks, task_count);
    int mode = 0;

    if (argc >= 3) {
        if (strcmp(argv[2], "completo") == 0) mode = 1;
        else if (strcmp(argv[2], "passo") == 0) mode = 2;
    }

    if (mode == 0) {
        printf("Selecione o modo de execucao:\n");
        printf("  1 - Completo     (executa tudo de uma vez)\n");
        printf("  2 - Passo-a-passo (interativo, com Gantt ao vivo)\n");
        printf("Opcao: ");

        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            mode = atoi(buffer);
        }
        if (mode != 1 && mode != 2) {
            printf("Opcao invalida. Usando modo completo.\n");
            mode = 1;
        }
        printf("\n");
    }

    if (mode == 2) simulation_run_step_by_step(sim);
    else           simulation_run_complete(sim);

    free(sim);  // no final, antes do return 0

    return 0;
}
