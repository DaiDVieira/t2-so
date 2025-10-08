#include <stdio.h>
#include <stdlib.h>
#include "processo.h"
#include "so.h"

processo_t inicializa_processo(processo_t processo, int id, int PC, int tam){
    processo.id = 0;
    processo.PC = PC;
    processo.A = 0;
    processo.X = 0;
    processo.memIni = PC;
    processo.memTam = tam;
    processo.t_cpu = 0;
    processo.n_exec = 0;
    processo.estado = pronto;
    processo.prio = 0;
    processo.id_terminal = 0;
    return processo;
}

int encontra_indice_processo(processo_t processos[MAX_PROCESSOS], processo_t processo){
    for(int i = 0; i <= MAX_PROCESSOS; i++){
        if(processos[i].id == processo.id)
            return i;
    }
    return -1;
}


/*processo_t *inicializa_processos(processo_t processos[MAX_PROCESSOS]){
    for(int i = 1; i <= MAX_PROCESSOS; i++){
        inicializa_processo(processos[i], i);
    }
    return processos;
}*/