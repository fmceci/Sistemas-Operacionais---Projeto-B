#ifndef CONFIG_H
#define CONFIG_H

#include "task.h"

/*
 * MAX_LINE — mantido apenas para compatibilidade histórica. A leitura das
 * linhas passou a ser DINÂMICA (getline), então linhas com milhares de
 * eventos não são mais truncadas (correção do BUG 4).
 */
#define MAX_LINE 512

/* Tamanho máximo do nome do algoritmo de escalonamento */
#define MAX_ALGO 32

/* Número máximo de CPUs suportadas */
#define MAX_CPUS 16

/*
 * Config - parâmetros gerais da simulação (primeira linha do arquivo).
 *
 * Formatos aceitos:
 *   "SRTF;5;2"        → algoritmo SRTF, quantum 5, 2 CPUs
 *   "PRIOP;5;2"       → PRIOP, quantum 5, 2 CPUs
 *   "PRIOPEnv;5;2;1"  → PRIOPEnv/PRIOd, quantum 5, 2 CPUs, alpha 1
 *
 * O nome do algoritmo é normalizado para MAIÚSCULAS (req 3.3.2).
 * Nomes reconhecidos: FCFS, RR, SJF, SRTF, PRIOC, PRIOP, PRIOD (e PRIOPENV).
 */
typedef struct {
    char algorithm[MAX_ALGO]; /* Nome do algoritmo em maiúsculas          */
    int  quantum;             /* Quantum máximo por fatia de execução      */
    int  cpu_count;           /* Número de CPUs (mínimo 2)                 */
    int  alpha;               /* Fator de envelhecimento (PRIOd/PRIOPEnv)  */
} Config;

/*
 * load_config - lê o arquivo de configuração e preenche Config e vetor de Task.
 *
 * Formato:
 *   Linha 1 : algoritmo;quantum;qtde_cpus[;alpha]
 *   Linha 2+: id;cor;ingresso;duracao;prioridade[;eventos...]
 *
 * O campo 'id' pode conter prefixo textual (ex.: "t03"); apenas os dígitos
 * são usados. Todos os campos de evento restantes na linha são capturados e
 * repassados a parse_events() (correção do BUG 1).
 *
 * Retorna 1 em sucesso, 0 em erro.
 */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count);

#endif /* CONFIG_H */
