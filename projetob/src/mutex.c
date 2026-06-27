#include <stdio.h>
#include <string.h>
#include "mutex.h"

/*
 * mutex_table_init - zera a tabela de mutexes.
 */
void mutex_table_init(MutexTable *table) {
    memset(table, 0, sizeof(*table));
    table->count = 0;
}

/*
 * mutex_find_or_create - busca o mutex pelo ID; cria se não existir.
 *
 * Criação lazy: mutexes só existem quando uma tarefa tenta adquiri-los
 * pela primeira vez. Isso evita que o arquivo de configuração precise
 * declarar mutexes explicitamente.
 */
Mutex *mutex_find_or_create(MutexTable *table, int mutex_id) {
    /* Busca linear (número de mutexes é pequeno) */
    for (int i = 0; i < table->count; i++) {
        if (table->mutexes[i].id == mutex_id) {
            return &table->mutexes[i];
        }
    }

    /* Não encontrou: cria novo mutex livre */
    if (table->count >= MAX_MUTEXES) {
        fprintf(stderr, "Erro: numero maximo de mutexes (%d) atingido.\n",
                MAX_MUTEXES);
        return NULL;
    }

    Mutex *m     = &table->mutexes[table->count++];
    m->id        = mutex_id;
    m->owner_id  = -1;  /* livre */
    m->queue_len = 0;
    memset(m->wait_queue, -1, sizeof(m->wait_queue));

    return m;
}

/*
 * mutex_lock - tenta adquirir o mutex para a tarefa.
 *
 * Retorna 0 se adquiriu (tarefa continua RUNNING).
 * Retorna 1 se bloqueou (chamador deve colocar tarefa em SUSPENDED).
 */
int mutex_lock(MutexTable *table, int mutex_id, int task_id) {
    Mutex *m = mutex_find_or_create(table, mutex_id);
    if (m == NULL) return 0; /* Falha silenciosa: deixa a tarefa continuar */

    if (m->owner_id == -1) {
        /* Mutex livre: a tarefa o adquire imediatamente */
        m->owner_id = task_id;
        printf("  [MUTEX]      T%d adquiriu mutex M%d.\n", task_id, mutex_id);
        return 0; /* não bloqueou */
    }

    /* Mutex ocupado: entra na fila de espera */
    if (m->queue_len < MAX_MUTEX_QUEUE) {
        m->wait_queue[m->queue_len++] = task_id;
        printf("  [MUTEX]      T%d bloqueada aguardando mutex M%d (dono: T%d).\n",
               task_id, mutex_id, m->owner_id);
        return 1; /* bloqueou */
    }

    /* Fila cheia (improvável): deixa continuar sem bloquear */
    fprintf(stderr, "Aviso: fila do mutex M%d cheia. T%d nao bloqueou.\n",
            mutex_id, task_id);
    return 0;
}

/*
 * mutex_unlock - libera o mutex e acorda a próxima tarefa da fila, se houver.
 *
 * Retorna o ID da tarefa acordada, ou -1 se a fila estava vazia.
 */
int mutex_unlock(MutexTable *table, int mutex_id, int task_id) {
    Mutex *m = mutex_find_or_create(table, mutex_id);
    if (m == NULL) return -1;

    /* Verifica se quem está liberando é realmente o dono */
    if (m->owner_id != task_id) {
        fprintf(stderr,
                "Aviso: T%d tentou liberar mutex M%d, mas o dono e T%d.\n",
                task_id, mutex_id, m->owner_id);
        return -1;
    }

    if (m->queue_len == 0) {
        /* Nenhuma tarefa esperando: mutex fica livre */
        m->owner_id = -1;
        printf("  [MUTEX]      T%d liberou mutex M%d (livre agora).\n",
               task_id, mutex_id);
        return -1;
    }

    /*
     * Há tarefas esperando: a primeira da fila recebe o mutex (FIFO).
     * Essa escolha segue a política FIFO simples — sem herança de prioridade
     * (que seria necessária para tratar inversão de prioridade, um item
     * de análise mencionado no enunciado mas não obrigatório na implementação).
     */
    int next_task = m->wait_queue[0];

    /* Remove o primeiro da fila (shift para a esquerda) */
    for (int i = 0; i < m->queue_len - 1; i++) {
        m->wait_queue[i] = m->wait_queue[i + 1];
    }
    m->queue_len--;

    m->owner_id = next_task;

    printf("  [MUTEX]      T%d liberou mutex M%d → T%d o recebeu e volta para PRONTO.\n",
           task_id, mutex_id, next_task);

    return next_task; /* chamador deve acordar esta tarefa (→ READY) */
}
