#include <stdio.h>
#include <string.h>
#include "mutex.h"

/* =======================================================================
 * INICIALIZAÇÃO E GERENCIAMENTO DE MUTEXES
 * ======================================================================= */

/*
 * mutex_table_init - Zera a tabela (o "chaveiro" do sistema) de mutexes.
 * * Objetivo: Preparar o sistema quando ele é ligado. 
 * Garante que não haja lixo de memória atrapalhando a contagem.
 */
void mutex_table_init(MutexTable *table) {
    /* Preenche toda a estrutura de memória com zeros e zera o contador */
    memset(table, 0, sizeof(*table));
    table->count = 0;
}

/*
 * mutex_find_or_create - Busca um mutex pelo seu ID numérico; cria um se não existir.
 *
 * Objetivo: Criação "Preguiçosa" (Lazy). Em vez de obrigar o usuário a 
 * declarar no arquivo TXT "vou usar o mutex 1 e o 2", o sistema cria a 
 * chave automaticamente na primeira vez que alguém tentar usá-la.
 */
Mutex *mutex_find_or_create(MutexTable *table, int mutex_id) {
    
    /* 1. Busca Linear: Procura no chaveiro se essa chave já existe */
    for (int i = 0; i < table->count; i++) {
        if (table->mutexes[i].id == mutex_id) {
            return &table->mutexes[i]; /* Achou! Devolve a chave existente */
        }
    }

    /* 2. Não achou: Precisamos fabricar uma chave nova.
     * Primeiro verificamos se ainda há espaço no chaveiro (limite máximo). */
    if (table->count >= MAX_MUTEXES) {
        fprintf(stderr, "Erro: numero maximo de mutexes (%d) atingido.\n",
                MAX_MUTEXES);
        return NULL;
    }

    /* 3. Fabrica a nova chave em uma posição livre da tabela */
    Mutex *m     = &table->mutexes[table->count++];
    m->id        = mutex_id;
    m->owner_id  = -1;  /* Ninguém é dono dela ainda (-1 = livre) */
    m->queue_len = 0;   /* A fila de espera começa vazia */
    
    /* Preenche a fila de espera com -1 (indicando posições vazias) */
    memset(m->wait_queue, -1, sizeof(m->wait_queue));

    return m;
}


/* =======================================================================
 * OPERAÇÕES PRINCIPAIS: LOCK (Trancar) e UNLOCK (Destrancar)
 * ======================================================================= */

/*
 * mutex_lock - Uma tarefa tenta pegar a chave (adquirir o mutex).
 *
 * Retornos importantes para o Escalonador (quem chamou a função):
 * -> Retorna 0: A tarefa conseguiu a chave (ou houve erro tolerável) e continua rodando (RUNNING).
 * -> Retorna 1: A chave estava em uso. A tarefa foi bloqueada (deve ir para SUSPENDED/BLOCKED).
 */
int mutex_lock(MutexTable *table, int mutex_id, int task_id) {
    Mutex *m = mutex_find_or_create(table, mutex_id);
    
    /* Se deu erro ao criar a chave (ex: limite atingido), finge que não aconteceu nada
     * para não quebrar a simulação inteira. */
    if (m == NULL) return 0; 

    /* Cenário A: A chave está livre! */
    if (m->owner_id == -1) {
        /* A tarefa atual anota seu nome na chave (vira a dona) */
        m->owner_id = task_id;
        printf("  [MUTEX]      T%d adquiriu mutex M%d.\n", task_id, mutex_id);
        return 0; /* Sucesso. A tarefa não precisa ser bloqueada. */
    }

    /* Cenário B: A chave já tem dono! (Mutex ocupado) */
    /* Verifica se ainda cabe gente na fila de espera. */
    if (m->queue_len < MAX_MUTEX_QUEUE) {
        /* Coloca a tarefa atual no final da fila de espera do mutex */
        m->wait_queue[m->queue_len++] = task_id;
        printf("  [MUTEX]      T%d bloqueada aguardando mutex M%d (dono: T%d).\n",
               task_id, mutex_id, m->owner_id);
        return 1; /* Retorna 1 para avisar ao SO: "Bloqueia essa tarefa!" */
    }

    /* Cenário C (Raro): A fila de espera do mutex lotou. */
    fprintf(stderr, "Aviso: fila do mutex M%d cheia. T%d nao bloqueou.\n",
            mutex_id, task_id);
    return 0; /* Deixa continuar para não travar o sistema irreversivelmente */
}

/*
 * mutex_unlock - A tarefa dona da chave terminou de usar e quer devolver.
 *
 * Retornos importantes para o Escalonador:
 * -> Retorna -1: Chave devolvida, mas não tinha ninguém esperando.
 * -> Retorna um ID (>0): Chave devolvida para o ID X. O SO precisa acordar essa tarefa!
 */
int mutex_unlock(MutexTable *table, int mutex_id, int task_id) {
    Mutex *m = mutex_find_or_create(table, mutex_id);
    if (m == NULL) return -1;

    /* REGRA DE SEGURANÇA: Só o dono da chave pode devolver a chave!
     * Isso impede que uma tarefa maliciosa ou bugada libere o mutex de outra. */
    if (m->owner_id != task_id) {
        fprintf(stderr,
                "Aviso: T%d tentou liberar mutex M%d, mas o dono e T%d.\n",
                task_id, mutex_id, m->owner_id);
        return -1;
    }

    /* Cenário A: Ninguém estava na fila esperando. */
    if (m->queue_len == 0) {
        m->owner_id = -1; /* O mutex fica completamente livre */
        printf("  [MUTEX]      T%d liberou mutex M%d (livre agora).\n",
               task_id, mutex_id);
        return -1; /* Avisa ao SO que ninguém precisa ser acordado */
    }

    /* Cenário B: Tem gente na fila! 
     * Pega a primeira tarefa da fila (política FIFO - First In, First Out) */
    int next_task = m->wait_queue[0];

    /* Como tiramos o primeiro da fila, todos os outros dão um passo à frente.
     * Fazemos um "shift" (deslocamento) dos IDs para a esquerda do vetor. */
    for (int i = 0; i < m->queue_len - 1; i++) {
        m->wait_queue[i] = m->wait_queue[i + 1];
    }
    m->queue_len--; /* Reduz o tamanho da fila, já que um saiu */

    /* A primeira tarefa da fila agora é a nova dona oficial da chave */
    m->owner_id = next_task;

    printf("  [MUTEX]      T%d liberou mutex M%d → T%d o recebeu e volta para PRONTO.\n",
           task_id, mutex_id, next_task);

    /* Devolve o ID da tarefa que ganhou a chave.
     * O Escalonador usará esse ID para mudar o estado dela de BLOQUEADA para PRONTA. */
    return next_task; 
}
