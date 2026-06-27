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
 *   Separador entre eventos: vírgula (',')
 *   Exemplo: "ML01:02,IO:04-03,MU01:09"
 */

/* Número máximo de eventos que uma tarefa pode ter */
#define MAX_EVENTS_PER_TASK 64

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
 * EventList — lista de eventos de uma única tarefa.
 * Armazenada diretamente no TCB (task.h).
 */
typedef struct {
    Event list[MAX_EVENTS_PER_TASK];
    int   count;
} EventList;

/*
 * parse_events - interpreta a string de eventos do arquivo de configuração
 *               e preenche um EventList.
 *
 * Parâmetros:
 *   str  - string bruta (ex: "ML01:02,IO:04-03,MU01:09")
 *          Valor "-" ou vazio indica ausência de eventos.
 *   out  - ponteiro para o EventList a preencher
 *
 * Eventos são separados por vírgula. A função ignora silenciosamente
 * tokens malformados, conforme req 3.3.2 (case-insensitive).
 */
void parse_events(const char *str, EventList *out);

#endif /* EVENTS_H */
