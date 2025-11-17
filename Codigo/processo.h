#ifndef PROCESSO_H
#define PROCESSO_H

#include "so.h"

#define INI_MEM_PROC 100
#define MAX_PROCESSOS 4 //número máximo de processos

typedef enum { bloqueado, pronto, morto } estado_proc;

struct processo_t{
    int id;
    int PC;
    int A;
    int X;
    int regErro;
    err_t erro;
    int memIni;
    int memTam;
    int t_cpu;
    int n_exec;
    estado_proc estado;
    float prio;
    int id_terminal;
    int espera_terminal;
    int quantum;
};
typedef struct processo_t processo_t;

struct lista_processos {
    int id;
    float prio;
    estado_proc estado;
    struct lista_processos* prox;
};
typedef struct lista_processos Lista_processos;

#define TIPOS_ESTADOS 3

struct historico_processos {
    int id;
    int tempo_vida;     /*tempo de criacao --> tempo_vida = agora - tempo_vida*/
    int n_preempcoes;      /*Numero de vezes que foi escalonado*/
    int tempo_espera;       /*Tempo entre de espera entre desbloqueio e escalonamento - total para no final pegar media*/
    int quant_irq[TIPOS_IRQ];
    int tempo_estado[TIPOS_ESTADOS];
    int quant_estado[TIPOS_ESTADOS];
    struct historico_processos* prox;
};
typedef struct historico_processos Historico_processos;


//processo_t inicializa_init(processo_t processo);
//processo_t *inicializa_processos(processo_t processos[MAX_PROCESSOS]);
processo_t* inicializa_processo(processo_t* processo, int id, int PC, int tam);
int entrada_livre_tabela_proc(processo_t processos[MAX_PROCESSOS]);
int encontra_indice_processo(processo_t processos[MAX_PROCESSOS], int id);
void altera_estado_proc_tabela(processo_t processos[MAX_PROCESSOS], int id, estado_proc estado);
char *estado_nome(estado_proc est);

void lst_libera(Lista_processos* l);
void lst_imprime (Lista_processos* l);
Lista_processos* lst_altera_estado(Lista_processos* l, int id, estado_proc estado);
Lista_processos* lst_insere_ordenado (Lista_processos* l, int id, float prio);
Lista_processos* lst_adicionar_final(Lista_processos* l, int id, float prio);
Lista_processos* lst_retira (Lista_processos* l, int id);
Lista_processos* lst_busca(Lista_processos* l, int id);
void lst_atualiza_prioridades(Lista_processos *l);

Historico_processos* inicializa_historico_proc(int id, int tempo);
void hst_libera(Historico_processos* h);
void hst_imprime(Historico_processos* h);
int hst_vazia(Historico_processos* h);
Historico_processos* hst_insere_ordenado (Historico_processos* h, int id, int tempo);
Historico_processos* hst_retira (Historico_processos* h, int id);
Historico_processos* hst_busca(Historico_processos* h, int id);
Historico_processos* hst_atualiza_preempcoes(Historico_processos* h, int id);

#endif