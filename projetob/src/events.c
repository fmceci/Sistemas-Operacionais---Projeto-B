#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "events.h"

/* Capacidade inicial ao inserir o primeiro evento */
#define EVENT_INITIAL_CAP 8

/* -----------------------------------------------------------------------
 * Ciclo de vida da EventList (init / free / copy)
 * ----------------------------------------------------------------------- */

void event_list_init(EventList *el) {
    el->list     = NULL;
    el->count    = 0;
    el->capacity = 0;
}

void event_list_free(EventList *el) {
    if (el == NULL) return;
    free(el->list);      /* free(NULL) é seguro */
    el->list     = NULL;
    el->count    = 0;
    el->capacity = 0;
}

void event_list_copy(EventList *dst, const EventList *src) {
    dst->count    = src->count;
    dst->capacity = src->count; /* capacidade justa é suficiente para a cópia */

    if (src->count > 0) {
        dst->list = (Event *)malloc(sizeof(Event) * (size_t)src->count);
        if (dst->list == NULL) {
            /* Falha de memória: degrada para lista vazia em vez de travar */
            dst->count = 0;
            dst->capacity = 0;
            return;
        }
        memcpy(dst->list, src->list, sizeof(Event) * (size_t)src->count);
    } else {
        dst->list = NULL;
        dst->capacity = 0;
    }
}

/*
 * event_list_push - acrescenta um evento à lista, crescendo se necessário.
 * [CORREÇÃO BUG 4] Crescimento geométrico (dobra) para amortizar realloc.
 */
static void event_list_push(EventList *el, Event ev) {
    if (el->count >= el->capacity) {
        int new_cap = (el->capacity == 0) ? EVENT_INITIAL_CAP : el->capacity * 2;
        Event *novo = (Event *)realloc(el->list, sizeof(Event) * (size_t)new_cap);
        if (novo == NULL) {
            fprintf(stderr, "Erro: sem memoria ao expandir lista de eventos.\n");
            return; /* mantém o que já existe; ignora este evento */
        }
        el->list     = novo;
        el->capacity = new_cap;
    }
    el->list[el->count++] = ev;
}

/* -----------------------------------------------------------------------
 * Parsing dos eventos
 * ----------------------------------------------------------------------- */

/*
 * str_upper_copy - copia src para dst em MAIÚSCULAS, respeitando max_len.
 * Necessário para o req 3.3.2 (case-insensitive).
 */
static void str_upper_copy(char *dst, const char *src, size_t max_len) {
    size_t i;
    for (i = 0; i + 1 < max_len && src[i] != '\0'; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/*
 * parse_one_token - interpreta UM token já isolado (ex.: "ML01:02", "IO:03-05")
 * e, se válido, adiciona o evento a 'out'. Tokens malformados são ignorados.
 */
static void parse_one_token(const char *token, EventList *out) {
    /* Pula espaços iniciais */
    while (*token == ' ') token++;
    if (*token == '\0') return;

    Event ev;
    ev.rel_tick = 0;
    ev.mutex_id = -1;
    ev.duration = 0;

    if (strncmp(token, "ML", 2) == 0) {
        int mid = 0, tick = 0;
        if (sscanf(token + 2, "%d:%d", &mid, &tick) == 2) {
            ev.type     = EVT_MUTEX_LOCK;
            ev.mutex_id = mid;
            ev.rel_tick = tick;
            event_list_push(out, ev);
        }
    } else if (strncmp(token, "MU", 2) == 0) {
        int mid = 0, tick = 0;
        if (sscanf(token + 2, "%d:%d", &mid, &tick) == 2) {
            ev.type     = EVT_MUTEX_UNLOCK;
            ev.mutex_id = mid;
            ev.rel_tick = tick;
            event_list_push(out, ev);
        }
    } else if (strncmp(token, "IO:", 3) == 0) {
        int tick = 0, dur = 0;
        if (sscanf(token + 3, "%d-%d", &tick, &dur) == 2) {
            if (dur < 1) dur = 1; /* duração mínima = 1 (req 3.4) */
            ev.type     = EVT_IO;
            ev.mutex_id = -1;
            ev.rel_tick = tick;
            ev.duration = dur;
            event_list_push(out, ev);
        }
    }
    /* Qualquer outro prefixo é ignorado silenciosamente */
}

/*
 * is_event_prefix - retorna o comprimento do prefixo de evento que começa
 * em 'p' (2 para ML/MU, 3 para IO:) ou 0 se não for início de evento.
 *
 * [CORREÇÃO BUG 1] Usado para separar eventos CONCATENADOS sem separador,
 * como "IO:01-02MU01:03".
 */
static int is_event_prefix(const char *p) {
    if (strncmp(p, "ML", 2) == 0) return 2;
    if (strncmp(p, "MU", 2) == 0) return 2;
    if (strncmp(p, "IO:", 3) == 0) return 3;
    return 0;
}

void parse_events(const char *str, EventList *out) {
    /* Garante estado limpo (evita vazar se 'out' já tinha algo) */
    event_list_free(out);
    event_list_init(out);

    /* Sem eventos */
    if (str == NULL || str[0] == '\0' || strcmp(str, "-") == 0) {
        return;
    }

    /*
     * Trabalhamos sobre uma cópia em maiúsculas alocada dinamicamente,
     * pois a linha pode conter MUITOS eventos (BUG 4) e não cabe em um
     * buffer fixo pequeno.
     */
    size_t len = strlen(str);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) {
        fprintf(stderr, "Erro: sem memoria ao parsear eventos.\n");
        return;
    }
    str_upper_copy(buf, str, len + 1);

    /*
     * [CORREÇÃO BUG 1] Varredura manual (não usamos strtok simples), porque
     * precisamos lidar com três situações ao mesmo tempo:
     *   1. Separadores ';' e ','.
     *   2. Eventos concatenados sem separador ("IO:01-02MU01:03").
     *   3. Espaços e separadores redundantes.
     *
     * Estratégia: percorremos a string acumulando um token; quando
     * encontramos um separador OU o início de um novo prefixo de evento
     * (após já termos conteúdo no token), fechamos o token atual.
     */
    char token[64];
    int  tlen = 0;
    size_t i = 0;

    while (i < len) {
        char c = buf[i];

        /* Separadores explícitos fecham o token atual */
        if (c == ';' || c == ',') {
            if (tlen > 0) {
                token[tlen] = '\0';
                parse_one_token(token, out);
                tlen = 0;
            }
            i++;
            continue;
        }

        /*
         * Detecta início de um NOVO evento colado ao anterior.
         * Só fecha o token se já houver conteúdo acumulado e a posição
         * atual for realmente um prefixo de evento.
         */
        if (tlen > 0 && is_event_prefix(&buf[i]) > 0) {
            token[tlen] = '\0';
            parse_one_token(token, out);
            tlen = 0;
            /* NÃO incrementa i: o prefixo detectado inicia o próximo token */
            continue;
        }

        /* Acumula o caractere no token (com proteção de limite) */
        if (tlen < (int)sizeof(token) - 1) {
            token[tlen++] = c;
        }
        i++;
    }

    /* Fecha o último token pendente */
    if (tlen > 0) {
        token[tlen] = '\0';
        parse_one_token(token, out);
    }

    free(buf);
}
