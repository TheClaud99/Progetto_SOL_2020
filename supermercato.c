#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"
#include "queue.h"

typedef struct Cassa
{
	int thid;
	int TP;
	Queue_t *q;
	char active;
} Cassa_t;

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

int FaiAcquisti(unsigned int *seed, int T, int P)
{
	long r = 10 + rand_r(seed) % (T - 10);
	printf("Facendo acquisti per %ld ms \n", r);
	struct timespec t = {0, r};
	nanosleep(&t, NULL);

	return rand_r(seed) % P;
}

void ServiCliente(Cassa_t *cassa, long t_cassiere)
{
	// Prende il primo cliente in coda
	Cliente_t *cliente = pop(cassa->q);

	if (cliente != NULL)
	{
		// Acquisice la mutua esclusione sul cliente e lo serve
		Pthread_mutex_lock(&cliente->mtx);
		struct timespec t = {0, t_cassiere + cassa->TP * cliente->nprod};
		nanosleep(&t, NULL);
		cliente->servito = 1;
		Pthread_cond_signal(&cliente->cond);
		Pthread_mutex_unlock(&cliente->mtx);

		printf("Servito cliente %d\n", cliente->id);
	}
}

void aspettaInCoda(Cliente_t *cliente, Cassa_t *casse, int K)
{

	// Il cliente si mette in coda alla cassa con fila più corta dopo averle controllate tutte
	Cassa_t *cassa_scelta = casse;
	for (int i = 0; i < K; ++i)
	{
		if (cassa_scelta->q->qlen > casse[i].q->qlen)
			cassa_scelta = casse + i;
	}
	push(cassa_scelta->q, cliente);

	printf("%d messo in coda alla cassa %d\n", cliente->id, cassa_scelta->thid);

	// Aspetta fino a che non è servito
	Pthread_mutex_lock(&cliente->mtx);
	while (cliente->servito == 0)
	{
		Pthread_cond_wait(&cliente->cond, &cliente->mtx);
	}
	Pthread_mutex_unlock(&cliente->mtx);
}

// thread cliente
void *Cliente(void *arg)
{
	int myid = ((threadClienteArgs_t *)arg)->thid;
	int K = ((threadClienteArgs_t *)arg)->K;
	int T = ((threadClienteArgs_t *)arg)->T;
	int P = ((threadClienteArgs_t *)arg)->P;
	int S = ((threadClienteArgs_t *)arg)->S;
	Cassa_t *casse = ((threadClienteArgs_t *)arg)->casse;
	unsigned int seed = myid;
	Cliente_t *cliente = malloc(sizeof(Cliente_t));

	if(pthread_mutex_init(&cliente->mtx, NULL) != 0)
	{
		fprintf(stderr, "pthread_mutex_init failed\n");
		exit(EXIT_FAILURE);
	}
	
	if(pthread_cond_init(&cliente->cond, NULL) != 0)
	{
		fprintf(stderr, "pthread_cond_init failed\n");
		exit(EXIT_FAILURE);
	}

	cliente->nprod = FaiAcquisti(&seed, T, P);
	cliente->id = myid;

	aspettaInCoda(cliente, casse, K);

	fflush(stdout);
	pthread_exit(NULL);
}

// thread cassiere
void *Cassiere(void *arg)
{
	Cassa_t *cassa = (Cassa_t *)arg;
	unsigned int seed = cassa->thid;

	long t_cassiere = 20 + rand_r(&seed) % 60;

	int i = 0;
	while (cassa->active != 0 && i < 100)
	{
		ServiCliente(cassa, t_cassiere);
		i++;
	}

	pthread_exit(NULL);
}

void initCassieri(pthread_t **th, Cassa_t **thARGS, int K, int TP)
{
	*th = malloc(K * sizeof(pthread_t));
	*thARGS = malloc(K * sizeof(Cassa_t));

	if (!(*th) || !(*thARGS))
	{
		fprintf(stderr, "malloc fallita cassieri fallita\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < K; ++i)
	{
		(*thARGS)[i].thid = (i + 1);
		(*thARGS)[i].TP = TP;
		(*thARGS)[i].active = 1;
		(*thARGS)[i].q = initQueue();
	}

	for (int i = 0; i < K; ++i)
		if (pthread_create((*th + i), NULL, Cassiere, (*thARGS + i)) != 0)
		{
			fprintf(stderr, "pthread_create failed\n");
			exit(EXIT_FAILURE);
		}
}

void initClienti(pthread_t **th, int K, int C, int T, int P, int S, Cassa_t *casse)
{
	(*th) = malloc(C * sizeof(pthread_t));
	threadClienteArgs_t *thARGS = malloc(C * sizeof(threadClienteArgs_t));

	if (!(*th) || !thARGS)
	{
		fprintf(stderr, "malloc fallita clienti fallita\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < C; ++i)
	{
		thARGS[i].thid = (i + 1);
		thARGS[i].K = K;
		thARGS[i].T = T;
		thARGS[i].P = P;
		thARGS[i].S = S;
		thARGS[i].casse = casse;
	}

	for (int i = 0; i < C; ++i)
		if (pthread_create(*th + i, NULL, Cliente, &thARGS[i]) != 0)
		{
			fprintf(stderr, "pthread_create failed\n");
			exit(EXIT_FAILURE);
		}

	free(thARGS);
}

int main(int argc, char **argv)
{
	// Numero di casse
	int K = 6;

	// Numero max di clienti nel supermercato allo stesso momento
	int C = 50;

	// Clienti che vengono fatti entrare quando si arriva a C-E clienti
	int E = 3;

	// Tempo impiegato da un cliente per fare acquisti t : 10 < t < T
	int T = 200;

	// Prodotti acquistati da un cliente p : 0 < p < P
	int P = 100;

	// Ogni quanti ms un cliente valuta se spostarti di cassa
	int S = 20;

	// Tempo per prodotto
	int TP = 10;

	pthread_t *th_clienti = NULL;
	pthread_t *th_new_clienti = NULL;
	pthread_t *th_cassieri = NULL;
	Cassa_t *casse = NULL;

	if (argc == 8)
	{
		K = atoi(argv[1]);
		C = atoi(argv[2]) + 1;
		E = atoi(argv[3]);
		T = atoi(argv[4]);
		P = atoi(argv[5]);
		S = atoi(argv[6]);
		TP = atoi(argv[7]);
	}

	initCassieri(&th_cassieri, &casse, K, TP);

	initClienti(&th_clienti, K, C, T, P, S, casse);

	if (!th_cassieri || !th_clienti)
	{
		fprintf(stderr, "malloc fallita\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < C; ++i)
	{
		if (pthread_join(th_clienti[i], NULL) == -1)
		{
			fprintf(stderr, "pthread_join failed\n");
		}

		if(C - i == E)
			initClienti(&th_new_clienti, K, E, T, P, S, casse);
	}

	for(int i = 0; i < E; i++)
	{
		if (pthread_join(th_new_clienti[i], NULL) == -1)
		{
			fprintf(stderr, "pthread_join failed\n");
		}
	}

	free(th_clienti);
	free(th_new_clienti);
	free(th_cassieri);

	return 0;
}