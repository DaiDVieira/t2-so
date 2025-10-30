#include <stdio.h>
#include <stdlib.h>
#include "processo.h"
#include "so.h"

processo_t* inicializa_processo(processo_t processo, int id, int PC, int tam){
    if (PC < 0) {
      // t2: deveria escrever no PC do descritor do processo criado
      //self->regPC = ender_carga;
      /*if((indice_proc = encontra_indice_processo(self->processos, processo)) != -1)
        self->processos[indice_proc].PC = ender_carga;*/
        return NULL;
    } // else?
    processo.id = id;
    processo.PC = PC;
    processo.erro = 0;
    processo.regErro = ERR_OK;
    processo.A = 0;
    processo.X = 0;
    processo.memIni = PC;
    processo.memTam = tam;
    processo.t_cpu = 0;
    processo.n_exec = 0;
    processo.estado = pronto;
    processo.prio = 0;
    processo.id_terminal = 0;
    return &processo;
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