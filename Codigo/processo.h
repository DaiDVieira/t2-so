#ifndef PROCESSO_H
#define PROCESSO_H

#define MAX_PROCESSOS 3 //número máximo de processos

typedef enum { executando, bloqueado, pronto, morto } estado_proc;

struct processo_t{
    int id;
    int PC;
    int A;
    int X;
    int* memIni;
    int* memTam;
    int t_cpu;
    int n_exec;
    estado_proc estado;
    float prio;
};

typedef struct processo_t processo_t;

processo_t inicializa_processo(processo_t processo, int id);
processo_t *inicializa_processos(processo_t processos[MAX_PROCESSOS]);

#endif