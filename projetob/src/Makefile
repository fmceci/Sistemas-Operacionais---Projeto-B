# Makefile - Projeto B
# Simulador de SO Multitarefa com Mutex, E/S e escalonadores diversos

CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11
TARGET  = simulador

# Arquivos-fonte do projeto (parser.c REMOVIDO: era codigo morto com
# caminho hardcoded; o carregamento real e feito por config.c).
SRCS = main.c \
       config.c \
       task.c \
       events.c \
       mutex.c \
       io.c \
       scheduler.c \
       simulation.c \
       gantt.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean run run_passo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Dependencias explicitas
main.o:       main.c config.h task.h events.h simulation.h scheduler.h gantt.h
config.o:     config.c config.h task.h events.h
task.o:       task.c task.h events.h
events.o:     events.c events.h
mutex.o:      mutex.c mutex.h task.h
io.o:         io.c io.h
scheduler.o:  scheduler.c scheduler.h task.h config.h
simulation.o: simulation.c simulation.h task.h config.h scheduler.h gantt.h mutex.h io.h
gantt.o:      gantt.c gantt.h task.h scheduler.h config.h

# Atalhos de execucao
run: $(TARGET)
	./$(TARGET).exe ../entrada.txt

run_passo: $(TARGET)
	./$(TARGET).exe ../entrada.txt passo
clean:
	rm -f $(OBJS) $(TARGET) gantt.svg
