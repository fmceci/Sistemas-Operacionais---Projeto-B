#ifndef MUTEX_H
#define MUTEX_H

#include "task.h"

/* =======================================================================
 * LIMITES DO SISTEMA
 * ======================================================================= */

/* Número máximo de chaves (mutexes) diferentes que podem existir simultaneamente.
 * Impede que o sistema fique sem memória se criarem chaves infinitas. */
#define MAX_MUTEXES 32

/* Número máximo de tarefas que podem ficar na fila de espera para UMA única chave. */
#define MAX_MUTEX_QUEUE 64

/* =======================================================================
 * ESTRUTURAS DE DADOS
 * ======================================================================= */

/*
 * Mutex — Representa um mecanismo de Exclusão Mútua (Mutual Exclusion).
 * * Analogia: É a "chave do banheiro".
 * Regras:
 * - Binário: Ou tá livre (0) ou tá ocupado (1). Não existe "meio ocupado".
 * - Não-recursivo: Se a tarefa já tem a chave, ela não pode pedir a chave de novo.
 * - owner_id == -1  → Ninguém está usando (livre).
 * - owner_id >= 0   → ID da tarefa que pegou a chave (ocupado).
 *
 * A fila de espera (wait_queue) é estritamente FIFO (First In, First Out):
 * O primeiro a chegar na fila é o primeiro a receber a chave quando ela vagar.
 */
typedef struct {
    int id;                          /* Identificador único numérico (ex: o '1' do M01) */
    int owner_id;                    /* ID da tarefa dona da chave no momento (-1 = livre) */
    int wait_queue[MAX_MUTEX_QUEUE]; /* Os IDs das tarefas que estão na fila esperando */
    int queue_len;                   /* Quantas pessoas (tarefas) estão na fila agora? */
} Mutex;

/*
 * MutexTable — O "Chaveiro" do Sistema Operacional.
 * * Guarda todas as chaves (mutexes) que o sistema gerencia.
 * Para economizar recursos, as chaves não existem desde o começo; 
 * elas são forjadas sob demanda (quando uma tarefa pede pela primeira vez).
 */
typedef struct {
    Mutex mutexes[MAX_MUTEXES]; /* O vetor onde guardamos todas as chaves */
    int   count;                /* Quantas chaves já foram fabricadas/registradas */
} MutexTable;


/* =======================================================================
 * API / FUNÇÕES (Contratos de uso)
 * ======================================================================= */

/*
 * mutex_table_init
 * Zera o "chaveiro", preparando o sistema para iniciar a simulação limpo.
 */
void mutex_table_init(MutexTable *table);

/*
 * mutex_find_or_create
 * Procura a chave pelo ID. Se ela não existir, cria uma nova chave novinha e livre.
 * Se o chaveiro estiver lotado (passou de 32), devolve NULL (Erro).
 */
Mutex
