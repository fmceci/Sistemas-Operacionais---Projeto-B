#ifndef IO_H
#define IO_H

/* Número máximo de operações de E/S simultâneas em andamento */
#define MAX_IO_OPS 128

/*
 * IOOp — representa uma operação de E/S em andamento.
 *
 * Uma vez agendada, a operação termina exatamente no tick 'finish_tick'.
 * No tick seguinte (finish_tick + 1), uma IRQ é gerada e a tarefa volta
 * ao estado READY (req 3.4).
 */
typedef struct {
    int task_id;     /* ID da tarefa que solicitou a E/S         */
    int finish_tick; /* Tick absoluto em que a E/S termina        */
    int active;      /* 1 = operação em curso; 0 = slot livre     */
} IOOp;

/*
 * IOQueue — fila de operações de E/S ativas na simulação.
 * Mantida no SimulationState.
 */
typedef struct {
    IOOp ops[MAX_IO_OPS];
    int  count; /* número de slots ativos */
} IOQueue;

/*
 * io_queue_init - zera a fila de E/S.
 */
void io_queue_init(IOQueue *q);

/*
 * io_schedule - agenda uma operação de E/S para a tarefa 'task_id'.
 *
 * A operação termina em 'finish_tick' (tick absoluto).
 * Chamado quando a tarefa atinge o instante relativo do evento IO.
 */
void io_schedule(IOQueue *q, int task_id, int finish_tick);

/*
 * io_check_irq - verifica se alguma operação de E/S terminou neste tick.
 *
 * Chamado no início de cada tick (antes das chegadas), para que a IRQ
 * seja processada com antecedência suficiente para o escalonador
 * considerar a tarefa no mesmo tick (req 3.4: IRQ no instante seguinte
 * ao término).
 *
 * Parâmetros:
 *   q          - fila de E/S
 *   tick       - tick atual do relógio global
 *   done       - vetor de saída: IDs das tarefas cujas E/S terminaram
 *   max_done   - tamanho máximo do vetor done[]
 *
 * Retorna o número de operações que terminaram (0 se nenhuma).
 * As entradas correspondentes são marcadas como inativas na fila.
 */
int io_check_irq(IOQueue *q, int tick, int done[], int max_done);

#endif /* IO_H */
