/*
 * config.h
 * ---------
 * Este é o arquivo de CABEÇALHO (header) do módulo de configuração.
 *
 * Em C, um arquivo .h normalmente NÃO contém a implementação das
 * funções (isso fica no .c correspondente, aqui "config.c"). O papel
 * do .h é declarar a "interface pública" do módulo: quais structs,
 * constantes e funções outros arquivos do programa podem enxergar e
 * usar quando fazem "#include "config.h"".
 *
 * Pense nele como o "cardápio" do módulo: mostra o que está disponível,
 * sem revelar como cada prato é preparado por trás.
 */

/* -------------------------------------------------------------------
 * Include guard (guarda de inclusão)
 *
 * Este bloco #ifndef / #define / #endif existe para evitar um problema
 * clássico em C: se este mesmo arquivo .h for incluído (via #include)
 * mais de uma vez durante a compilação de um mesmo arquivo .c — o que
 * acontece com frequência em projetos maiores, quando vários headers
 * dependem uns dos outros — o compilador tentaria "ler" as mesmas
 * declarações de novo, gerando erros de redefinição.
 *
 * Como funciona:
 *   1. Na PRIMEIRA vez que este arquivo é incluído, o símbolo CONFIG_H
 *      ainda não existe, então "#ifndef CONFIG_H" (se CONFIG_H NÃO
 *      estiver definido) é verdadeiro, e o conteúdo do arquivo é
 *      processado normalmente. A primeira linha dentro do bloco,
 *      "#define CONFIG_H", marca que este header já foi "visto".
 *   2. Em qualquer inclusão SEGUINTE do mesmo arquivo (dentro da mesma
 *      compilação), CONFIG_H já vai estar definido, então
 *      "#ifndef CONFIG_H" será falso, e todo o conteúdo até o
 *      "#endif" final é simplesmente ignorado pelo pré-processador.
 *
 * O nome do símbolo (CONFIG_H) é apenas uma convenção: geralmente usa-
 * se o nome do arquivo em maiúsculas, trocando o ponto por underscore.
 * ------------------------------------------------------------------- */
#ifndef CONFIG_H
#define CONFIG_H

/*
 * Inclui o header de Task (task.h), pois a assinatura da função
 * load_config() (mais abaixo) usa o tipo "Task" como parâmetro. Sem
 * este #include, o compilador não saberia o que é "Task" ao ler este
 * arquivo.
 */
#include "task.h"

/*
 * MAX_LINE — mantido apenas para compatibilidade histórica.
 *
 * Esta constante já foi usada no passado para limitar o tamanho máximo
 * de uma linha lida do arquivo de configuração. Hoje ela não controla
 * mais nada de fato no comportamento do programa (o tamanho real do
 * buffer de leitura é definido diretamente dentro de config.c), mas
 * foi deixada aqui para não quebrar nenhum código antigo que ainda
 * possa referenciar este nome.
 *
 * "#define" cria uma MACRO: em tempo de compilação, todo lugar do
 * código que usar o texto "MAX_LINE" é literalmente substituído pelo
 * valor 512, antes mesmo do compilador propriamente compilar o código.
 * Não é uma variável — não ocupa memória em tempo de execução.
 */
#define MAX_LINE 512

/* Tamanho máximo do nome do algoritmo de escalonamento.
 * Usado para dimensionar o array de caracteres "algorithm" dentro da
 * struct Config, logo abaixo (garante espaço suficiente para nomes
 * como "PRIOPENV" mais o caractere de terminação '\0'). */
#define MAX_ALGO 32

/* Número máximo de CPUs suportadas pela simulação.
 * Serve como limite superior para dimensionar vetores fixos (arrays)
 * que guardam informações por CPU em outras partes do programa (ex.:
 * o vetor de structs CPU no simulador). */
#define MAX_CPUS 16

/* ---------------------------------------------------------------------
 * struct Config
 *
 * Representa os PARÂMETROS GERAIS da simulação, ou seja, tudo aquilo
 * que é lido da PRIMEIRA linha do arquivo de configuração (diferente
 * das tarefas individuais, que vêm das linhas seguintes e são
 * representadas pela struct Task, definida em task.h).
 *
 * Exemplos de linhas de configuração aceitas e o que cada uma
 * significa:
 *
 *   "SRTF;5;2"
 *     -> algoritmo = SRTF (Shortest Remaining Time First)
 *        quantum   = 5
 *        cpu_count = 2
 *        alpha     = 0 (não informado; PRIOd/PRIOPEnv não é usado aqui)
 *
 *   "PRIOP;5;2"
 *     -> algoritmo = PRIOP (Prioridade Preemptiva)
 *        quantum   = 5
 *        cpu_count = 2
 *
 *   "PRIOPEnv;5;2;1"
 *     -> algoritmo = PRIOPEnv (também chamado de PRIOd: Prioridade
 *                    Dinâmica com envelhecimento)
 *        quantum   = 5
 *        cpu_count = 2
 *        alpha     = 1 (fator de envelhecimento, usado só por este
 *                    algoritmo)
 *
 * Importante: o nome do algoritmo lido do arquivo é sempre convertido
 * para MAIÚSCULAS antes de ser guardado aqui (requisito 3.3.2 do
 * enunciado: o tratamento deve ser "case-insensitive", ou seja, não
 * deve importar se o usuário escreveu "priop", "PRIOP" ou "PrioP" —
 * todos devem ser reconhecidos como o mesmo algoritmo). Essa conversão
 * é feita na implementação, dentro de config.c; aqui no header apenas
 * declaramos ONDE esse valor fica guardado.
 *
 * Nomes de algoritmo reconhecidos pelo restante do programa:
 *   FCFS, RR, SJF, SRTF, PRIOC, PRIOP, PRIOD (e o sinônimo PRIOPENV).
 * --------------------------------------------------------------------- */
typedef struct {
    /* Nome do algoritmo de escalonamento, já normalizado para
     * maiúsculas (ex.: "SRTF", "PRIOPENV"). O tamanho do array é
     * MAX_ALGO, então o nome pode ter no máximo (MAX_ALGO - 1)
     * caracteres, sobrando sempre 1 posição para o '\0' final que toda
     * string em C precisa ter. */
    char algorithm[MAX_ALGO];

    /* Quantum: quantidade máxima de "ticks" (unidades de tempo) que
     * uma tarefa pode executar de forma contínua antes de ser
     * obrigatoriamente preemptada (tirada da CPU), em algoritmos que
     * usam quantum (como o RR - Round Robin). Para algoritmos que não
     * usam quantum, este valor pode simplesmente não ter efeito. */
    int quantum;

    /* Quantidade de CPUs/processadores simulados. O enunciado exige um
     * mínimo de 2 CPUs — essa regra é garantida na hora de LER o
     * arquivo (em config.c), não aqui na struct em si. */
    int cpu_count;

    /* Fator de envelhecimento ("aging"), usado apenas pelos algoritmos
     * de prioridade dinâmica (PRIOd / PRIOPEnv). Quanto maior o alpha,
     * mais rápido a prioridade "dinâmica" de uma tarefa cresce enquanto
     * ela espera na fila de prontos — isso evita que tarefas de baixa
     * prioridade fiquem esperando para sempre (o problema clássico de
     * "inanição"/starvation). Para os demais algoritmos, este campo
     * simplesmente não é utilizado. */
    int alpha;
} Config;

/* ---------------------------------------------------------------------
 * load_config - Protótipo (assinatura) da função que lê o arquivo de
 * configuração do disco e preenche as estruturas de dados da
 * simulação.
 *
 * Este é apenas o "contrato" da função: diz quais parâmetros ela
 * recebe e o que ela devolve. A implementação de verdade — o que a
 * função efetivamente FAZ passo a passo — está no arquivo config.c.
 * Declarar a função aqui no header é o que permite que outros arquivos
 * do projeto (como main.c) chamem load_config() depois de dar
 * "#include "config.h"", mesmo sem "enxergar" o código-fonte de
 * config.c diretamente.
 *
 * Formato de arquivo esperado:
 *
 *   Linha 1  : algoritmo;quantum;qtde_cpus[;alpha]
 *              (o "[;alpha]" indica que este último campo é opcional)
 *
 *   Linha 2+ : id;cor;ingresso;duracao;prioridade[;eventos...]
 *              (uma linha para cada tarefa da simulação)
 *
 * Detalhes sobre os campos de cada tarefa:
 *   - O campo 'id' pode vir com um prefixo textual, como "t03" ou
 *     "T10" — apenas a parte numérica é efetivamente utilizada como
 *     identificador da tarefa (o prefixo é apenas "decorativo" no
 *     arquivo de entrada, para facilitar a leitura por humanos).
 *   - Tudo o que sobrar na linha após o campo "prioridade" é tratado
 *     como uma lista de eventos (solicitações/liberações de mutex e
 *     operações de E/S) e é repassado, de uma só vez, para a função
 *     parse_events() (definida em events.c/events.h), responsável por
 *     interpretar esse formato específico.
 *
 * Parâmetros:
 *   filename   - caminho (path) do arquivo de configuração a ser lido
 *   config     - ponteiro para uma struct Config já alocada pelo
 *                chamador, que será preenchida com os parâmetros
 *                gerais lidos da primeira linha
 *   tasks[]    - vetor (array) de structs Task, também já alocado pelo
 *                chamador, onde cada tarefa lida do arquivo será
 *                armazenada, uma posição por tarefa
 *   task_count - ponteiro para um inteiro que a função vai atualizar
 *                com o número TOTAL de tarefas efetivamente carregadas
 *                (ou seja, é um "parâmetro de saída": a função escreve
 *                nele, e o chamador lê o valor depois de chamar a
 *                função)
 *
 * Retorno:
 *   1 (verdadeiro) se o arquivo foi lido e interpretado com sucesso
 *   0 (falso) em caso de erro — por exemplo, arquivo inexistente,
 *     vazio, ou sem nenhuma tarefa válida
 * --------------------------------------------------------------------- */
int load_config(const char *filename, Config *config, Task tasks[], int *task_count);

#endif /* CONFIG_H */
