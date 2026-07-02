#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "events.h"

/* Capacidade inicial do vetor quando o primeiro evento for inserido.
 * Começar com 8 é um bom equilíbrio para não alocar muita memória à toa,
 * mas também não precisar realocar logo nos primeiros itens. */
#define EVENT_INITIAL_CAP 8

/* -----------------------------------------------------------------------
 * Ciclo de vida da EventList (Inicialização / Liberação / Cópia)
 * ----------------------------------------------------------------------- */

/*
 * event_list_init - Prepara a lista para ser usada pela primeira vez.
 * É fundamental inicializar ponteiros com NULL e contadores com 0 para 
 * evitar comportamento indesejado (lixo de memória).
 */
void event_list_init(EventList *el) {
    el->list     = NULL;
    el->count    = 0;
    el->capacity = 0;
}

/*
 * event_list_free - Libera a memória que foi alocada para os eventos
 * e devolve a lista ao seu estado inicial (vazio).
 */
void event_list_free(EventList *el) {
    if (el == NULL) return; // Proteção contra ponteiros nulos
    
    free(el->list);      /* A função free(NULL) é segura em C, então não precisamos testar se list != NULL */
    el->list     = NULL; /* Zera o ponteiro para evitar o uso de memória já liberada (dangling pointer) */
    el->count    = 0;
    el->capacity = 0;
}

/*
 * event_list_copy - Cria uma cópia independente (Deep Copy) da lista original.
 * Se alterarmos a cópia, a original não será afetada.
 */
void event_list_copy(EventList *dst, const EventList *src) {
    dst->count    = src->count;
    dst->capacity = src->count; /* A cópia só precisa do tamanho exato dos dados atuais */

    if (src->count > 0) {
        // Aloca espaço suficiente para guardar todos os eventos da lista fonte
        dst->list = (Event *)malloc(sizeof(Event) * (size_t)src->count);
        
        if (dst->list == NULL) {
            /* Falha de memória: Se o SO não tiver memória, deixamos a lista destino
             * vazia em vez de travar o programa abruptamente. */
            dst->count = 0;
            dst->capacity = 0;
            return;
        }
        // Copia os dados da memória da lista fonte para a recém-alocada
        memcpy(dst->list, src->list, sizeof(Event) * (size_t)src->count);
    } else {
        // Se a lista fonte estava vazia, a destino também fica
        dst->list = NULL;
        dst->capacity = 0;
    }
}

/*
 * event_list_push - Adiciona um novo evento ao final da lista.
 * Se a lista estiver cheia, ela cresce dinamicamente.
 */
static void event_list_push(EventList *el, Event ev) {
    // Verifica se atingimos o limite da capacidade atual
    if (el->count >= el->capacity) {
        /* Crescimento geométrico: dobramos a capacidade atual.
         * Isso é uma técnica clássica para evitar chamar o 'realloc' toda hora,
         * o que deixaria o programa muito lento. */
        int new_cap = (el->capacity == 0) ? EVENT_INITIAL_CAP : el->capacity * 2;
        
        // Tenta realocar a memória para o novo tamanho
        Event *novo = (Event *)realloc(el->list, sizeof(Event) * (size_t)new_cap);
        if (novo == NULL) {
            fprintf(stderr, "Erro: sem memoria ao expandir lista de eventos.\n");
            return; /* Se falhar, mantém a lista como estava e ignora a inserção */
        }
        
        // Atualiza a lista com o novo bloco de memória e a nova capacidade
        el->list     = novo;
        el->capacity = new_cap;
    }
    
    // Insere o evento na próxima posição livre e incrementa o contador
    el->list[el->count++] = ev;
}

/* -----------------------------------------------------------------------
 * Parsing dos eventos (Leitura e Interpretação de Strings)
 * ----------------------------------------------------------------------- */

/*
 * str_upper_copy - Copia uma string transformando todas as letras em MAIÚSCULAS.
 * Ajuda a padronizar a entrada, permitindo que "ml01" e "ML01" sejam lidos da mesma forma.
 */
static void str_upper_copy(char *dst, const char *src, size_t max_len) {
    size_t i;
    for (i = 0; i + 1 < max_len && src[i] != '\0'; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0'; // Garante que a string termine corretamente
}

/*
 * parse_one_token - Lê um pedaço de texto isolado (ex: "ML01:02") e transforma
 * na estrutura de dados de evento correspondente, inserindo-a na lista.
 */
static void parse_one_token(const char *token, EventList *out) {
    /* Pula espaços em branco no começo da string */
    while (*token == ' ') token++;
    if (*token == '\0') return; // Se a string ficou vazia, não há o que processar

    // Cria um evento com valores padrão limpos
    Event ev;
    ev.rel_tick = 0;
    ev.mutex_id = -1;
    ev.duration = 0;
    ev.fired    = 0; /* Define que o evento ainda não ocorreu */

    /* Verifica qual é o tipo do evento comparando os primeiros caracteres */
    
    if (strncmp(token, "ML", 2) == 0) {
        // ML = Mutex Lock
        int mid = 0, tick = 0;
        // sscanf extrai os números ignorando o texto. Esperamos 2 valores (ID e Tick)
        if (sscanf(token + 2, "%d:%d", &mid, &tick) == 2) {
            ev.type     = EVT_MUTEX_LOCK;
            ev.mutex_id = mid;
            ev.rel_tick = tick;
            event_list_push(out, ev);
        }
    } else if (strncmp(token, "MU", 2) == 0) {
        // MU = Mutex Unlock
        int mid = 0, tick = 0;
        if (sscanf(token + 2, "%d:%d", &mid, &tick) == 2) {
            ev.type     = EVT_MUTEX_UNLOCK;
            ev.mutex_id = mid;
            ev.rel_tick = tick;
            event_list_push(out, ev);
        }
    } else if (strncmp(token, "IO:", 3) == 0) {
        // IO = Input/Output
        int tick = 0, dur = 0;
        if (sscanf(token + 3, "%d-%d", &tick, &dur) == 2) {
            if (dur < 1) dur = 1; /* Garante que a operação leve pelo menos 1 tick */
            ev.type     = EVT_IO;
            ev.mutex_id = -1; // IO não usa mutex
            ev.rel_tick = tick;
            ev.duration = dur;
            event_list_push(out, ev);
        }
    }
    /* Se o texto não for nenhum desses (ex: string malformada),
     * a função simplesmente ignora e não adiciona nada. */
}

/*
 * is_event_prefix - Verifica se um trecho de texto é o começo de um evento conhecido.
 * Retorna o tamanho da sigla se for válido, ou 0 se não for.
 * Isso é útil para separar strings "grudadas" como "IO:01-02MU01:03".
 */
static int is_event_prefix(const char *p) {
    if (strncmp(p, "ML", 2) == 0) return 2;
    if (strncmp(p, "MU", 2) == 0) return 2;
    if (strncmp(p, "IO:", 3) == 0) return 3;
    return 0;
}

/*
 * parse_events - Função principal que recebe uma linha completa de texto
 * com vários eventos e preenche a lista 'out'.
 */
void parse_events(const char *str, EventList *out) {
    /* Limpa a lista de destino para garantir que não vamos misturar
     * dados novos com lixo antigo. */
    event_list_free(out);
    event_list_init(out);

    /* Se a string estiver vazia ou for um "-", encerramos cedo */
    if (str == NULL || str[0] == '\0' || strcmp(str, "-") == 0) {
        return;
    }

    /* Cria uma cópia da string original para podermos transformá-la 
     * toda em maiúsculas (facilita muito o trabalho depois).
     * Usamos malloc em vez de um vetor fixo porque a string pode ser gigante. */
    size_t len = strlen(str);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL) {
        fprintf(stderr, "Erro: sem memoria ao parsear eventos.\n");
        return;
    }
    str_upper_copy(buf, str, len + 1);

    /*
     * Lógica de Varredura:
     * Vamos ler a string caractere por caractere e ir guardando no 'token'.
     * Quando percebermos que o 'token' formou um evento completo, enviamos 
     * para 'parse_one_token' e limpamos o 'token' para começar o próximo.
     */
    char token[64];
    int  tlen = 0; // Tamanho atual do token que estamos montando
    size_t i = 0;  // Posição na string principal (buf)

    while (i < len) {
        char c = buf[i];

        /* Caso 1: Encontramos um separador claro (ponto-e-vírgula ou vírgula) */
        if (c == ';' || c == ',') {
            if (tlen > 0) {
                // Se já tínhamos algo no token, é hora de processá-lo
                token[tlen] = '\0'; // Fecha a string
                parse_one_token(token, out);
                tlen = 0; // Reinicia o token para o próximo evento
            }
            i++;
            continue; // Pula para o próximo caractere
        }

        /*
         * Caso 2: Detectamos o início de um novo evento grudado (sem separador).
         * Ex: Estamos no meio de "IO:01-02MU01:03". Se o i chegou na letra 'M' de "MU",
         * is_event_prefix vai avisar.
         */
        if (tlen > 0 && is_event_prefix(&buf[i]) > 0) {
            token[tlen] = '\0'; // Fecha o token do evento anterior (ex: "IO:01-02")
            parse_one_token(token, out);
            tlen = 0; // Reinicia o token
            
            /* ATENÇÃO: Aqui NÃO fazemos i++. Nós mantemos a posição para que
             * o próximo laço comece lendo exatamente esse 'M' que detectamos agora. */
            continue;
        }

        /* Caso 3: É um caractere normal de texto. Acumulamos no token atual.
         * A verificação 'tlen < sizeof(token) - 1' impede um buffer overflow
         * caso alguém passe um texto bizarro muito longo. */
        if (tlen < (int)sizeof(token) - 1) {
            token[tlen++] = c;
        }
        i++;
    }

    /* Fim do loop. Se ainda sobrou algum pedaço no token, processa ele.
     * Isso acontece com o último evento da string, que não tem separador depois dele. */
    if (tlen > 0) {
        token[tlen] = '\0';
        parse_one_token(token, out);
    }

    // Limpa a memória que alocamos para a versão em maiúsculas
    free(buf);
}
