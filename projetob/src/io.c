#include <stdio.h>
#include <string.h>
#include "io.h"

/*
 * io_queue_init - inicializa todos os slots como inativos.
 */
void io_queue_init(IOQueue *q) {
    memset(q, 0, sizeof(*q));
    q->count = 0;
}

/*
 * io_schedule - registra uma nova operação de E/S na fila.
 *
 * Busca o primeiro slot inativo e o ocupa.
 * Caso a fila esteja cheia, emite aviso e não agenda (situação anormal
 * que não deve ocorrer em cenários normais de teste).
 */
void io_schedule(IOQueue *q, int task_id, int finish_tick) {
    for (int i = 0; i < MAX_IO_OPS; i++) {
        if (!q->ops[i].active) {
            q->ops[i].task_id    = task_id;
            q->ops[i].finish_tick = finish_tick;
            q->ops[i].active     = 1;
            q->count++;
            printf("  [E/S]        T%d iniciou E/S; termina no tick %d.\n",
                   task_id, finish_tick);
            return;
        }
    }
    fprintf(stderr, "Aviso: fila de E/S cheia. Operacao de T%d nao agendada.\n",
            task_id);
}

/*
 * io_check_irq - coleta as operações que terminaram no tick atual.
 *
 * Segundo o req 3.4, a E/S termina em 'finish_tick' e a IRQ é gerada
 * "no instante de tempo imediatamente depois do instante de término".
 * Portanto, chamamos esta função no início do tick (tick = finish_tick + 1)
 * para que a tarefa possa ser re-escalonada no mesmo tick em que a IRQ chega.
 *
 * Na prática, agendamos finish_tick = clock_atual + duração, e verificamos
 * no início do tick seguinte (clock == finish_tick), o que equivale ao
 * "tick imediatamente após".
 */
int io_check_irq(IOQueue *q, int tick, int done[], int max_done) {
    int found = 0;

    for (int i = 0; i < MAX_IO_OPS && found < max_done; i++) {
        if (!q->ops[i].active) continue;

        /*
         * A IRQ é gerada quando o tick atual == finish_tick.
         * (finish_tick foi calculado como: tick_do_inicio_da_ES + duracao)
         */
        if (q->ops[i].finish_tick == tick) {
            done[found++] = q->ops[i].task_id;
            printf("  [IRQ]        E/S de T%d concluida no tick %d → T%d volta para PRONTO.\n",
                   q->ops[i].task_id, tick, q->ops[i].task_id);
            q->ops[i].active = 0;
            q->count--;
        }
    }

    return found;
}
