#include "processo.h"
#include "so.h"

#define INI_MEM_PROC 100

struct processo_t{
    int id;
    int PC;
    int A;
    int X;
    int* memIni;
    int* memFim;
    int t_cpu;
    int n_exec;
    estado_proc estado;
    float prio;
};

processo_t inicializa_processos(processo_t *processos){
    for(int i = 1; i < MAX_PROCESSOS; i++){
        processos[i]->id = i;
        processos[i]->A = 0;
        processos[i]->X = 0;
        //processos[i]->memIni = 
        //processos[i]->memFim = 
        processos[i]->t_cpu = 0;
        processos[i]->n_exec = 0;
        processos[i]->estado = vivo;
        processos[i]->prio = 0;

    }
}