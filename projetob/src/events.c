#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "events.h"

/*
 * str_upper_copy - copia a string src para dst convertendo para maiúsculas.
 * Respeita o tamanho máximo max_len (incluindo '\0').
 * Necessário para atender ao req 3.3.2 (case-insensitive).
 */
static void str_upper_copy(char *dst, const char *src, int max_len) {
    int i;
    for (i = 0; i < max_len - 1 && src[i] != '\0'; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/*
 * parse_events - converte a string bruta de eventos em um EventList.
 *
 * Algoritmo:
 *   1. Faz uma cópia da string para não modificar o original (strtok é destrutivo).
 *   2. Tokeniza por vírgula.
 *   3. Para cada token, detecta o tipo pelo prefixo (ML, MU, IO) e extrai
 *      os parâmetros com sscanf.
 *   4. Tokens inválidos são silenciosamente ignorados.
 *
 * Exemplos válidos:
 *   "ML01:02"  → MUTEX_LOCK,  mutex_id=1,  rel_tick=2,  duration=0
 *   "MU01:08"  → MUTEX_UNLOCK, mutex_id=1, rel_tick=8,  duration=0
 *   "IO:03-05" → IO,           mutex_id=-1, rel_tick=3, duration=5
 */
void parse_events(const char *str, EventList *out) {
    out->count = 0;

    /* Sem eventos */
    if (str == NULL || str[0] == '\0' || strcmp(str, "-") == 0) {
        return;
    }

    /* Cópia local para tokenização segura (strtok modifica a string) */
    char buf[512];
    str_upper_copy(buf, str, sizeof(buf));

    char *token = strtok(buf, ",");
    while (token != NULL && out->count < MAX_EVENTS_PER_TASK) {
        /* Remove espaços em volta do token */
        while (*token == ' ') token++;

        Event ev;
        ev.rel_tick = 0;
        ev.mutex_id = -1;
        ev.duration = 0;

        if (strncmp(token, "ML", 2) == 0) {
            /*
             * Formato: MLxx:tt
             *   xx = ID do mutex (2 dígitos, mas aceita mais)
             *   tt = instante relativo
             */
            int mid = 0, tick = 0;
            if (sscanf(token + 2, "%d:%d", &mid, &tick) == 2) {
                ev.type     = EVT_MUTEX_LOCK;
                ev.mutex_id = mid;
                ev.rel_tick = tick;
                out->list[out->count++] = ev;
            }
            /* else: token malformado, ignora */

        } else if (strncmp(token, "MU", 2) == 0) {
            /*
             * Formato: MUxx:tt
             */
            int mid = 0, tick = 0;
            if (sscanf(token + 2, "%d:%d", &mid, &tick) == 2) {
                ev.type     = EVT_MUTEX_UNLOCK;
                ev.mutex_id = mid;
                ev.rel_tick = tick;
                out->list[out->count++] = ev;
            }

        } else if (strncmp(token, "IO:", 3) == 0) {
            /*
             * Formato: IO:xx-yy
             *   xx = instante relativo em que a E/S ocorre
             *   yy = duração da operação (mínimo 1, req 3.4)
             */
            int tick = 0, dur = 0;
            if (sscanf(token + 3, "%d-%d", &tick, &dur) == 2) {
                if (dur < 1) dur = 1; /* duração mínima = 1 (req 3.4) */
                ev.type     = EVT_IO;
                ev.mutex_id = -1;
                ev.rel_tick = tick;
                ev.duration = dur;
                out->list[out->count++] = ev;
            }
        }
        /* Qualquer outro prefixo é ignorado silenciosamente */

        token = strtok(NULL, ",");
    }
}
