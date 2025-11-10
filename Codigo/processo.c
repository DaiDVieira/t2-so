#include <stdio.h>
#include <stdlib.h>
#include "processo.h"
#include "so.h"

processo_t* inicializa_processo(processo_t* processo, int id, int PC, int tam){
    if (PC < 0) {
      // t2: deveria escrever no PC do descritor do processo criado
      //self->regPC = ender_carga;
      /*if((indice_proc = encontra_indice_processo(self->processos, processo)) != -1)
        self->processos[indice_proc].PC = ender_carga;*/
        return NULL;
    } // else?

    processo->id = id;
    processo->PC = PC;
    processo->erro = 0;
    processo->regErro = ERR_OK;
    processo->A = 0;
    processo->X = 0;
    processo->memIni = PC;
    processo->memTam = tam;
    processo->t_cpu = 0;
    processo->n_exec = 0;
    processo->prio = 0.0;
    processo->id_terminal = 0;
    return processo;
}

int encontra_indice_processo(processo_t processos[MAX_PROCESSOS], int id){
    for(int i = 0; i < MAX_PROCESSOS; i++){
        if(processos[i].id == id)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------
// LISTA DE PROCESSOS
// ---------------------------------------------------------------------

void lst_libera(Lista_processos* l){
    Lista_processos* p = l;
    while(p != NULL){
        Lista_processos* t = p->prox;
        free(p);
        p = t;
    }
}

void lst_imprime (Lista_processos* l){
    Lista_processos* p;
    for (p = l; p != NULL; p = p->prox)
        printf("pid = %d prio = %.2f estado = %d\n", p->id, p->prio, p->estado);
}

int lst_vazia (Lista_processos* l){
    return (l == NULL);
}

Lista_processos* lst_altera_estado(Lista_processos* l, int id, estado_proc estado){
    Lista_processos* p;
    for(p = l; p != NULL; p = p->prox){
        if(p->id == id){
            p->estado = estado;
            return l;
        }
    }
    return l; //id invalido, lista inalterada
}

Lista_processos* lst_insere_ordenado (Lista_processos* l, int id, float prio, estado_proc estado){
    Lista_processos* novo;
    Lista_processos* ant = NULL; /* ponteiro para elemento anterior */
    Lista_processos* p = l; /* ponteiro para percorrer a lista */
    /* procura posição de inserção */
    while (p != NULL && p->prio > prio){ 
        ant = p; 
        p = p->prox; 
    }
    /* cria novo elemento */
    novo = (Lista_processos*)malloc(sizeof(Lista_processos));
    novo->id = id;
    novo->prio = prio;
    novo->estado = estado;
    /* encadeia elemento */
    if (ant == NULL){
        novo->prox = l; 
        l = novo; 
    }
    else {
        novo->prox = ant->prox;
        ant->prox = novo; 
    }
    return l;
}

Lista_processos* lst_retira (Lista_processos* l, int id)
{
    Lista_processos* ant = NULL;
    Lista_processos* p = l;
    while (p != NULL){ 
        if (p->id == id || p->estado == morto)
            break;
        ant = p;
        p = p->prox; 
    }
    if (p == NULL)
        return l;
    if (ant == NULL){
        l = p->prox; }
    else { 
        ant->prox = p->prox; 
    }
    free(p);
    return l;
}
