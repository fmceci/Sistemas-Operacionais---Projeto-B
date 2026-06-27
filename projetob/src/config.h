#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

/* Tamanho máximo de uma linha do arquivo de configuração */
#define MAX_LINE 512

/* Tamanho máximo do nome do algoritmo de escalonamento */
#define MAX_ALGO 32

/* Número máximo de CPUs suportadas */
#define MAX_CPUS 16

/*
 * Config - parâmetros gerais da simulação.
 *
 * Lidos da primeira linha do arquivo de configuração.
 *
 * Projeto A — formatos aceitos:
 *   "SRTF;5;2"    → algoritmo SRTF, quantum 5, 2 CPUs
 *   "PRIOP;5;2"   → algoritmo PRIOP, quantum 5, 2 CPUs
 *
 * Projeto B — novo formato para PRIOPEnv (req 1.1):
 *   "PRIOPEnv;5;2;1" → algoritmo PRIOPEnv, quantum 5, 2 CPUs, alpha 1
 *
 * O campo alpha é ignorado (e deixado em 0) para SRTF e PRIOP.
 */
typedef struct {
    char algorithm[MAX_ALGO]; /* Nome do algoritmo: "SRTF", "PRIOP" ou "PRIOPENV" */
    int  quantum;             /* Quantum máximo por fatia de execução               */
    int  cpu_count;           /* Número de CPUs disponíveis (mínimo 2)              */
    int  alpha;               /* Fator de envelhecimento para PRIOPEnv (req 1.1)    */
} Config;

/*
 * load_config - lê o arquivo de configuração e preenche Config e vetor de Task.
 *
 * Formato do arquivo:
 *   Linha 1: algoritmo;quantum;qtde_cpus[;alpha]
 *   Linha 2+: id;cor;ingresso;duracao;prioridade[;lista_eventos]
 *
 * O campo lista_eventos é parseado por parse_events() (events.c).
 *
 * Retorna 1 em sucesso, 0 em erro.
 */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count);

#endif /* CONFIG_H */
