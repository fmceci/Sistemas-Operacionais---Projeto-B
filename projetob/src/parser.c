#include <stdio.h>
#include "task.h"

// Função responsável por carregar tarefas de um arquivo
// tarefas[] → vetor onde as tarefas serão armazenadas
// max → número máximo de tarefas que podem ser lidas
int carregar_tarefas(Task tarefas[], int max) {


    FILE *f = fopen("config/entrada.txt", "r");

    // Verifica se o arquivo foi aberto corretamente
    if (!f) {
        printf("Erro ao abrir arquivo\n");
        return 0;
    }

    char linha[256]; // Buffer para armazenar cada linha do arquivo


    fgets(linha, sizeof(linha), f);

    int count = 0;

    // Loop para ler cada linha do arquivo até o fim ou até atingir o limite
    while (fgets(linha, sizeof(linha), f) && count < max) {

        sscanf(linha, "%d;%*[^;];%d;%d;%d",
            &tarefas[count].id,          // ID da tarefa
            &tarefas[count].ingresso,    // Tempo de chegada
            &tarefas[count].duracao,     // Tempo total de execução
            &tarefas[count].prioridade   // Prioridade da tarefa
        );

        // Inicializa o tempo restante igual à duração total
        tarefas[count].restante = tarefas[count].duracao;

        count++;
    }

    fclose(f);

    // Retorna o número de tarefas carregadas
    return count;
}