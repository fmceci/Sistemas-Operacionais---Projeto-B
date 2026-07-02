#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* Configurações específicas para o Windows (ex: exibir acentos corretos no CMD) */
#ifdef _WIN32
#include <windows.h>
#endif

#include "task.h"
#include "config.h"
#include "simulation.h"
#include "scheduler.h"
#include "gantt.h"

/*
 * pedir_caminho_arquivo - Solicita ao usuário o caminho do arquivo de entrada.
 *
 * Objetivo: Tornar o programa mais amigável. Se o usuário abrir o programa
 * com 2 cliques (sem usar o terminal para passar argumentos), ele não vai
 * simplesmente fechar. O programa vai pausar e pedir o caminho do arquivo TXT.
 */
static void pedir_caminho_arquivo(char *dest, size_t tam) {
    printf("Digite o caminho do arquivo de entrada (.txt): ");
    
    /* Lê a linha digitada pelo usuário */
    if (fgets(dest, (int)tam, stdin) != NULL) {
        /* strcspn procura onde está o 'Enter' (\n ou \r) e o substitui por \0 (fim de string).
         * Isso evita erros ao tentar abrir o arquivo depois. */
        dest[strcspn(dest, "\r\n")] = '\0'; 
    } else {
        dest[0] = '\0'; /* Se der erro na leitura, deixa a string vazia */
    }
}

int main(int argc, char *argv[]) {
    /* * Remove o buffer de saída e erro. 
     * Isso garante que os 'printf' apareçam na tela IMEDIATAMENTE, 
     * sem atrasos, o que é vital para ver a simulação passo-a-passo fluída.
     */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

#ifdef _WIN32
    /* Configura o terminal do Windows para usar UTF-8.
     * Assim, caracteres especiais, acentos e bordas do gráfico de Gantt
     * não vão aparecer como símbolos bizarros. */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    /* Inicializa o gerador de números aleatórios com a hora atual.
     * Usado para garantir que os sorteios (ex: desempate por Loteria)
     * sejam diferentes a cada vez que rodamos o programa. */
    srand((unsigned int)time(NULL));

    /* =======================================================================
     * 1. PREPARAÇÃO DO ARQUIVO DE ENTRADA
     * ======================================================================= */
    char caminho[1024];
    
    /* Se o usuário rodou "simulador.exe meuarquivo.txt" no terminal... */
    if (argc >= 2) {
        strncpy(caminho, argv[1], sizeof(caminho) - 1);
        caminho[sizeof(caminho) - 1] = '\0'; /* Garante segurança na string */
    } 
    /* Se o usuário só deu 2 cliques no programa... */
    else {
        pedir_caminho_arquivo(caminho, sizeof(caminho));
        if (caminho[0] == '\0') {
            fprintf(stderr, "Nenhum arquivo informado. Encerrando.\n");
            return 1; /* Sai do programa com código de erro */
        }
    }

    /* =======================================================================
     * 2. CARREGAMENTO DOS DADOS E PARSING DO TXT
     * ======================================================================= */
    Config config;               /* Vai guardar o Quantum, nº de CPUs, etc. */
    Task   tasks[MAX_TASKS];     /* O vetor que vai guardar todas as tarefas */
    int    task_count = 0;       /* Quantas tarefas foram lidas do TXT */

    /* A função load_config abre o arquivo e preenche as variáveis acima. */
    if (!load_config(caminho, &config, tasks, &task_count)) {
        fprintf(stderr, "Falha ao carregar configuracao de '%s'. Encerrando.\n", caminho);
        return 1;
    }

    /* Cabeçalho de boas vindas */
    printf("\n========================================\n");
    printf("  Simulador de SO Multitarefa - v0.5B\n");
    printf("========================================\n\n");

    /* Mostra o que foi achado dentro do TXT antes do usuário alterar algo */
    printf("Configuracoes carregadas de '%s':\n", caminho);
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n", config.cpu_count);
    if (config.alpha > 0)
        printf("  Alpha     : %d (envelhecimento)\n\n", config.alpha);
    else
        printf("\n");

    /* =======================================================================
     * 3. MENU INTERATIVO (Sobrescrita de Configurações)
     * ======================================================================= */
    
    /* O usuário pode querer rodar o mesmo arquivo TXT usando outro algoritmo.
     * Este menu permite trocar a regra do escalonador em tempo de execução. */
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
        int opcao = atoi(buffer); /* Converte a resposta em número inteiro */
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

    /* O Alpha é o "peso" do envelhecimento (evita starvation). 
     * Só perguntamos por ele se o algoritmo escolhido usar prioridade dinâmica. */
    int usa_envelhecimento = (strcmp(config.algorithm, "PRIOD") == 0 ||
                              strcmp(config.algorithm, "PRIOPENV") == 0);
    
    if (usa_envelhecimento && config.alpha == 0) {
        config.alpha = ler_inteiro_com_padrao(
            "Alpha para envelhecimento (ENTER para manter %d)", 1);
        if (config.alpha < 0) config.alpha = 0; /* Proteção contra valor negativo */
    }

    /* Ajuste do Quantum (fatia de tempo máxima por tarefa na CPU) */
    config.quantum = ler_inteiro_com_padrao(
        "Quantum (ENTER para manter %d)", config.quantum);
    if (config.quantum < 1) config.quantum = 1; /* Proteção contra quantum 0 */

    /* Ajuste da quantidade de processadores (CPUs) do simulador */
    config.cpu_count = ler_inteiro_com_padrao(
        "Numero de CPUs (ENTER para manter %d)", config.cpu_count);
    if (config.cpu_count < 2) {
        printf("Minimo de 2 CPUs. Ajustando para 2.\n");
        config.cpu_count = 2; /* Garante que o SO é multiprocessado */
    }
    if (config.cpu_count > MAX_CPUS) {
        printf("Maximo de %d CPUs. Ajustando.\n", MAX_CPUS);
        config.cpu_count = MAX_CPUS; /* Impede travamento por falta de memória */
    }

    /* =======================================================================
     * 4. RESUMO DA ÓPERA
     * ======================================================================= */
    printf("\n--- Configuracoes finais ---\n");
    printf("  Algoritmo : %s\n", config.algorithm);
    printf("  Quantum   : %d\n", config.quantum);
    printf("  CPUs      : %d\n", config.cpu_count);
    if (usa_envelhecimento)
        printf("  Alpha     : %d\n", config.alpha);
    printf("\n");

    /* Lista as tarefas que foram carregadas, como uma verificação de segurança */
    printf("Tarefas carregadas (%d):\n", task_count);
    for (int i = 0; i < task_count; i++) {
        printf("  T%-3d | Cor: #%s | Ingresso: %2d | Duracao: %2d | Prio: %2d | Eventos: %d\n",
               tasks[i].id, tasks[i].color, tasks[i].arrival_time,
               tasks[i].duration, tasks[i].priority, tasks[i].event_list.count);
    }
    printf("\n");

    /* =======================================================================
     * 5. INICIALIZAÇÃO DA SIMULAÇÃO (A MÁGICA COMEÇA AQUI)
     * ======================================================================= */
    
    /* Aloca memória para guardar o estado dinâmico do simulador */
    SimulationState *sim = malloc(sizeof(SimulationState));
    if (!sim) {
        fprintf(stderr, "Erro: sem memoria.\n");
        /* Se faltar memória, precisamos limpar a sujeira antes de fechar o app */
        for (int i = 0; i < task_count; i++) event_list_free(&tasks[i].event_list);
        return 1;
    }
    
    sim->stall_ticks = 0; /* [BUG 1 CORRIGIDO] Zera o contador de proteção contra loops infinitos */

    /* Prepara o simulador injetando nossas configurações e tarefas nele */
    simulation_init(sim, &config, tasks, task_count);

    /* * O simulation_init fez uma CÓPIA PROFUNDA (deep copy) das tarefas.
     * Ou seja, a simulação agora tem sua própria cópia independente das listas
     * de eventos (E/S, Mutex). Sendo assim, podemos destruir as listas "originais"
     * criadas aqui no main, economizando memória (evita memory leak).
     */
    for (int i = 0; i < task_count; i++) {
        event_list_free(&tasks[i].event_list);
    }

    /* =======================================================================
     * 6. ESCOLHA DE MODO DE EXECUÇÃO
     * ======================================================================= */
    int mode = 0;
    
    /* Verifica se o modo de execução foi passado por linha de comando */
    if (argc >= 3) {
        if (strcmp(argv[2], "completo") == 0) mode = 1;
        else if (strcmp(argv[2], "passo") == 0) mode = 2;
    }

    /* Se não foi passado no terminal, pergunta ao usuário */
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

    /* =======================================================================
     * 7. START!
     * ======================================================================= */
    if (mode == 2) 
        simulation_run_step_by_step(sim); /* Modo "Debug" tick por tick */
    else           
        simulation_run_complete(sim);     /* Roda até o final e solta o resultado */

    /* =======================================================================
     * 8. FIM E LIMPEZA
     * ======================================================================= */
    /* As funções simulation_run_* já limparam a memória da simulação internamente,
     * então só precisamos liberar o ponteiro principal que nós criamos no malloc. */
    free(sim);
    return 0; /* Sucesso absoluto! */
}
