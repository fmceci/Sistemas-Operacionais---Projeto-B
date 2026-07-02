#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "simulation.h"

/*
 * Se a simulação passar STALL_LIMIT ticks consecutivos sem progresso
 * real, assumimos uma situação de travamento.
 *
 * "Sem progresso" aqui significa:
 * - nenhuma CPU executou tarefa neste tick;
 * - não há nenhuma E/S pendente que possa desbloquear tarefas.
 *
 * Isso ajuda a detectar deadlock, por exemplo quando uma tarefa
 * adquire um mutex e nunca o libera.
 */
#define STALL_LIMIT 1000

/* -----------------------------------------------------------------------
 * Entrada de dados
 * ----------------------------------------------------------------------- */

/*
 * limpa_buffer:
 * Consome caracteres restantes até o fim da linha.
 *
 * É útil depois de usar scanf(), para evitar que sobrem '\n' ou lixo
 * no buffer de entrada e interfiram na leitura seguinte.
 */
void limpar_buffer(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}

/*
 * lê um inteiro com valor padrão.
 *
 * Se o usuário apenas apertar Enter, o valor padrão 
