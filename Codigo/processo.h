#ifndef PROCESSO_H
#define PROCESSO_H

#define INI_MEM_PROC 100
#define MAX_PROCESSOS 3 //número máximo de processos

typedef enum { executando, bloqueado, pronto, morto } estado_proc;

struct processo_t{
    int id;
    int PC;
    int A;
    int X;
    int erro;
    err_t regErro;
    int* memIni;
    int* memTam;
    int t_cpu;
    int n_exec;
    estado_proc estado;
    float prio;
    int id_terminal;
};

typedef struct processo_t processo_t;

//processo_t inicializa_init(processo_t processo);
//processo_t *inicializa_processos(processo_t processos[MAX_PROCESSOS]);
processo_t* inicializa_processo(processo_t processo, int id, int PC, int tam);
int encontra_indice_processo(processo_t processos[MAX_PROCESSOS], processo_t processo);

#endif