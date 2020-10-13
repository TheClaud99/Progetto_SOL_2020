#if !defined(GLOBAL_H)
#define GLOBAL_H

#include <pthread.h>
#include "utils.h"
#include "queue.h"

#define APRICASSA   1
#define CHIUDICASSA 2
#define DIRETTOREID 0
#define CASSACHIUSA -1

typedef struct Cassa
{
	int thid;
	int TP;				 // Tempo impiegato per ogni prodotto
	Queue_t *q;			 // Fila alla cassa
	char active;		 // Dice se la cassa è aperta
	pthread_mutex_t mtx; // Mutua esclusione per accedere ad active
	pthread_cond_t cond; // Segnala la disattivazione o l'arrivo di un cliente
} Cassa_t;

typedef struct threadCassaArgs
{
	int *clienti_attivi;
	Cassa_t cassa;
} threadCassaArgs_t;

typedef struct Cliente
{
	int id;
	int nprod;			 // Numero dei prodotti che il cliente ha acquistato
	char servito;		 // 1 se il cliente è stato servito, 0 se deve ancora esserlo
	pthread_mutex_t mtx; // Mutua esclusione per accedere a servito
	pthread_cond_t cond; // Condizione per segnalare che il cliente è stato servito
} Cliente_t;

typedef struct threadClienteArgs
{
	int thid;
	int K;
	int T;
	int P;
	int S;
	Cassa_t *casse;
} threadClienteArgs_t;

typedef struct threadDirettoreArgs
{
	int K;
	int S1;
	int S2;
	Cassa_t *casse;
} threadDirettoreArgs_t;

/** 
 * tipo del messaggio
 */
typedef struct msg {
    int len;
    int cassa_id;
	int active;
} msg_t;


char isActive(Cassa_t *cassa)
{
	char active;
	if(cassa == NULL)
		return 0;
	
	Pthread_mutex_lock(&cassa->mtx);
	active = (cassa->active == 1);
	Pthread_mutex_unlock(&cassa->mtx);
	return active;
}

void printCasse(Cassa_t *casse, int K) {
    for(int i = 0; i < K; i++) {
        printf(" id: %d \n active: %d \n length: %ld \n ", casse[i].thid, casse[i].active, length(casse[i].q));
    }
}

#endif /* GLOBAL_H */
