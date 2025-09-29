#include <stdio.h>
#include <stdlib.h>
#include "processo.h"
#include "so.h"

#define INI_MEM_PROC 100

processo_t inicializa_processo(processo_t processo, int id){
    processo.id = id;
    processo.PC = 0;
    processo.A = 0;
    processo.X = 0;
    //processos[i]->memIni = 
    //processos[i]->memFim = 
    processo.t_cpu = 0;
    processo.n_exec = 0;
    processo.estado = pronto;
    processo.prio = 0;
    return processo;
}

processo_t *inicializa_processos(processo_t processos[MAX_PROCESSOS]){
    for(int i = 1; i < MAX_PROCESSOS; i++){
        inicializa_processo(processos[i], i);
    }
    return processos;
}