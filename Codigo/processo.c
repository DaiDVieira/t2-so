#include <stdio.h>
#include <stdlib.h>
#include "processo.h"

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
    processo->regErro = 0;
    processo->erro = ERR_OK;
    processo->A = 0;
    processo->X = 0;
    processo->memIni = PC;
    processo->memTam = tam;
    processo->t_cpu = 0;
    processo->n_exec = 0;
    processo->prio = 0.0;
    processo->id_terminal = id % 4 * 4;     //0-3, 4-7, 8-11, 12-15
    processo->espera_terminal = 0;     //Sem espera = 0, Le = 1, Escreve = 2
    return processo;
}

int encontra_indice_processo(processo_t processos[MAX_PROCESSOS], int id){
    for(int i = 0; i < MAX_PROCESSOS; i++){
        if(processos[i].id == id)
            return i;
    }
    return -1;
}

void altera_estado_proc_tabela(processo_t processos[MAX_PROCESSOS], int id, estado_proc estado){
    int indice = encontra_indice_processo(processos, id);
    if(indice != -1){
        processos[indice].estado = estado;
    }
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

void lst_imprime(Lista_processos* l){
    Lista_processos* p;
    for (p = l; p != NULL; p = p->prox)
        console_printf("pid = %d prio = %.2f estado = %d\n", p->id, p->prio, p->estado);
}

int lst_vazia(Lista_processos* l){
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

Lista_processos* lst_insere_ordenado(Lista_processos* l, int id, float prio){
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
    novo->estado = pronto;
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

Lista_processos* lst_retira(Lista_processos* l, int id){
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

// ---------------------------------------------------------------------
// LISTA DE HISTORICO DE PROCESSOS
// ---------------------------------------------------------------------

Historico_processos* inicializa_historico_proc(int id, int tempo){
    Historico_processos* novo = (Historico_processos*)malloc(sizeof(Historico_processos));
    if(!novo) return NULL; 

    novo->id = id;
    novo->tempo_vida = tempo;
    novo->n_preempcoes = 0;
    novo->tempo_espera = 0;
    for(int i = 0; i < TIPOS_IRQ; i++){
        novo->quant_irq[i] = 0;
    }
    for(int i = 0; i < TIPOS_ESTADOS; i++){
        novo->tempo_estado[i] = 0;
        novo->quant_estado[i] = 0;
    }
    novo->prox = NULL;
    return novo;
} 

void hst_libera(Historico_processos* h){
    Historico_processos* p = h;
    while(p != NULL){
        Historico_processos* t = p->prox;
        free(p);
        p = t;
    }
}

void hst_imprime(Historico_processos* h){
    Historico_processos* p;
    for (p = h; p != NULL; p = p->prox)
        console_printf("pid = %d tempo de vida = %.2f preempcoes = %d\n", p->id, p->tempo_vida, p->n_preempcoes);
}

int hst_vazia(Historico_processos* h){
    return (h == NULL);
}

Historico_processos* hst_insere_ordenado (Historico_processos* h, int id, int tempo){
    Historico_processos* ant = NULL; 
    Historico_processos* p = h; /* ponteiro para percorrer a lista */
    /* procura posição de inserção */
    while (p != NULL && p->id > id){ 
        ant = p; 
        p = p->prox; 
    }
    /* cria novo elemento */
    Historico_processos* novo = inicializa_historico_proc(id, tempo);
    if(novo == NULL) return h; //retorna lista inalterada
    /* encadeia elemento */
    if (ant == NULL){
        novo->prox = h; 
        h = novo; 
    }
    else {
        novo->prox = ant->prox;
        ant->prox = novo; 
    }
    return h;
}

Historico_processos* hst_retira(Historico_processos* h, int id){
    Historico_processos* ant = NULL;
    Historico_processos* p = h;
    while (p != NULL){ 
        if (p->id == id)
            break;
        ant = p;
        p = p->prox; 
    }
    if (p == NULL)
        return h;
    if (ant == NULL){
        h = p->prox; }
    else { 
        ant->prox = p->prox; 
    }
    free(p);
    return h;
}

Historico_processos* hst_busca(Historico_processos* h, int id){
    Historico_processos* p;
    for (p = h; p != NULL; p = p->prox){
        if(p->id == id)
            return p;
    }
    return NULL;
}
