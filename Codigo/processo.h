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
    int memIni;
    int memTam;
    int t_cpu;
    int n_exec;
    estado_proc estado;
    float prio;
    int id_terminal;
};
typedef struct processo_t processo_t;

struct lista_processos {
    int id;
    float prio;
    estado_proc estado;
    struct lista_processos* prox;
};
typedef struct lista_processos Lista_processos;

//processo_t inicializa_init(processo_t processo);
//processo_t *inicializa_processos(processo_t processos[MAX_PROCESSOS]);
processo_t* inicializa_processo(processo_t* processo, int id, int PC, int tam);
int encontra_indice_processo(processo_t processos[MAX_PROCESSOS], int id);

void lst_libera(Lista_processos* l);
void lst_imprime (Lista_processos* l);
Lista_processos* lst_altera_estado(Lista_processos* l, int id, estado_proc estado);
Lista_processos* lst_insere_ordenado (Lista_processos* l, int id, float prio, estado_proc estado);
Lista_processos* lst_retira (Lista_processos* l, int id);
void lst_atualiza_prioridades(Lista_processos *l);

#endif