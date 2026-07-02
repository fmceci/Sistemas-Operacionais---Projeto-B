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
 * pedir_caminho_arquivo - solicita ao usuário o caminho do arquivo de entrada.
 *
 * Quando nenhum arquivo é passado por argumento, em vez de
 * usar um caminho fixo, perguntamos ao usuário. Assim o programa roda QUALQUER
 * arquivo .txt sem recompilar.
 */
static void pedir_caminho_arquivo(char *dest, size_t tam) {
    printf("Digite o caminho do arquivo de entrada (.txt): ");
    if (fgets(dest, (int)tam, stdin) != NULL) {
        dest[strcspn(dest, "\r\n")] = '\0'; /* remove o \n */
    } else {
        dest[0] = '\0';
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    srand((unsigned int)time(NULL));

    /* [CORREÇÃO BUG 2] Caminho vem de argv[1]; se ausente, pergunta ao usuário */
    char caminho[1024];
    if (argc >= 2) {
        strncpy(caminho, argv[1], sizeof(caminho) - 1);
        caminho[sizeof(caminho) - 1] = '\0';
    } else {
        pedir_caminho_arquivo(caminho, sizeof(caminho));
        if (caminho[0] == '\0') {
            fprintf(stderr, "Nenhum arquivo informado. Encerrando.\n");
            return 1;
        }
    }

    Config config;
    Task   tasks[MAX_TASKS];
    int    task_count = 0;

    if (!load_config(caminho, &config, tasks, &task_count)) {
        fprintf(stderr, "Falha ao carregar configuracao de '%s'. Encerrando.\n",
                caminho);
        return 1;
    }

    printf("\n========================================\n");
    printf("  Simulador de SO Multitarefa - v0.5B\n");
    printf("========================================\n\n");

    printf("Configuracoes carregadas de '%s':\n", caminho);
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n", config.cpu_count);
    if (config.alpha > 0)
        printf("  Alpha     : %d (envelhecimento)\n\n", config.alpha);
    else
        printf("\n");

    /*
     * Menu com os 7 algoritmos selecionáveis.
     * O nome interno (usado no scheduler) está entre parênteses.
     */
    printf("Selecione o algoritmo (ENTER para manter %s):\n", config.algorithm);
    printf("  1 - FCFS  (First-Come, First-Served)        [cooperativo]\n");
    printf("  2 - RR    (Round Robin)                     [preemptivo]\n");
    printf("  3 - SJF   (Shortest Job First)              [cooperativo]\n");
    printf("  4 - SRTF  (Shortest Remaining Time First)   [preemptivo]\n");
    printf("  5 - PRIOc (Prioridade Cooperativo)          [cooperativo]\n");
    printf("  6 - PRIOp (Prioridade Preemptivo)           [preemptivo]\n");
    printf("  7 - PRIOd (Prioridade Dinamica/Envelhec.)   [preemptivo]\n");
    printf("Opcao: ");

    char buffer[64];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL && buffer[0] != '\n') {
        int opcao = atoi(buffer);
        switch (opcao) {
            case 1: strcpy(config.algorithm, "FCFS");  break;
            case 2: strcpy(config.algorithm, "RR");    break;
            case 3: strcpy(config.algorithm, "SJF");   break;
            case 4: strcpy(config.algorithm, "SRTF");  break;
            case 5: strcpy(config.algorithm, "PRIOC"); break;
            case 6: strcpy(config.algorithm, "PRIOP"); break;
            case 7: strcpy(config.algorithm, "PRIOD"); break;
            default:
                printf("Opcao invalida. Mantendo: %s\n", config.algorithm);
        }
    }

    /* Alpha só faz sentido para PRIOd (prioridade dinâmica com envelhecimento) */
    int usa_envelhecimento = (strcmp(config.algorithm, "PRIOD") == 0 ||
                              strcmp(config.algorithm, "PRIOPENV") == 0);
    if (usa_envelhecimento && config.alpha == 0) {
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
    if (usa_envelhecimento)
        printf("  Alpha     : %d\n", config.alpha);
    printf("\n");

    printf("Tarefas carregadas (%d):\n", task_count);
    for (int i = 0; i < task_count; i++) {
        printf("  T%-3d | Cor: #%s | Ingresso: %2d | Duracao: %2d | Prio: %2d | Eventos: %d\n",
               tasks[i].id, tasks[i].color, tasks[i].arrival_time,
               tasks[i].duration, tasks[i].priority, tasks[i].event_list.count);
    }
    printf("\n");

    SimulationState *sim = malloc(sizeof(SimulationState));
    if (!sim) {
        fprintf(stderr, "Erro: sem memoria.\n");
        /* libera eventos das tarefas locais antes de sair */
        for (int i = 0; i < task_count; i++) event_list_free(&tasks[i].event_list);
        return 1;
    }
    sim->stall_ticks = 0; /* [BUG 1] inicializa o contador de stall */

    simulation_init(sim, &config, tasks, task_count);

    /*
     * A simulação fez CÓPIA PROFUNDA dos eventos (event_list_copy). Portanto,
     * já podemos liberar as listas das tarefas LOCAIS aqui — a simulação tem
     * as suas próprias cópias.
     */
    for (int i = 0; i < task_count; i++) {
        event_list_free(&tasks[i].event_list);
    }

    int mode = 0;
    if (argc >= 3) {
        if (strcmp(argv[2], "completo") == 0) mode = 1;
        else if (strcmp(argv[2], "passo") == 0) mode = 2;
    }

    if (mode == 0) {
        printf("Selecione o modo de execucao:\n");
        printf("  1 - Completo      (executa tudo de uma vez)\n");
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

    /* simulation_run_* já chamou simulation_free_events(sim) internamente */
    free(sim);
    return 0;
}
