#ifndef MUTEX_H
#define MUTEX_H

#include "task.h"

/* Número máximo de mutexes distintos que podem existir na simulação */
#define MAX_MUTEXES 32

/* Número máximo de tarefas aguardando um único mutex */
#define MAX_MUTEX_QUEUE 64

/*
 * Mutex — representa um mutex do sistema operacional simulado.
 *
 * Semântica: mutex binário não-recursivo.
 *   - owner_id == -1  → mutex livre
 *   - owner_id >= 0   → mutex ocupado pela tarefa de ID owner_id
 *
 * A fila de espera (wait_queue) segue FIFO: a primeira tarefa que solicitou
 * o mutex é a primeira a recebê-lo quando ele for liberado (req 2.7).
 */
typedef struct {
    int id;                          /* Identificador do mutex (campo xx de MLxx) */
    int owner_id;                    /* ID da tarefa dona (-1 = livre)             */
    int wait_queue[MAX_MUTEX_QUEUE]; /* IDs das tarefas aguardando, em ordem FIFO  */
    int queue_len;                   /* Número de tarefas na fila                  */
} Mutex;

/*
 * MutexTable — conjunto de todos os mutexes da simulação.
 * Criado dinamicamente conforme os eventos de lock são processados.
 */
typedef struct {
    Mutex mutexes[MAX_MUTEXES];
    int   count;
} MutexTable;

/*
 * mutex_table_init - inicializa a tabela de mutexes com zero mutexes.
 */
void mutex_table_init(MutexTable *table);

/*
 * mutex_find_or_create - localiza o mutex de ID 'mutex_id' na tabela;
 * se não existir, cria um novo mutex livre.
 *
 * Retorna ponteiro para o Mutex encontrado/criado, ou NULL se a tabela
 * estiver cheia.
 */
Mutex *mutex_find_or_create(MutexTable *table, int mutex_id);

/*
 * mutex_lock - tenta adquirir o mutex 'mutex_id' para a tarefa 'task'.
 *
 * Comportamento (req 2.7):
 *   - Se o mutex está livre: a tarefa o adquire e continua RUNNING.
 *     Retorna 0 (não bloqueou).
 *   - Se o mutex está ocupado: a tarefa entra na fila de espera do mutex
 *     e deve ser suspensa (SUSPENDED) pelo chamador.
 *     Retorna 1 (bloqueou).
 *
 * Parâmetros:
 *   table    - tabela de mutexes da simulação
 *   mutex_id - identificador do mutex a adquirir
 *   task_id  - ID da tarefa que solicita o lock
 */
int mutex_lock(MutexTable *table, int mutex_id, int task_id);

/*
 * mutex_unlock - libera o mutex 'mutex_id' pela tarefa 'task_id'.
 *
 * Comportamento (req 2.7):
 *   - Remove a posse da tarefa.
 *   - Se há tarefas na fila de espera, a primeira recebe o mutex
 *     imediatamente e a função retorna o ID dessa tarefa (que deve
 *     ser acordada pelo chamador, voltando para READY).
 *   - Se a fila está vazia, o mutex volta ao estado livre e retorna -1.
 *
 * Retorna o ID da tarefa acordada, ou -1 se nenhuma foi acordada.
 */
int mutex_unlock(MutexTable *table, int mutex_id, int task_id);

#endif /* MUTEX_H */
