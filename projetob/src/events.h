/* * As três linhas abaixo são chamadas de "Include Guards" (Guardas de Inclusão).
 * Elas impedem que este arquivo seja copiado mais de uma vez durante a compilação,
 * o que causaria erros de "redefinição" de estruturas.
 */
#ifndef EVENTS_H
#define EVENTS_H

/*
 * events.h - Estruturas e funções para representar e interpretar (parsear) eventos
 * de tarefas em um sistema operacional simulado.
 *
 * Os eventos suportados são:
 * - Solicitação de Mutex (ML - Mutex Lock)
 * - Liberação de Mutex (MU - Mutex Unlock)
 * - Operações de Entrada/Saída (IO - Input/Output)
 *
 * Formato esperado no texto do arquivo de configuração:
 * ML01:02  → Travar (Lock) o mutex 01 no instante relativo 02
 * MU01:08  → Destravar (Unlock) o mutex 01 no instante relativo 08
 * IO:03-05 → Iniciar operação de E/S no instante relativo 03, que vai durar 05 ticks
 *
 * Separadores:
 * O sistema entende eventos separados por ';' ou ','.
 * Também entende eventos "grudados" (ex: "IO:01-02MU01:03"), pois a inteligência
 * do código no .c sabe identificar os inícios "ML", "MU" e "IO".
 */

/* -----------------------------------------------------------------------
 * Enumerações e Estruturas de Dados
 * ----------------------------------------------------------------------- */

/*
 * EventType — Usamos um `enum` para dar nomes amigáveis aos tipos de evento.
 * Isso torna o código muito mais legível do que usar números soltos (0, 1 e 2).
 */
typedef enum {
    EVT_MUTEX_LOCK   = 0,  /* Tipo ML */
    EVT_MUTEX_UNLOCK = 1,  /* Tipo MU */
    EVT_IO           = 2   /* Tipo IO */
} EventType;

/*
 * Event — Representa a "ficha" de um único evento que uma tarefa vai realizar.
 * * Como um evento pode ser de Mutex ou de IO, usamos a mesma estrutura para ambos,
 * mas alguns campos ficam "inativos" dependendo do tipo:
 */
typedef struct {
    EventType type;      /* Qual é o tipo deste evento? (Lock, Unlock ou IO) */
    
    int       rel_tick;  /* Quando esse evento deve acontecer? (Instante relativo ao início da tarefa) */
    
    /* Campos específicos (O uso depende do 'type' acima): */
    int       mutex_id;  /* Qual Mutex afetar? (Usado só para ML/MU. Para IO, deixamos como -1) */
    int       duration;  /* Quanto tempo demora? (Usado só para IO. Para ML/MU, deixamos como 0) */
    
    /* Estado de controle: */
    int       fired;     /* Flag de controle: 0 = "Ainda não aconteceu", 1 = "Já foi processado (disparado)" */
} Event;
