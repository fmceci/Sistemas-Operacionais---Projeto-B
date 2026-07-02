/*
 * config.c
 * ---------
 * Este arquivo é responsável por LER o arquivo de configuração da simulação
 * (texto puro) e PREENCHER as estruturas de dados que o resto do programa
 * vai usar: a struct Config (parâmetros gerais) e o vetor de Task (uma
 * struct por tarefa/processo simulado).
 *
 * Formato esperado do arquivo de entrada:
 *
 *   Linha 1: algoritmo;quantum;qtde_cpus[;alpha]
 *            Exemplo: "PRIOPEnv;3;2;1"
 *
 *   Linha 2 em diante (uma por tarefa):
 *   id;cor;ingresso;duracao;prioridade[;lista_de_eventos]
 *            Exemplo: "t03;FF0000;1;30;4;ML01:00;IO:01-02;MU01:03"
 *
 * Os "eventos" (ML = lock de mutex, MU = unlock de mutex, IO = operação
 * de E/S) ficam concentrados na "cauda" da linha e são interpretados por
 * uma função separada: parse_events() (ver events.c).
 */

/*
 * _GNU_SOURCE precisa ser definido ANTES de qualquer #include para
 * habilitar extensões GNU da biblioteca padrão (aqui, historicamente,
 * era usado para liberar a função getline(); neste arquivo específico
 * getline() não é mais usada — veja o comentário mais abaixo sobre
 * fgets() — mas a definição foi mantida por segurança/compatibilidade).
 */
#define _GNU_SOURCE
#include <stdio.h>   /* fopen, fgets, fprintf, FILE                 */
#include <stdlib.h>  /* atoi                                        */
#include <string.h>  /* strtok, strncpy, strcpy, strlen, strcspn     */
#include <ctype.h>   /* toupper, isdigit                             */
#include "config.h"  /* struct Config, MAX_ALGO, MAX_TASKS etc.      */
#include "events.h"  /* struct EventList, parse_events()             */

/* -------------------------------------------------------------------
 * Funções auxiliares (privadas a este arquivo)
 *
 * A palavra-chave 'static' aqui significa que estas funções só podem
 * ser chamadas DENTRO deste arquivo .c — são "detalhes internos" de
 * implementação, não fazem parte da interface pública (config.h) do
 * módulo.
 * ------------------------------------------------------------------- */

/*
 * str_to_upper - converte uma string para MAIÚSCULAS, "in-place"
 * (ou seja, modifica o próprio conteúdo do buffer 's', sem criar uma
 * cópia nova).
 *
 * Isso é usado para normalizar o nome do algoritmo de escalonamento,
 * pois o enunciado exige tratamento "case-insensitive": "PRIOP",
 * "priop" e "PrioP" devem significar exatamente a mesma coisa
 * (requisito 3.3.2). Convertendo tudo para maiúsculas logo na leitura,
 * o resto do programa só precisa comparar contra strings em maiúsculas
 * (ex.: "SRTF", "FCFS"), sem se preocupar com variações de caixa.
 */
static void str_to_upper(char *s) {
    /* Percorre a string caractere a caractere até encontrar o
     * terminador nulo '\0' (fim da string em C). */
    for (; *s; s++) {
        *s = (char)toupper((unsigned char)*s);
        /* O cast para 'unsigned char' antes de toupper() é uma boa
         * prática defensiva: se o char for negativo (o que pode
         * acontecer com certos caracteres acentuados em algumas
         * plataformas), passar isso direto para toupper() é
         * comportamento indefinido. Convertendo para unsigned char
         * primeiro, evitamos esse problema. */
    }
}

/*
 * extrair_id_numerico - extrai o primeiro número presente em um token.
 *
 * O campo "id" da tarefa pode vir de formas variadas no arquivo de
 * configuração, como "t03", "T10", "tarefa7" ou simplesmente "3".
 * Esta função ignora qualquer prefixo que NÃO seja dígito e lê os
 * números a partir daí, devolvendo o valor inteiro correspondente.
 *
 * Exemplos:
 *   "t03"  -> 3
 *   "T10"  -> 10
 *   "7"    -> 7
 *   NULL   -> 0 (valor de segurança, evita acessar ponteiro inválido)
 */
static int extrair_id_numerico(const char *token) {
    if (token == NULL) return 0;

    /* Avança o ponteiro enquanto o caractere atual existir (*token != 0)
     * E não for um dígito. Ou seja: "pula" qualquer letra/prefixo até
     * encontrar o primeiro número. */
    while (*token && !isdigit((unsigned char)*token)) {
        token++;
    }

    /* atoi() converte a string (a partir daqui, já sem o prefixo) para
     * um número inteiro. Se não sobrou nenhum dígito, atoi() devolve 0,
     * o que também é um valor "seguro" de fallback. */
    return atoi(token);
}

/* -------------------------------------------------------------------
 * load_config - função principal deste módulo.
 *
 * Parâmetros:
 *   filename   - caminho do arquivo de configuração a ser lido
 *   config     - ponteiro para a struct Config que será preenchida
 *                (algoritmo, quantum, qtde de CPUs, alpha)
 *   tasks[]    - vetor (já alocado pelo chamador) onde cada tarefa lida
 *                será armazenada
 *   task_count - ponteiro para um inteiro que, ao final, vai conter
 *                quantas tarefas foram efetivamente carregadas
 *
 * Retorno:
 *   1 (verdadeiro) em caso de sucesso
 *   0 (falso) em caso de erro (arquivo não encontrado, vazio, etc.)
 * ------------------------------------------------------------------- */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count) {

    /* Tenta abrir o arquivo em modo leitura ("r" = read/texto). Se o
     * caminho estiver errado ou o arquivo não existir/não tiver
     * permissão de leitura, fopen() devolve NULL. */
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", filename);
        return 0; /* Falha: sinaliza erro ao chamador */
    }

    /*
     * Buffer FIXO de 10.000 caracteres para armazenar cada linha lida
     * do arquivo.
     *
     * ATENÇÃO / PONTO DE ATENÇÃO PARA ESTUDO:
     * Diferente de uma leitura com getline() (que aloca memória
     * dinamicamente e cresce conforme necessário), aqui usamos um
     * array de tamanho FIXO na pilha (stack). Isso é mais simples de
     * entender, mas tem uma limitação importante: se alguma linha do
     * arquivo de configuração (por exemplo, uma tarefa com MUITOS
     * eventos de mutex/E-S) tiver mais de 10.000 caracteres, ela será
     * TRUNCADA silenciosamente por fgets() — ou seja, o restante da
     * linha além desse limite é simplesmente perdido/ignorado nesta
     * leitura. Para a maioria dos casos de teste isso não é problema,
     * mas é bom ter consciência dessa limitação ao usar um buffer de
     * tamanho fixo em vez de alocação dinâmica.
     */
    char line[10000];

    /* --- Primeira linha do arquivo: parâmetros gerais do sistema --- */

    /* fgets() lê uma linha inteira (até encontrar '\n' ou até encher o
     * buffer) e a armazena em 'line', incluindo o '\n' no final (se
     * houver espaço). Devolve NULL se não conseguiu ler nada (arquivo
     * vazio ou erro de leitura). */
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Erro: arquivo '%s' esta vazio.\n", filename);
        fclose(file); /* sempre fechar o arquivo antes de sair, para não
                        * vazar o descritor de arquivo (recurso do SO) */
        return 0;
    }

    /*
     * strcspn(line, "\r\n") devolve o número de caracteres em 'line'
     * ANTES do primeiro '\r' ou '\n' encontrado. Colocamos um '\0'
     * nessa posição para "cortar" a string ali, removendo a quebra de
     * linha (que pode ser "\n" em Linux/Mac ou "\r\n" em Windows).
     * Isso evita problemas ao comparar/exibir a string depois.
     */
    line[strcspn(line, "\r\n")] = '\0';

    /*
     * A primeira linha tem o formato: algoritmo;quantum;qtde_cpus[;alpha]
     *
     * strtok() é uma função clássica do C para "fatiar" uma string em
     * pedaços (tokens), usando um ou mais caracteres como separador.
     *
     * IMPORTANTE sobre strtok():
     *   - Na PRIMEIRA chamada, passamos a string original (line).
     *   - strtok() MODIFICA a string original, substituindo cada
     *     separador encontrado por '\0' (por isso 'line' não pode ser
     *     uma string literal constante, precisa ser um buffer editável).
     *   - Nas chamadas SEGUINTES, passamos NULL como primeiro argumento
     *     para dizer "continue de onde parou na última vez".
     */

    /* --- Campo 1: nome do algoritmo de escalonamento --- */
    char *token = strtok(line, ";");
    if (token != NULL) {
        /* Copia o token para dentro da struct Config, mas de forma seg-
         * ura: strncpy() nunca escreve mais que (MAX_ALGO - 1) bytes,
         * deixando sempre espaço para o terminador '\0' que colocamos
         * manualmente na linha seguinte (proteção contra overflow). */
        strncpy(config->algorithm, token, MAX_ALGO - 1);
        config->algorithm[MAX_ALGO - 1] = '\0';
        str_to_upper(config->algorithm); /* normaliza para maiúsculas */
    } else {
        /* Se por algum motivo não havia nem o primeiro campo, usamos
         * um valor padrão razoável em vez de deixar a struct "lixo". */
        strcpy(config->algorithm, "SRTF");
    }

    /* --- Campo 2: quantum (tempo máximo de execução contínua) --- */
    token = strtok(NULL, ";"); /* NULL = "continue fatiando a mesma linha" */
    config->quantum = (token != NULL) ? atoi(token) : 1;
    if (config->quantum < 1) config->quantum = 1; /* quantum mínimo = 1 tick */

    /* --- Campo 3: quantidade de CPUs/processadores --- */
    token = strtok(NULL, ";");
    config->cpu_count = (token != NULL) ? atoi(token) : 2;
    if (config->cpu_count < 2) config->cpu_count = 2; /* mínimo de 2 CPUs (requisito) */

    /* --- Campo 4 (opcional): alpha, fator de envelhecimento --- */
    /* Só é usado pelos algoritmos PRIOd / PRIOPEnv (prioridade dinâmica
     * com envelhecimento). Se não estiver presente no arquivo, assume 0
     * (sem envelhecimento). */
    token = strtok(NULL, ";");
    config->alpha = (token != NULL) ? atoi(token) : 0;
    if (config->alpha < 0) config->alpha = 0;

    /* ---------------------------------------------------------------
     * Linhas seguintes: uma tarefa (processo) por linha.
     *
     * O laço continua enquanto:
     *   (a) ainda conseguirmos ler uma nova linha do arquivo, E
     *   (b) ainda não atingimos o limite máximo de tarefas (MAX_TASKS).
     * --------------------------------------------------------------- */
    *task_count = 0;
    while (fgets(line, sizeof(line), file) != NULL && *task_count < MAX_TASKS) {

        /* Remove a quebra de linha, igual fizemos com a primeira linha. */
        line[strcspn(line, "\r\n")] = '\0';

        /* Linhas vazias ou que começam com '#' são tratadas como
         * comentários/espaçamento e simplesmente IGNORADAS (o laço
         * pula para a próxima iteração sem incrementar task_count). */
        if (strlen(line) == 0 || line[0] == '#') continue;

        /* 't' é um "apelido" (ponteiro) para a posição do vetor tasks[]
         * onde a tarefa atual será armazenada. Facilita a leitura do
         * código: em vez de escrever "tasks[*task_count].campo" toda
         * hora, escrevemos apenas "t->campo". */
        Task *t = &tasks[*task_count];

        /* Antes de qualquer coisa, inicializamos a lista de eventos
         * desta tarefa como "vazia" (ponteiro interno NULL, contagem
         * zero). Isso garante que, mesmo que parse_events() nunca seja
         * chamada (ex.: linha inválida), não sobra "lixo de memória"
         * dentro da struct — importante porque event_list usa alocação
         * dinâmica internamente (ver events.c/events.h). */
        event_list_init(&t->event_list);

        /*
         * Cada linha de tarefa tem o formato:
         *   id;cor;ingresso;duracao;prioridade;lista_de_eventos...
         *
         * Os 5 primeiros campos são simples (sem ';' internos), mas a
         * "lista_de_eventos" pode conter MUITOS eventos, cada um
         * potencialmente separado por ';' também. Se usássemos
         * strtok() ingenuamente em TODA a linha, ele fatiaria os
         * eventos junto com os outros campos e perderíamos a
         * informação de onde cada evento começa e termina.
         *
         * Por isso, fazemos o parsing dos 5 primeiros campos "na mão",
         * procurando cada ';' manualmente com strchr(), e SÓ DEPOIS
         * repassamos o restante da linha (a "cauda", com todos os
         * eventos) de uma vez para parse_events() (definida em
         * events.c), que sabe como interpretar aquele formato
         * específico.
         */
        char *p = line;       /* ponteiro "cursor" que caminha pela linha */
        char *campos[5] = {0}; /* vai guardar o início de cada um dos 5 campos */
        int   nc = 0;           /* quantos campos já encontramos até agora */

        /* O primeiro campo começa logo no início da linha. */
        campos[nc++] = p;

        /* Procura os próximos ';' um a um, até termos 5 campos ou até
         * a linha acabar. */
        while (nc < 5) {
            char *sep = strchr(p, ';'); /* procura o próximo ';' a partir de p */
            if (sep == NULL) {
                /* Não há mais ';' na linha: os campos restantes não
                 * existem. Marcamos p como NULL para sinalizar isso
                 * depois. */
                p = NULL;
                break;
            }
            *sep = '\0';       /* transforma o ';' em terminador de string,
                                 * "cortando" o campo anterior ali mesmo */
            p = sep + 1;        /* p passa a apontar para o caractere
                                 * logo após o ';' (início do próximo campo) */
            campos[nc++] = p;   /* guarda o início do novo campo */
        }

        /* Se a linha não tinha nem os 5 campos obrigatórios
         * (id;cor;ingresso;duracao;prioridade), ela é inválida e
         * deve ser descartada. Liberamos a lista de eventos (que
         * ainda está vazia, mas é uma boa prática sempre "desfazer"
         * o que foi inicializado antes de pular a linha) e seguimos
         * para a próxima linha do arquivo. */
        if (nc < 5) {
            event_list_free(&t->event_list);
            continue;
        }

        /*
         * Neste ponto, 'p' aponta para o que sobrou da linha depois do
         * 5º ';' — ou seja, a lista de eventos "crua" (ex.:
         * "ML01:00;IO:01-02;MU01:03"). Pode ser NULL se a linha
         * terminou exatamente no 5º campo (tarefa sem nenhum evento).
         */
        char *cauda_eventos = p;

        /* --- Campo 1: id da tarefa --- */
        /* Aceita formatos como "t03", "T10" etc., extraindo apenas a
         * parte numérica (ver extrair_id_numerico() acima). */
        t->id = extrair_id_numerico(campos[0]);

        /* --- Campo 2: cor (código hexadecimal RRGGBB, ex.: "FF0000") --- */
        if (campos[1] != NULL && campos[1][0] != '\0') {
            /* Copia no máximo 7 caracteres (6 do hex + reserva) para
             * dentro do buffer t->color, que tem 8 posições (7 + '\0'). */
            strncpy(t->color, campos[1], 7);
            t->color[7] = '\0'; /* garante terminação, por segurança */
        } else {
            /* Se a cor não foi informada, usamos um cinza neutro como
             * valor padrão, em vez de deixar a struct com lixo. */
            strcpy(t->color, "AAAAAA");
        }

        /* --- Campo 3: instante de ingresso (chegada) da tarefa --- */
        t->arrival_time = atoi(campos[2]);

        /* --- Campo 4: duração total da tarefa (em ticks de CPU) --- */
        t->duration = atoi(campos[3]);
        if (t->duration < 1) t->duration = 1; /* duração mínima de 1 tick */
        /* 'remaining_time' começa igual à duração total: é o contador
         * que vai sendo decrementado conforme a tarefa executa. */
        t->remaining_time = t->duration;

        /* --- Campo 5: prioridade estática da tarefa --- */
        t->priority = atoi(campos[4]);

        /* --- Campo 6 em diante (cauda): lista de eventos --- */
        /* Delega para parse_events() (em events.c) a responsabilidade
         * de interpretar toda a string de eventos de uma só vez e
         * preencher t->event_list com a lista já estruturada
         * (locks/unlocks de mutex e operações de E/S). */
        parse_events(cauda_eventos, &t->event_list);

        /* -----------------------------------------------------------
         * Inicialização dos campos de CONTROLE da tarefa.
         *
         * Estes campos não vêm do arquivo de configuração — eles
         * representam o "estado dinâmico" da tarefa durante a
         * simulação, e por isso precisam começar sempre com os mesmos
         * valores neutros/iniciais, independente do que foi lido.
         * ----------------------------------------------------------- */
        t->state              = NEW; /* tarefa ainda não chegou ao sistema */
        t->cpu_id             = -1;  /* não está executando em nenhuma CPU */
        t->start_time         = -1;  /* ainda não começou a executar */
        t->finish_time        = -1;  /* ainda não terminou */
        t->ticks_this_slice   = 0;   /* nenhum tick executado na fatia atual */

        /* --- Campos específicos do Projeto B (prioridade dinâmica,
         *     mutex e E/S) --- */
        t->priority_dynamic   = t->priority; /* prioridade dinâmica começa
                                               * igual à prioridade estática */
        t->ticks_in_queue     = 0;  /* ainda não esperou nenhum tick na fila
                                     * de prontos (usado no envelhecimento) */
        t->ticks_executed     = 0;  /* nenhum tick de CPU consumido ainda */
        t->mutex_waiting      = -1; /* não está esperando nenhum mutex */

        /* Mais uma tarefa foi carregada com sucesso: avança o contador. */
        (*task_count)++;
    }

    /* Terminamos de ler o arquivo: podemos fechá-lo. */
    fclose(file);

    /* Se, depois de ler o arquivo inteiro, nenhuma tarefa válida foi
     * encontrada, consideramos isso um erro de configuração (o
     * enunciado exige pelo menos uma linha de tarefa além da linha de
     * parâmetros gerais). */
    if (*task_count == 0) {
        fprintf(stderr, "Erro: arquivo '%s' nao contém tarefas.\n", filename);
        return 0;
    }

    /* Tudo certo: Config e tasks[] estão devidamente preenchidos. */
    return 1;
}
