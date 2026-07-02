#ifndef EVENTS_H
#define EVENTS_H

/*
 * events.h - Estruturas e funções para representar e parsear eventos
 *            de tarefas: solicitação/liberação de mutex (ML/MU) e
 *            operações de E/S (IO).
 *
 * Projeto B — requisitos 2 e 3.
 *
 * Formato esperado na lista de eventos do arquivo de configuração:
 *   ML01:02   → Lock do mutex 01 no instante relativo 02
 *   MU01:08   → Unlock do mutex 01 no instante relativo 08
 *   IO:03-05  → Operação de E/S no instante relativo 03, duração 05
 *
 * Separadores aceitos entre eventos: ponto-e-vírgula (';') e vírgula (',').
 * Além disso, eventos CONCATENADOS sem separador (ex.: "IO:01-02MU01:03")
 * também são reconhecidos, pois o parser detecta os prefixos ML/MU/IO.
 *
 * [CORREÇÃO BUG 4] A lista de eventos agora é ALOCADA DINAMICAMENTE, sem
 * limite fixo. Uma tarefa pode ter milhares de eventos.
 */

/*
 * EventType — tipos de eventos suportados.
 */
typedef enum {
    EVT_MUTEX_LOCK   = 0,  /* MLxx:tt — solicitar mutex xx no instante relativo tt */
    EVT_MUTEX_UNLOCK = 1,  /* MUxx:tt — liberar mutex xx no instante relativo tt   */
    EVT_IO           = 2   /* IO:tt-dd — E/S no instante relativo tt, duração dd   */
} EventType;

/*
 * Event — um único evento de uma tarefa.
 *
 * Para MUTEX_LOCK e MUTEX_UNLOCK:
 *   rel_tick = instante relativo ao início da tarefa
 *   mutex_id = identificador do mutex (campo xx)
 *   duration = não utilizado (0)
 *
 * Para IO:
 *   rel_tick = instante relativo em que a E/S inicia
 *   mutex_id = não utilizado (-1)
 *   duration = tempo de duração da operação de E/S
 */
typedef struct {
    EventType type;
    int       rel_tick;  /* instante relativo ao início da tarefa */
    int       mutex_id;  /* ID do mutex (LOCK/UNLOCK); -1 para IO */
    int       duration;  /* duração da E/S; 0 para mutex */
} Event;

/*
 * EventList — lista dinâmica de eventos de uma única tarefa.
 *
 * [CORREÇÃO BUG 4] Substituído o array estático por um vetor dinâmico:
 *   list     - ponteiro para os eventos (alocado com malloc/realloc)
 *   count    - número de eventos efetivamente armazenados
 *   capacity - capacidade atual alocada
 */
typedef struct {
    Event *list;
    int    count;
    int    capacity;
} EventList;

/*
 * event_list_init - inicializa uma EventList vazia (sem alocação inicial).
 */
void event_list_init(EventList *el);

/*
 * event_list_free - libera a memória interna da EventList e a zera.
 * Segura para chamar em listas já vazias/liberadas (idempotente).
 */
void event_list_free(EventList *el);

/*
 * event_list_copy - realiza uma CÓPIA PROFUNDA de src para dst.
 *
 * Necessária porque as tarefas são copiadas por valor para dentro do
 * SimulationState. Sem cópia profunda, dois TCBs apontariam para o mesmo
 * bloco de eventos, causando double-free ao liberar.
 *
 * dst NÃO deve conter memória previamente alocada (ou deve ser liberado antes).
 */
void event_list_copy(EventList *dst, const EventList *src);

/*
 * parse_events - interpreta a string de eventos do arquivo de configuração
 *                e preenche um EventList (alocando dinamicamente).
 *
 * Parâmetros:
 *   str  - string bruta (ex: "ML01:02;IO:04-03;MU01:09" ou "IO:01-02MU01:03")
 *          Valor "-", NULL ou vazio indica ausência de eventos.
 *   out  - ponteiro para o EventList a preencher (deve estar inicializado
 *          ou zerado; a função o reinicia internamente).
 *
 * Eventos podem ser separados por ';' ou ',', e também reconhecidos quando
 * concatenados sem separador. Tokens malformados são ignorados (req 3.3.2,
 * tratamento case-insensitive).
 */
void parse_events(const char *str, EventList *out);

#endif /* EVENTS_H */
