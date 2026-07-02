#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "scheduler.h"

/* =======================================================================
 * INICIALIZAÇÃO
 * ======================================================================= */

/*
 * init_cpus: Prepara as CPUs virtuais quando o sistema liga.
 * Deixa todas ociosas e desocupadas.
 */
void init_cpus(CPU cpus[], int cpu_count) {
    for (int i = 0; i < cpu_count; i++) {
        cpus[i].id        = i;   /* Nomeia a CPU (0, 1, 2...) */
        cpus[i].task_id   = -1;  /* Nenhuma tarefa rodando nela (-1 = livre) */
        cpus[i].active    = 0;   /* CPU desligada/dormindo para economizar energia */
        cpus[i].idle_time = 0;   /* Zera o contador de tempo ocioso */
    }
}

/* =======================================================================
 * CLASSIFICAÇÃO: PREEMPTIVO vs COOPERATIVO
 * ======================================================================= */

/*
 * scheduler_is_preemptive: O algoritmo atual permite "arrancar" uma tarefa da CPU à força?
 * * - Preemptivo (1): O SO pode interromper uma tarefa no meio para colocar outra mais urgente.
 * - Cooperativo (0): O SO precisa "pedir com licença". A tarefa roda até acabar
 * ou até ela mesma parar (ex: para fazer leitura de disco/mutex).
 */
int scheduler_is_preemptive(const char *algorithm) {
    if (strcmp(algorithm, "RR")       == 0) return 1; /* Round Robin */
    if (strcmp(algorithm, "SRTF")     == 0) return 1; /* Shortest Remaining Time First */
    if (strcmp(algorithm, "PRIOP")    == 0) return 1; /* Prioridade Preemptivo */
    if (strcmp(algorithm, "PRIOD")    == 0) return 1; /* Prioridade Dinâmica */
    if (strcmp(algorithm, "PRIOPENV") == 0) return 1; /* Prioridade c/ Envelhecimento (Sinônimo) */
    
    /* Se não for nenhum dos acima, então é cooperativo (FCFS, SJF, PRIOC) */
    return 0;
}


/* =======================================================================
 * CRITÉRIOS DE DESEMPATE (O "Juiz")
 * ======================================================================= */

/*
 * tiebreak: Se duas tarefas têm a mesma prioridade/tempo, quem ganha a vaga na CPU?
 * Segue uma lista rígida de regras para desempatar (Requisito 4.3).
 */
static int tiebreak(Task tasks[], int idx_a, int idx_b,
                    int current_idx, int *lottery_used) {
    
    /* REGRA 1: "Quem já tá na cadeira, fica na cadeira".
     * Se uma das empatadas JÁ estava rodando na CPU, ela ganha.
     * Isso evita uma troca de contexto (context switch) desnecessária. */
    if (idx_a == current_idx) return idx_a;
    if (idx_b == current_idx) return idx_b;

    /* REGRA 2: Ordem de Chegada.
     * Quem entrou primeiro no sistema (menor arrival_time) ganha. */
    if (tasks[idx_a].arrival_time != tasks[idx_b].arrival_time)
        return (tasks[idx_a].arrival_time < tasks[idx_b].arrival_time) ? idx_a : idx_b;

    /* REGRA 3: Menor tamanho.
     * Quem for mais rápida de terminar (menor duração total), ganha. */
    if (tasks[idx_a].duration != tasks[idx_b].duration)
        return (tasks[idx_a].duration < tasks[idx_b].duration) ? idx_a : idx_b;

    /* REGRA 4: A Loteria (Cara ou Coroa).
     * Se absolutamente TUDO for igual, o SO joga a moeda pra cima. */
    if (lottery_used) *lottery_used = 1; /* Avisa o sistema que rolou sorteio */
    return (rand() % 2 == 0) ? idx_a : idx_b;
}

/*
 * tiebreak_prio_static: Desempate especial para quando usamos prioridade dinâmica.
 * Se a prioridade dinâmica (aumentada pelo envelhecimento) empatar, 
 * o primeiro critério de desempate é olhar a prioridade ORIGINAL (estática) da tarefa.
 */
static int tiebreak_prio_static(Task tasks[], int idx_a, int idx_b,
                                int current_idx, int *lottery_used) {
    if (tasks[idx_a].priority != tasks[idx_b].priority)
        return (tasks[idx_a].priority > tasks[idx_b].priority) ? idx_a : idx_b;
    
    /* Se a original também for igual, chama o juiz normal (tiebreak) */
    return tiebreak(tasks, idx_a, idx_b, current_idx, lottery_used);
}


/* =======================================================================
 * ALGORITMOS DE ESCALONAMENTO (As Regras do Jogo)
 * ======================================================================= */

/*
 * Observação: Todas as funções abaixo fazem a mesma coisa básica:
 * Olham para TODAS as tarefas que estão PRONTAS (READY) e tentam 
 * achar a "melhor" baseada numa regra específica.
 */

/* * 1. FCFS (First-Come, First-Served) / RR (Round Robin)
 * Regra: Quem chegou primeiro, leva a CPU.
 */
static int schedule_fcfs(Task tasks[], int task_count,
                         int current_idx, int *lottery_used) {
    int best = -1; /* ID da melhor tarefa encontrada até o momento */
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue; /* Só olha quem tá pronto */
        if (best == -1) { best = i; continue; }/* Pega o 1º cara que achar como referência */
        
        /* Achou alguém que chegou mais cedo? Ele vira o "melhor" */
        if (tasks[i].arrival_time < tasks[best].arrival_time) {
            best = i;
        } 
        /* Chegaram no exato mesmo momento? Chama o Juiz de desempate */
        else if (tasks[i].arrival_time == tasks[best].arrival_time) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/* * 2. SJF (Shortest Job First)
 * Regra: A tarefa mais curta (menor duração TOTAL) leva a CPU.
 */
static int schedule_sjf(Task tasks[], int task_count,
                        int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].duration < tasks[best].duration) {
            best = i;
        } else if (tasks[i].duration == tasks[best].duration) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/* * 3. SRTF (Shortest Remaining Time First)
 * Regra: A tarefa que falta MENOS TEMPO para terminar (Restante) leva a CPU.
 */
static int schedule_srtf(Task tasks[], int task_count,
                         int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].remaining_time < tasks[best].remaining_time) {
            best = i;
        } else if (tasks[i].remaining_time == tasks[best].remaining_time) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/* * 4. Prioridade Estática (PRIOc, PRIOp)
 * Regra: O "VIP" fura a fila. Maior número de prioridade ganha.
 */
static int schedule_prio_static(Task tasks[], int task_count,
                                int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].priority > tasks[best].priority) {
            best = i;
        } else if (tasks[i].priority == tasks[best].priority) {
            best = tiebreak(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/* * 5. Prioridade Dinâmica (Envelhecimento) (PRIOd)
 * Regra: Evita "Starvation" (Morte por inanição). Tarefas ignoradas há muito
 * tempo ganham "pontos de pena" (priority_dynamic) e sobem na prioridade até serem atendidas.
 */
static int schedule_prio_dynamic(Task tasks[], int task_count,
                                 int current_idx, int *lottery_used) {
    int best = -1;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != READY) continue;
        if (best == -1) { best = i; continue; }
        if (tasks[i].priority_dynamic > tasks[best].priority_dynamic) {
            best = i;
        } else if (tasks[i].priority_dynamic == tasks[best].priority_dynamic) {
            best = tiebreak_prio_static(tasks, best, i, current_idx, lottery_used);
        }
    }
    return best;
}

/*
 * O "Gerente Geral" (A função que de fato é chamada pelo resto do sistema)
 * Ela olha qual algoritmo foi escolhido pelo usuário no Menu e repassa
 * o trabalho para a função específica.
 */
int schedule(const char *algorithm, Task tasks[], int task_count,
             int current_task, int *lottery_used) {
             
    if (strcmp(algorithm, "FCFS") == 0) {
        return schedule_fcfs(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "RR") == 0) {
        /* [DICA DE OURO] O Round Robin usa a mesmíssima lógica do FCFS (Ordem de chegada).
         * O que faz ele ser o RR é o 'Quantum' arrancando a tarefa na marra,
         * o que joga ela pro FINAL da fila (isso acontece na função assign_tasks lá embaixo). */
        return schedule_fcfs(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "SJF") == 0) {
        return schedule_sjf(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "SRTF") == 0) {
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOC") == 0) {
        return schedule_prio_static(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOP") == 0) {
        return schedule_prio_static(tasks, task_count, current_task, lottery_used);
    } else if (strcmp(algorithm, "PRIOD") == 0 ||
               strcmp(algorithm, "PRIOPENV") == 0) {
        return schedule_prio_dynamic(tasks, task_count, current_task, lottery_used);
    } else {
        /* Se escreveram abobrinha no arquivo de configuração, usa o SRTF como salva-vidas */
        fprintf(stderr, "Aviso: algoritmo '%s' desconhecido. Usando SRTF.\n", algorithm);
        return schedule_srtf(tasks, task_count, current_task, lottery_used);
    }
}


/* =======================================================================
 * DISTRIBUIÇÃO DE TAREFAS (Colocando as bundas nas cadeiras das CPUs)
 * ======================================================================= */

/*
 * assign_tasks: Onde a mágica REAL acontece. 
 * É chamada a cada 'tick' do relógio para verificar se as CPUs estão trabalhando 
 * com as tarefas corretas, se alguém precisa ser expulso (Quantum), ou promovido.
 */
int assign_tasks(const char *algorithm, Task tasks[], int task_count,
                 CPU cpus[], int cpu_count, int quantum, int tick, int alpha) {
    (void)tick; /* 'tick' não é usado aqui, evitamos warning do compilador */

    int global_lottery  = 0;
    int is_preemptive   = scheduler_is_preemptive(algorithm);
    int is_prio_dynamic = (strcmp(algorithm, "PRIOD") == 0 ||
                           strcmp(algorithm, "PRIOPENV") == 0);

    /* -------------------------------------------------------------
     * PASSO 0: ENVELHECIMENTO (Só se o algoritmo for Dinâmico)
     * ------------------------------------------------------------- */
    if (is_prio_dynamic) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == READY) {
                tasks[i].ticks_in_queue++; /* Mais um ciclo perdendo a vez... */
                /* Fórmula do envelhecimento: Prioridade Base + (Peso do Alpha * Tempo na fila) */
                tasks[i].priority_dynamic = tasks[i].priority
                                            + alpha * tasks[i].ticks_in_queue;
            }
        }
    }

    /* -------------------------------------------------------------
     * PASSO 1: EXPULSÃO POR FIM DE QUANTUM (Time Slice Expirado)
     * ------------------------------------------------------------- 
     * Se o algoritmo for preemptivo, verificamos se alguma tarefa
     * já gastou toda a sua "cota de tempo" (quantum) na CPU.
     */
    if (is_preemptive) {
        for (int i = 0; i < task_count; i++) {
            /* Tarefa tá rodando E já bateu o tempo limite do Quantum? */
            if (tasks[i].state == RUNNING &&
                tasks[i].ticks_this_slice >= quantum) {
                
                tasks[i].state            = READY; /* Volta pro fim da fila de Prontas */
                tasks[i].cpu_id           = -1;    /* Perde a CPU */
                tasks[i].ticks_this_slice = 0;     /* Zera o cronômetro dela */
                
                /* Se for prioridade dinâmica, ao perder a CPU ela perde os "pontos de pena"
                 * e volta para a sua prioridade humilde original. */
                if (is_prio_dynamic) {
                    tasks[i].ticks_in_queue   = 0;
                    tasks[i].priority_dynamic = tasks[i].priority;
                }
            }
        }
    }

    /* -------------------------------------------------------------
     * PASSO 2: ESCOLHER QUEM VAI RODAR EM CADA CPU
     * ------------------------------------------------------------- */
    for (int c = 0; c < cpu_count; c++) {

        /*
         * [BLOQUEIO COOPERATIVO]
         * Se o algoritmo é "educado" (não-preemptivo), a CPU está ativa e já
         * tem alguém lá... a gente pula essa CPU! Deixa o cara terminar em paz.
         */
        if (!is_preemptive && cpus[c].active && cpus[c].task_id != -1) {
            int still_running = 0;
            /* Verifica se a tarefa dona dessa CPU realmente ainda está rodando
             * (ela poderia ter se bloqueado pedindo um E/S no tick passado). */
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].id == cpus[c].task_id &&
                    tasks[i].state == RUNNING) {
                    still_running = 1;
                    break;
                }
            }
            if (still_running) continue; /* Mantém a tarefa lá e vai pra próxima CPU */
        }

        /* * Se chegamos aqui, precisamos decidir quem botar na CPU.
         * Primeiro: Quem estava nessa CPU até 1 milissegundo atrás?
         * Anotamos o ID dela ('current_idx') porque ela tem preferência no desempate (Regra 1).
         */
        int current_idx = -1;
        if (cpus[c].task_id != -1) {
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].id == cpus[c].task_id &&
                    (tasks[i].state == READY || tasks[i].state == RUNNING)) {
                    current_idx = i;
                    break;
                }
            }
        }

        /* * TRUQUE DE MÁGICA DO ESCALONADOR:
         * Para os algoritmos conseguirem comparar TUDO (quem tá esperando x quem tá rodando),
         * nós fingimos por um segundo que a tarefa que está rodando nesta CPU voltou
         * para a fila (READY). Assim ela concorre novamente pela vaga em pé de igualdade.
         */
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == RUNNING && tasks[i].cpu_id == cpus[c].id) {
                tasks[i].state = READY;
            }
        }

        /* Chama o "Gerente" que vai chamar a função específica (FCFS, SJF, etc) 
         * e nos devolver a tarefa vencedora! */
        int lottery_used = 0;
        int chosen_idx   = schedule(algorithm, tasks, task_count,
                                    current_idx, &lottery_used);
                                    
        if (lottery_used) global_lottery = 1; /* Alguém precisou jogar moeda hoje */

        /* -------------------------------------------------------------
         * PASSO 3: EFETIVAR A POSSE DA CPU
         * ------------------------------------------------------------- */
        if (chosen_idx == -1) {
            /* Ninguém quer rodar. Ninguém na fila. Desliga a CPU pra poupar luz. */
            cpus[c].task_id = -1;
            cpus[c].active  = 0;
        } else {
            /* Temos um vencedor! */
            int same_task = (cpus[c].task_id == tasks[chosen_idx].id);

            cpus[c].task_id          = tasks[chosen_idx].id; /* CPU anota quem sentou nela */
            cpus[c].active           = 1;                    /* Liga a CPU */
            tasks[chosen_idx].state  = RUNNING;              /* Vencedor entra no estado Executando */
            tasks[chosen_idx].cpu_id = c;                    /* Vencedor anota em qual cadeira sentou */

            /* Se rolou uma troca de tarefas (um saiu, outro entrou),
             * zera o cronômetro de fatias (Quantum) do cara que acabou de entrar. */
            if (!same_task) tasks[chosen_idx].ticks_this_slice = 0;

            /* Se a tarefa sentou na CPU, ela perde os privilégios acumulados
             * de envelhecimento na fila. */
            if (is_prio_dynamic) {
                tasks[chosen_idx].ticks_in_queue   = 0;
                tasks[chosen_idx].priority_dynamic = tasks[chosen_idx].priority;
            }
        }
    }

    return global_lottery; /* Retorna se rolou sorteio geral, útil pro Gantt desenhar a moeda */
}
