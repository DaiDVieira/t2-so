// so.c
// sistema operacional
// simulador de computador
// so25b

// ---------------------------------------------------------------------
// INCLUDES {{{1
// ---------------------------------------------------------------------

#include "so.h"
#include "dispositivos.h"
#include "err.h"
#include "irq.h"
#include "memoria.h"
#include "programa.h"
#include "cpu.h"
#include "processo.h"

#include <stdlib.h>
#include <stdbool.h>


// ---------------------------------------------------------------------
// CONSTANTES E TIPOS {{{1
// ---------------------------------------------------------------------

// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas
#define TERMINAIS 4

struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  bool erro_interno;

  int regA, regX, regPC, regERRO; // cópia do estado da CPU
  // t2: tabela de processos, processo corrente, pendências, etc
  processo_t processos[MAX_PROCESSOS];
  processo_t *processo_corrente;
  Lista_processos* ini_fila_proc;
  Lista_processos* ini_fila_proc_prontos;
  int cont_processos; 
  bool dispositivos_livres[TERMINAIS]; 
  Historico_processos* ini_hist_proc;
  int tempo_total_execucao;
  int tempo_ocioso_total;
  int momento_sist_ocioso;   /*Tempo que o sistema atualmente esta ocioso, antes de somar no total*/
  int n_preempcoes;      /*Numero total de vencimentos do quantum*/
  int quant_irq[TIPOS_IRQ+1];   /*Considerando a Interrupção Desconhecida*/
  escalonador_atual escalonador;
};

// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
// carrega o programa contido no arquivo na memória do processador; retorna end. inicial
static programa_t* so_carrega_programa(so_t *self, char *nome_do_executavel);
// copia para str da memória do processador, até copiar um 0 (retorna true) ou tam bytes
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);


// ---------------------------------------------------------------------
// CRIAÇÃO {{{1
// ---------------------------------------------------------------------

so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->es = es;
  self->console = console;
  self->erro_interno = false;

  self->cont_processos = 0;
  self->ini_fila_proc = NULL;
  self->processo_corrente = NULL;
  self->ini_hist_proc = NULL;
  self->ini_fila_proc_prontos = NULL;
  for(int i = 0; i < MAX_PROCESSOS; i++){
    self->processos[i].estado = morto;
  }

  for(int i = 0; i < TERMINAIS; i++){
    self->dispositivos_livres[i] = true;
  }
  es_le(self->es, D_RELOGIO_REAL, &self->tempo_total_execucao);   /*tempo inicial do relogio*/
  self->tempo_ocioso_total = 0;
  self->momento_sist_ocioso = 0;
  self->n_preempcoes = 0;
  for(int i = 0; i < TIPOS_IRQ+1; i++){
    self->quant_irq[i] = 0;
  }
  self->escalonador = simples;

  // quando a CPU executar uma instrução CHAMAC, deve chamar a função
  //   so_trata_interrupcao, com primeiro argumento um ptr para o SO
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}


// ---------------------------------------------------------------------
// TRATAMENTO DE INTERRUPÇÃO {{{1
// ---------------------------------------------------------------------

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);

// função a ser chamada pela CPU quando executa a instrução CHAMAC, no tratador de
//   interrupção em assembly
// essa é a única forma de entrada no SO depois da inicialização
// na inicialização do SO, a CPU foi programada para chamar esta função para executar
//   a instrução CHAMAC
// a instrução CHAMAC só deve ser executada pelo tratador de interrupção
//
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// o valor retornado por esta função é colocado no registrador A, e pode ser
//   testado pelo código que está após o CHAMAC. No tratador de interrupção em
//   assembly esse valor é usado para decidir se a CPU deve retornar da interrupção
//   (e executar o código de usuário) ou executar PARA e ficar suspensa até receber
//   outra interrupção
static int so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;
  // esse print polui bastante, recomendo tirar quando estiver com mais confiança
  console_printf("SO: recebi IRQ %d (%s)", irq, irq_nome(irq));
  // salva o estado da cpu no descritor do processo que foi interrompido
  so_salva_estado_da_cpu(self);
  // faz o atendimento da interrupção
  so_trata_irq(self, irq);
  // faz o processamento independente da interrupção
  so_trata_pendencias(self);
  // escolhe o próximo processo a executar
  so_escalona(self);
  // recupera o estado do processo escolhido
  return so_despacha(self);
}

static void so_salva_estado_da_cpu(so_t *self)    /*Feito*/
{
  // t2: salva os registradores que compõem o estado da cpu no descritor do
  //   processo corrente. os valores dos registradores foram colocados pela
  //   CPU na memória, nos endereços CPU_END_PC etc. O registrador X foi salvo
  //   pelo tratador de interrupção (ver trata_irq.asm) no endereço 59
  // se não houver processo corrente, não faz nada

  if(self->processo_corrente != NULL && self->processo_corrente->estado != morto){
    if (mem_le(self->mem, CPU_END_A, &self->processo_corrente->A) != ERR_OK
        || mem_le(self->mem, CPU_END_PC, &self->processo_corrente->PC) != ERR_OK
        || mem_le(self->mem, CPU_END_erro, &self->processo_corrente->regErro) != ERR_OK
        || mem_le(self->mem, 59, &self->processo_corrente->X) != ERR_OK) {
      console_printf("SO: erro na leitura dos registradores");
      self->erro_interno = true;
    }
  }
}

/*Funções chamadas por so_trata_pendencias*/
processo_t* so_proximo_pendente(so_t* self);
Lista_processos* so_coloca_fila_pronto(so_t* self, processo_t* processo);
static void so_chamada_espera_proc(so_t *self, processo_t* processo_pendente);
static void so_muda_estado_processo(so_t* self, int id_proc, estado_proc est);

static void so_trata_pendencias(so_t *self)
{
  // t2: realiza ações que não são diretamente ligadas com a interrupção que
  //   está sendo atendida:
  // - E/S pendente
  // - desbloqueio de processos
  // - contabilidades
  // - etc
  /*se esta usando terminal ou nao alocou/precisa terminal*/
  if(self->processo_corrente != NULL || self->processo_corrente->estado == bloqueado || self->processo_corrente->espera_terminal == 0)
      self->dispositivos_livres[self->processo_corrente->id_terminal/4] = true;
  /*E/S pendente*/
  processo_t* processo_pendente = NULL;
  while((processo_pendente = so_proximo_pendente(self)) != NULL){   /*Enquanto tiver processos pendentes*/
    console_printf("(depois while processo pendente)");
    if(processo_pendente->espera_terminal == 1){ 
      int teclado_ok = processo_pendente->id_terminal + TERM_TECLADO_OK;
      console_printf("verifica espera_terminal=1");
      if((es_le(self->es, teclado_ok, &processo_pendente->A)) == ERR_OK && self->dispositivos_livres[processo_pendente->id_terminal / 4]){
        /*altera_estado_proc_tabela(self->processos, processo_pendente->id, pronto);
        self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, processo_pendente->id, pronto);
        self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, processo_pendente);*/
        so_muda_estado_processo(self, processo_pendente->id, pronto);
      }
      else{
        console_printf("SO: teclado nao disponivel"); /*retirar depois - depuracao*/
      }
    }
    else if(processo_pendente->espera_terminal == 2){
      int tela_ok = processo_pendente->id_terminal + TERM_TELA_OK;
      console_printf("espera_terminal=2 erro: %d", tela_ok);
      if((es_le(self->es, tela_ok, &processo_pendente->A)) == ERR_OK && self->dispositivos_livres[processo_pendente->id_terminal / 4]){
        /*altera_estado_proc_tabela(self->processos, processo_pendente->id, pronto);
        self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, processo_pendente->id, pronto);
        self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, processo_pendente);*/
        so_muda_estado_processo(self, processo_pendente->id, pronto);
      }
      else{
        console_printf("SO: tela nao disponivel"); /*retirar depois - depuracao*/
      }
    }
    /*else{   //espera_terminal = 0, pode ser por esperar outro processo acabar
      //Desbloqueio de processos bloqueados por espera
      so_chamada_espera_proc(self, processo_pendente);
    }*/
  }

  /*bloqueia processos por tempo de cpu e reinicia o quantum*/
  if(self->escalonador != simples){
    if(self->processo_corrente != NULL && self->processo_corrente->quantum == 0){
      if(self->ini_fila_proc_prontos != NULL){
        /*self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processo_corrente->id, bloqueado);
        altera_estado_proc_tabela(self->processos, self->processo_corrente->id, bloqueado);*/
        self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id);
        console_printf("(escalonador = %d)", self->escalonador);
        self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, self->processo_corrente);
        self->ini_hist_proc = hst_atualiza_preempcoes(self->ini_hist_proc, self->processo_corrente->id);
      }
      self->processo_corrente->quantum = QUANTUM_INICIAL;
    }
  }

  /*Contabilidades - metricas: */

}

//funcao auxiliar temporaria para escalonamento
processo_t* so_proximo_pronto(so_t* self);
static void so_calcula_tempo_ocioso(so_t* self);

static void so_escalona(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
  // t2: na primeira versão, escolhe um processo pronto caso o processo
  //   corrente não possa continuar executando, senão deixa o mesmo processo.
  //   depois, implementa um escalonador melhor
  console_printf("(so_escalona)");
  if(self->processo_corrente != NULL && self->ini_fila_proc_prontos != NULL && self->escalonador == prioridade){
    if(self->processo_corrente->prio < self->ini_fila_proc_prontos->prio){
      self->ini_hist_proc = hst_atualiza_preempcoes(self->ini_hist_proc, self->processo_corrente->id);
      /*self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id); 
      self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processo_corrente->id, bloqueado);
      altera_estado_proc_tabela(self->processos, self->processo_corrente->id, bloqueado);
      so_muda_estado_processo(self, self->processo_corrente->id, bloqueado);*/
      self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id); 
      self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, self->processo_corrente);
    }
  }

  if(self->processo_corrente ==  NULL || self->processo_corrente->estado != pronto){
    processo_t* prox_processo = so_proximo_pronto(self);
    if(prox_processo != NULL)
      console_printf("(prox_proc_id %d)", prox_processo->id);
    else
      console_printf("(prox_proc eh nulo)");
    if(prox_processo != NULL && prox_processo->espera_terminal != 0){
      self->dispositivos_livres[prox_processo->id_terminal/4] = false;
      prox_processo->espera_terminal = 0;
    }
    self->processo_corrente = prox_processo; //pode ser NULL
  }
    
  //console_printf("id proc_corrente %d ", self->processo_corrente->id);
}

static int so_despacha(so_t *self)  /*Feito*/
{
  // t2: se houver processo corrente, coloca o estado desse processo onde ele
  //   será recuperado pela CPU (em CPU_END_PC etc e 59) e retorna 0,
  //   senão retorna 1
  // o valor retornado será o valor de retorno de CHAMAC, e será colocado no 
  //   registrador A para o tratador de interrupção (ver trata_irq.asm).

  /*Calculo do tempo ocioso*/
  so_calcula_tempo_ocioso(self);

  if(self->processo_corrente != NULL){
    if(mem_escreve(self->mem, CPU_END_A, self->processo_corrente->A) != ERR_OK
      || mem_escreve(self->mem, CPU_END_PC, self->processo_corrente->PC) != ERR_OK
      || mem_escreve(self->mem, CPU_END_erro, self->processo_corrente->regErro) != ERR_OK
      || mem_escreve(self->mem, 59, self->processo_corrente->X) != ERR_OK) {
      console_printf("SO: erro na escrita dos registradores do processo %d.", self->processo_corrente->id);
      self->erro_interno = true;
      return 1;
    }
    //console_printf("valor de A proc_corrente %d ", self->processo_corrente->A);
    return 0;
  }
  else{
    return 1;
  }

  //if (self->erro_interno) return 1;
  //else return 0;
}


// ---------------------------------------------------------------------
// TRATAMENTO DE UMA IRQ {{{1
// ---------------------------------------------------------------------

// funções auxiliares para tratar cada tipo de interrupção
static void so_trata_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  // verifica o tipo de interrupção que está acontecendo, e atende de acordo
  //console_printf("(trata_irq com irq %d)", irq);
  Historico_processos* h = NULL;
  if(self->processo_corrente != NULL){
    console_printf("(irq proc_id %d)", self->processo_corrente->id);
    h = hst_busca(self->ini_hist_proc, self->processo_corrente->id);
  }
  switch (irq) {
    case IRQ_RESET:
      so_trata_reset(self);
      if(h != NULL)
        h->quant_irq[IRQ_RESET]++;
      self->quant_irq[IRQ_RESET]++;
      break;
    case IRQ_SISTEMA:
      so_trata_irq_chamada_sistema(self);
      if(h != NULL)
        h->quant_irq[IRQ_SISTEMA]++;
      self->quant_irq[IRQ_SISTEMA]++;
      break;
    case IRQ_ERR_CPU:
      so_trata_irq_err_cpu(self);
      if(h != NULL)
        h->quant_irq[IRQ_ERR_CPU]++;
      self->quant_irq[IRQ_ERR_CPU]++;
      break;
    case IRQ_RELOGIO:
      so_trata_irq_relogio(self);
      if(h != NULL)
        h->quant_irq[IRQ_RELOGIO]++;
      self->quant_irq[IRQ_RELOGIO]++;
      break;
    default:
      so_trata_irq_desconhecida(self, irq);
      self->quant_irq[TIPOS_IRQ]++;
  }
  //console_printf("fim trata_irq com irq %d", irq);
}

processo_t* so_cria_entrada_processo(so_t* self, int PC, int tam);

// chamada uma única vez, quando a CPU inicializa
static void so_trata_reset(so_t *self)
{
  // coloca o tratador de interrupção na memória
  // quando a CPU aceita uma interrupção, passa para modo supervisor,
  //   salva seu estado à partir do endereço CPU_END_PC, e desvia para o
  //   endereço CPU_END_TRATADOR
  // colocamos no endereço CPU_END_TRATADOR o programa de tratamento
  //   de interrupção (escrito em asm). esse programa deve conter a
  //   instrução CHAMAC, que vai chamar so_trata_interrupcao (como
  //   foi definido na inicialização do SO)
  /*int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != CPU_END_TRATADOR) {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }*/
  programa_t *prog = so_carrega_programa(self, "trata_int.maq"); 
  if (prog_end_carga(prog) != CPU_END_TRATADOR) {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }
  prog_destroi(prog);

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK) {
    console_printf("SO: problema na programação do timer");
    self->erro_interno = true;
  }

  // t2: deveria criar um processo para o init, e inicializar o estado do
  //   processador para esse processo com os registradores zerados, exceto
  //   o PC e o modo.
  // como não tem suporte a processos, está carregando os valores dos
  //   registradores diretamente no estado da CPU mantido pelo SO; daí vai
  //   copiar para o início da memória pelo despachante, de onde a CPU vai
  //   carregar para os seus registradores quando executar a instrução RETI
  //   em bios.asm (que é onde está a instrução CHAMAC que causou a execução
  //   deste código
  // coloca o programa init na memória
  programa_t *proginit = so_carrega_programa(self, "init.maq");
  console_printf("endereco carga init %d", prog_end_carga(proginit));
  //processo_t *init = inicializa_processo(init, 0, prog_end_carga(proginit), prog_tamanho(proginit), pronto);
  processo_t *init = so_cria_entrada_processo(self, prog_end_carga(proginit), prog_tamanho(proginit));
  //console_printf("(init id_terminal %d)", init->id_terminal);
  prog_destroi(proginit);

  //atualiza processo corrente e coloca init na fila de processos
  if (init != NULL) {
      self->processo_corrente = init;
      init->estado = pronto;
      self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, init);
      self->ini_fila_proc = lst_insere_ordenado(self->ini_fila_proc, init->id, init->prio);
  }

  console_printf("(init id_terminal %d)", self->processo_corrente->id_terminal);
  int tempo;
  es_le(self->es, D_RELOGIO_REAL, &tempo);
  self->ini_hist_proc = hst_insere_ordenado(self->ini_hist_proc, init->id, tempo);

  //self->cont_processos++;     /*a quantidade de processos vira 1*/

  //self->cont_processos = 0; //ja inicializado em so_cria
  //self->processos[self->cont_processos] = *init;       /*guarda dados do processo criado no SO*/
  //self->processo_corrente = &self->processos[self->cont_processos];
  //self->processo_corrente = init;

  // altera o PC para o endereço de carga
  //self->regPC = ender; // deveria ser no processo 
  /*--> inicializa_processo() ja atribuiu o endereco*/
}

// interrupção gerada quando a CPU identifica um erro
static void so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em CPU_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  // t2: com suporte a processos, deveria pegar o valor do registrador erro
  //   no descritor do processo corrente, e reagir de acordo com esse erro
  //   (em geral, matando o processo)
  // err_t err = self->regERRO;
  mem_le(self->mem, CPU_END_erro, &self->processo_corrente->regErro);
  err_t err = self->processo_corrente->regErro;
  
  console_printf("SO: IRQ não tratada -- erro na CPU: %s", err_nome(err));
  self->erro_interno = true;
}

// interrupção gerada quando o timer expira
static void so_trata_irq_relogio(so_t *self)
{
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupção
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  
  if (e1 != ERR_OK || e2 != ERR_OK) {
    console_printf("SO: problema da reinicialização do timer");
    self->erro_interno = true;
  }
  // t2: deveria tratar a interrupção
  //   por exemplo, decrementa o quantum do processo corrente, quando se tem
  //   um escalonador com quantum
  //console_printf("SO: interrupção do relógio (não tratada)");

  if(self->escalonador != simples && self->processo_corrente != NULL)
    self->processo_corrente->quantum--; 
}

// foi gerada uma interrupção para a qual o SO não está preparado
static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  self->erro_interno = true;
}


// ---------------------------------------------------------------------
// CHAMADAS DE SISTEMA {{{1
// ---------------------------------------------------------------------

// funções auxiliares para cada chamada de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self, processo_t* processo_pendente);

static void so_trata_irq_chamada_sistema(so_t *self)
{
  // a identificação da chamada está no registrador A
  // t2: com processos, o reg A deve estar no descritor do processo corrente
  //int id_chamada = self->regA;

  int id_chamada = self->processo_corrente->A;
  console_printf("SO: chamada de sistema %d", id_chamada);

  Historico_processos *h = NULL;
  if(self->processo_corrente != NULL){
    h = hst_busca(self->ini_hist_proc, self->processo_corrente->id);
  }
  else
    return;

  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      h->quant_irq[IRQ_TECLADO]++;
      self->quant_irq[IRQ_TECLADO]++;
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      h->quant_irq[IRQ_TELA]++;
      self->quant_irq[IRQ_TELA]++;
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_chamada_espera_proc(self, self->processo_corrente);
      break;
    default:
      console_printf("SO: chamada de sistema desconhecida (%d)", id_chamada);
      // t2: deveria matar o processo
      so_chamada_mata_proc(self);
      self->erro_interno = true;
  }
}

// implementação da chamada se sistema SO_LE
// faz a leitura de um dado da entrada corrente do processo, coloca o dado no reg A
static void so_chamada_le(so_t *self)
{
  // implementação com espera ocupada
  //   t2: deveria realizar a leitura somente se a entrada estiver disponível,
  //     senão, deveria bloquear o processo.
  //   no caso de bloqueio do processo, a leitura (e desbloqueio) deverá
  //     ser feita mais tarde, em tratamentos pendentes em outra interrupção,
  //     ou diretamente em uma interrupção específica do dispositivo, se for
  //     o caso
  // implementação lendo direto do terminal A
  //   t2: deveria usar dispositivo de entrada corrente do processo
  //for (;;) {  // espera ocupada!
  int estado;
  if (es_le(self->es, self->processo_corrente->id_terminal + TERM_TECLADO_OK, &estado) != ERR_OK) {
    console_printf("SO: problema no acesso ao estado do teclado");
    self->processo_corrente->espera_terminal = 1;
    /*self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processo_corrente->id, bloqueado);
    altera_estado_proc_tabela(self->processos, self->processo_corrente->id, bloqueado);
    self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id); */
    so_muda_estado_processo(self, self->processo_corrente->id, bloqueado);
    return;
  }
    //if (estado != 0) break;
    // como não está saindo do SO, a unidade de controle não está executando seu laço.
    // esta gambiarra faz pelo menos a console ser atualizada
    // t2: com a implementação de bloqueio de processo, esta gambiarra não
    //   deve mais existir.
  console_tictac(self->console);
  //}
  int dado;
  if (es_le(self->es, self->processo_corrente->id_terminal + TERM_TECLADO, &dado) != ERR_OK) {
    console_printf("SO: problema no acesso ao teclado");
    self->processo_corrente->espera_terminal = 1;
    /*self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processo_corrente->id, bloqueado);
    self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id); 
    altera_estado_proc_tabela(self->processos, self->processo_corrente->id, bloqueado);*/
    so_muda_estado_processo(self, self->processo_corrente->id, bloqueado);
    return;
  }

  // escreve no reg A do processador
  // (na verdade, na posição onde o processador vai pegar o A quando retornar da int)
  // t2: se houvesse processo, deveria escrever no reg A do processo
  // t2: o acesso só deve ser feito nesse momento se for possível; se não, o processo
  //   é bloqueado, e o acesso só deve ser feito mais tarde (e o processo desbloqueado)
  
  if(self->processo_corrente != NULL){
    self->processo_corrente->A = dado;
  }

  //self->regA = dado;
}

// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{
  if(self->processo_corrente == NULL)
    console_printf("(proc_corrente null)");
  console_printf("(id_proc %d, id_terminal %d) ", self->processo_corrente->id, self->processo_corrente->id_terminal);
  // implementação com espera ocupada
  //   t2: deveria bloquear o processo se dispositivo ocupado
  // implementação escrevendo direto do terminal A
  //   t2: deveria usar o dispositivo de saída corrente do processo
  //for (;;) {
  int estado;
  if ((es_le(self->es, self->processo_corrente->id_terminal + TERM_TELA_OK, &estado)) != ERR_OK) {
    console_printf("SO: problema no acesso ao estado da tela");
    self->processo_corrente->espera_terminal = 2;
    /*self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processo_corrente->id, bloqueado);
    self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id); 
    altera_estado_proc_tabela(self->processos, self->processo_corrente->id, bloqueado);*/
    so_muda_estado_processo(self, self->processo_corrente->id, bloqueado);
    return;
  }
    //if (estado != 0) break;
    // como não está saindo do SO, a unidade de controle não está executando seu laço.
    // esta gambiarra faz pelo menos a console ser atualizada
    // t2: não deve mais existir quando houver suporte a processos, porque o SO não poderá
    //   executar por muito tempo, permitindo a execução do laço da unidade de controle
  console_tictac(self->console);
  //}
  int dado;
  // está lendo o valor de X e escrevendo o de A direto onde o processador colocou/vai pegar
  // t2: deveria usar os registradores do processo que está realizando a E/S
  // t2: caso o processo tenha sido bloqueado, esse acesso deve ser realizado em outra execução
  //   do SO, quando ele verificar que esse acesso já pode ser feito.
  //dado = self->regX;
  dado = self->processo_corrente->X;
  if ((es_escreve(self->es,  self->processo_corrente->id_terminal + TERM_TELA, dado)) != ERR_OK) {
    console_printf("SO: problema no acesso à tela");
    self->processo_corrente->espera_terminal = 2;
    /*self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processo_corrente->id, bloqueado);
    self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, self->processo_corrente->id); 
    altera_estado_proc_tabela(self->processos, self->processo_corrente->id, bloqueado);*/
    so_muda_estado_processo(self, self->processo_corrente->id, bloqueado);
    return;
  }
  //self->regA = 0;
  self->processo_corrente->A = 0;
}

// implementação da chamada se sistema SO_CRIA_PROC
// cria um processo
static void so_chamada_cria_proc(so_t *self)
{
  // ainda sem suporte a processos, carrega programa e passa a executar ele
  // quem chamou o sistema não vai mais ser executado, coitado!
  // t2: deveria criar um novo processo
  // em X está o endereço onde está o nome do arquivo
  // t2: deveria ler o X do descritor do processo criador
  //ender_proc = self->regX;
  int ender_proc;
  ender_proc = self->processo_corrente->X;
  processo_t *processo;

  char nome[100];
  if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
    //programa_t *prog = so_carrega_programa(self, nome, processo);
    programa_t *prog = so_carrega_programa(self, nome);
    processo = so_cria_entrada_processo(self, prog_end_carga(prog), prog_tamanho(prog));
    //processo = inicializa_processo(processo, self->cont_processos, prog_end_carga(prog), prog_tamanho(prog));
    if(processo != NULL){
      /*int ind_proc = encontra_indice_processo(self->processos, processo->id);
      if(ind_proc != -1){
        self->processos[ind_proc] = *processo;*/       /*guarda dados do processo criado no SO*/
      //self->cont_processos++;     /*contém a quantidade de processos*/
      processo->erro = ERR_OK;
      processo->regErro = 0;
      //} 
      self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, processo);
      console_printf("(id_proc: %d, ini_proc %d)", processo->id, self->ini_fila_proc_prontos->id);
      self->ini_fila_proc = lst_insere_ordenado(self->ini_fila_proc, processo->id, processo->prio);
      int tempo;
      es_le(self->es, D_RELOGIO_REAL, &tempo);
      self->ini_hist_proc = hst_insere_ordenado(self->ini_hist_proc, processo->id, tempo);
    }
    else{
      //cpu_interrompe(self->cpu, IRQ_ERR_CPU);
      processo->erro = ERR_OP_INV;
      processo->regErro = 1;
    }
    prog_destroi(prog);
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  if(processo->regErro == 1){
    self->processo_corrente->A = -1;
  }
  else{
    self->processo_corrente->A = processo->id;
  }
}

void so_calculo_e_impressao_metricas(so_t* self, int tempo);
static void so_libera_espera_proc(so_t *self, int id_proc_morrendo);

// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  // t2: deveria matar um processo
  // ainda sem suporte a processos, retorna erro -1
  //console_printf("SO: SO_MATA_PROC não implementada");
  //self->regA = -1;

  int id_proc_a_matar = self->processo_corrente->id;
  console_printf("(id proc_a_matar %d)", id_proc_a_matar);

  int indice = encontra_indice_processo(self->processos, id_proc_a_matar);
  Historico_processos* h = hst_busca(self->ini_hist_proc, id_proc_a_matar);
  /*calcula tempo de vida do processo*/
  int tempo;
  es_le(self->es, D_RELOGIO_REAL, &tempo);
  h->tempo_vida = tempo - h->tempo_vida;
  /*nao encontrou processo com esse id*/
  if(indice == -1){
    console_printf("SO: processo de id %d nao encontrado para SO_MATA_PROC", id_proc_a_matar);
    //self->regA = -1;
    self->processo_corrente->A = -1;
  }

  /*self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, id_proc_a_matar, morto);
  self->ini_fila_proc = lst_retira(self->ini_fila_proc, id_proc_a_matar);
  self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, id_proc_a_matar);
  altera_estado_proc_tabela(self->processos, id_proc_a_matar, morto);*/
  so_muda_estado_processo(self, id_proc_a_matar, morto);

  if(self->processo_corrente != NULL){
    self->dispositivos_livres[self->processo_corrente->id_terminal/4] = true;    //libera
    self->processo_corrente = NULL;
  }

  so_libera_espera_proc(self, id_proc_a_matar);

  /*Impressao das metricas finais*/
  if(id_proc_a_matar == 0){
    es_le(self->es, D_RELOGIO_REAL, &tempo);
    so_calculo_e_impressao_metricas(self, tempo);
  }

  self->regA = 0; //tudo ok
}

// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
static void so_chamada_espera_proc(so_t *self, processo_t* processo)
{
  // t2: deveria bloquear o processo se for o caso (e desbloquear na morte do esperado)
  // ainda sem suporte a processos, retorna erro -1
  /*console_printf("SO: SO_ESPERA_PROC não implementada");
  self->regA = -1;*/
  processo->estado = bloqueado;
  if(lst_busca(self->ini_fila_proc_prontos, processo->id) != NULL){
    console_printf("(proc %d esta em proc_prontos)", processo->id);
    self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, processo->id);
    if(self->ini_fila_proc_prontos != NULL)
      console_printf("(proc_pronto id %d)", self->ini_fila_proc_prontos->id);   /*Teste*/
  }

  /*int ind = encontra_indice_processo(self->processos, processo_pendente->X);
  if(ind == -1 || self->processos[ind].estado == morto){    //Se processo morto ou nao existe mais, entao pode parar de esperar
    processo_pendente->estado = pronto;
    self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, processo_pendente);
  }*/
}

// ---------------------------------------------------------------------
// CARGA DE PROGRAMA {{{1
// ---------------------------------------------------------------------

// carrega o programa na memória
// retorna o endereço de carga ou -1
static programa_t* so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return NULL;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf("Erro na carga da memória, endereco %d\n", end);
      return NULL;
    }
  }

  console_printf("SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return prog;
}


// ---------------------------------------------------------------------
// ACESSO À MEMÓRIA DOS PROCESSOS {{{1
// ---------------------------------------------------------------------

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// t2: deveria verificar se a memória pertence ao processo
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}
// vim: foldmethod=marker

float so_calcula_prioridade(processo_t* processo){
  int t_exec = QUANTUM_INICIAL - processo->quantum;
  float prioridade = (processo->prio + t_exec/QUANTUM_INICIAL) / 2;
  return prioridade;
}

Lista_processos* so_coloca_fila_pronto(so_t* self, processo_t* processo){
  float prio = 0.5; //simples e round-robin
  switch (self->escalonador){
  case simples:
    self->ini_fila_proc_prontos = lst_adicionar_final(self->ini_fila_proc_prontos, processo->id, processo->prio);
    break;
  case round_robin:
    self->ini_fila_proc_prontos = lst_adicionar_final(self->ini_fila_proc_prontos, processo->id, processo->prio);
    break;
  case prioridade:
    prio = so_calcula_prioridade(processo);
    self->ini_fila_proc_prontos = lst_insere_ordenado(self->ini_fila_proc_prontos, processo->id, prio);
  default:
    console_printf("SO: escalonador nao encontrado");
    break;
  }  
  return self->ini_fila_proc_prontos;
}

int so_busca_entrada_tabela(so_t* self){
  for(int i = 0; i < MAX_PROCESSOS; i++){
    if(self->processos[i].estado == morto){
      return i;
    }
  }
  return -1; //tabela cheia
}

processo_t* so_cria_entrada_processo(so_t* self, int PC, int tam) {
    int i = so_busca_entrada_tabela(self);
    if (i == -1) {
        console_printf("SO: tabela de processos cheia");
        return NULL;
    }
    int id = self->cont_processos++;
    inicializa_processo(&self->processos[i], id, PC, tam);
    self->processos[i].estado = pronto;
    if(self->processos[i].id == 0)
      console_printf("id eh 0");
    else
      console_printf("id diferente de zero");
    return &self->processos[i];
}

processo_t* so_proximo_pendente(so_t* self){
  Lista_processos* l = self->ini_fila_proc;
  while(l != NULL){
    if(l->estado == bloqueado){
      int indice = encontra_indice_processo(self->processos, l->id);
      return &self->processos[indice];
    }
    l = l->prox;
  }
  return NULL; //nao ha processos prontos
}

processo_t* so_proximo_pronto(so_t* self){
  Lista_processos* l = self->ini_fila_proc_prontos;
  if(l != NULL){
    int indice = encontra_indice_processo(self->processos, l->id);
    if(indice != -1)
      return &self->processos[indice];
  }
  return NULL; //nao ha processos prontos
}

static void so_muda_estado_processo(so_t* self, int id_proc, estado_proc est){
  altera_estado_proc_tabela(self->processos, id_proc, est);
  self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, id_proc, est);

  Historico_processos *h = hst_busca(self->ini_hist_proc, id_proc);
  if(h != NULL){
    int tempo;
    es_le(self->es, D_RELOGIO_REAL, &tempo);
    h->tempo_estado[est] += tempo - h->tempo_desde_ult_estado;
    h->tempo_desde_ult_estado = tempo;
  }

  if(est == pronto){
    self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, self->processo_corrente);
  }
  else{ /*bloqueado ou morto*/
    self->ini_fila_proc_prontos = lst_retira(self->ini_fila_proc_prontos, id_proc); 
    if(est == morto){
      self->ini_fila_proc = lst_retira(self->ini_fila_proc, id_proc);
    }
  }
}

static void so_libera_espera_proc(so_t *self, int id_proc_morrendo){
  for(int i = 0; i < MAX_PROCESSOS; i++){ 
    if(self->processos[i].X == id_proc_morrendo){
      /*altera_estado_proc_tabela(self->processos, self->processos[i].id, pronto);
      self->ini_fila_proc = lst_altera_estado(self->ini_fila_proc, self->processos[i].id, pronto);
      processo_t *processo = lst_busca(self->ini_fila_proc, self->processos[i].id);
      self->ini_fila_proc_prontos = so_coloca_fila_pronto(self, processo);*/
      so_muda_estado_processo(self, self->processos[i].id, pronto);
    }
  }
}

static void so_calcula_tempo_ocioso(so_t* self){
  if(self->processo_corrente == NULL){
    int tempo;
    es_le(self->es, D_RELOGIO_REAL, &tempo);
    self->momento_sist_ocioso = tempo;
  }
  else{
    if(self->momento_sist_ocioso != 0){
      int tempo;
      es_le(self->es, D_RELOGIO_REAL, &tempo);
      self->tempo_ocioso_total += tempo - self->momento_sist_ocioso;
      self->momento_sist_ocioso = 0;
    }
  }
}

void so_calculo_e_impressao_metricas(so_t* self, int tempo){
  console_printf("---Metricas---");
  console_printf("Foram criados %d processos", self->cont_processos);

  self->tempo_total_execucao = tempo - self->tempo_total_execucao;
  console_printf("O sistema ficou executando por %d", self->tempo_total_execucao);
  console_printf("O sistema ficou ocioso por %d\n", self->tempo_ocioso_total);

  for(int i = 0; i < TIPOS_IRQ; i++){
    console_printf("No total foram %d IRQ %d (%s)", self->quant_irq[i], i, irq_nome(i));
  }

  console_printf("\nForam %d preempcoes no total", self->n_preempcoes);

  for(int i = 0; i < self->cont_processos; i++){
    Historico_processos *h = hst_busca(self->ini_hist_proc, i);
    console_printf("Processo %d: ", i);
    console_printf("Tempo de retorno/vida: %d", h->tempo_vida);
    console_printf("Numero de preempcao: %d", h->n_preempcoes);
    console_printf("Em media, o tempo de resposta foi %d \n", h->tempo_espera/h->quant_estado[pronto]);
    for(int j = 0; j < 3; j++){
      console_printf("Estado %s", estado_nome(j));
      console_printf("Entrou %d vezes nesse estado", h->quant_estado[j]);
      console_printf("Ficou %d nesse estado", h->tempo_estado[j]);
    }   
  }
}