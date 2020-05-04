#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"
#include "queue.h"

typedef struct threadClienteArgs
{
	int thid;
    int K;
    int T;
    int P;
    int S;
} threadClienteArgs_t;

typedef struct Cassa
{
	int thid;
    int TP;
	Queue_t *q;
	char active;
} Cassa_t;

void FaiAcquisti(unsigned int *seed, int T)
{
	long r = 10 + rand_r(seed) % (T - 10);
	printf("Facendo acquisti per %ld ms \n", r);
	struct timespec t = {0, r};
	nanosleep(&t, NULL);
}

void ServiCliente(long t_cassiere, int TP) {
    int n_prodotti = 10;
	struct timespec t = {0, t_cassiere + TP * n_prodotti};
	nanosleep(&t, NULL);
}

// thread cliente
void *Cliente(void *arg)
{
	int myid = ((threadClienteArgs_t *)arg)->thid;
	int K = ((threadClienteArgs_t *)arg)->K;
	int T = ((threadClienteArgs_t *)arg)->T;
	int P = ((threadClienteArgs_t *)arg)->P;
	int S = ((threadClienteArgs_t *)arg)->S;
	unsigned int seed = myid;

    FaiAcquisti(&seed, T);

	fflush(stdout);
	pthread_exit(NULL);
}

// thread cassiere
void *Cassiere(void *arg)
{
    int myid = ((Cassa_t *)arg)->thid;
	int TP = ((Cassa_t *)arg)->TP;
    unsigned int seed = myid;

    long t_cassiere = 20 + rand_r(&seed) % 60;

    ServiCliente(t_cassiere, TP);

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

void initClienti(pthread_t **th, int K, int C, int T, int P, int S)
{
	(*th) =  malloc(C * sizeof(pthread_t));
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
	}

    
	for (int i = 0; i < C; ++i)
		if (pthread_create(*th + i, NULL, Cliente, &thARGS[i]) != 0)
		{
			fprintf(stderr, "pthread_create failed\n");
			exit(EXIT_FAILURE);
		}

    free(thARGS);
}

int main(int argc, char **argv) {
	// Numero di casse
    int K=6;

    // Numero max di clienti nel supermercato allo stesso momento
    int C=50;

    // Clienti che vengono fatti entrare quando si arriva a C-E clienti
    int E=3;

    // Tempo impiegato da un cliente per fare acquisti t : 10 < t < T
    int T=200;

    // Prodotti acquistati da un cliente p : 0 < p < P
    int P=100;

    // Ogni quanti ms un cliente valuta se spostarti di cassa
    int S=20;

    // Tempo per prodotto
    int TP = 10;

	pthread_t *th_clienti =  NULL;
    pthread_t *th_cassieri = NULL;
	Cassa_t *casse =  NULL;
	
	if( argc == 8 ) {
        K = atoi(argv[1]);
        C = atoi(argv[2]) + 1;
        E = atoi(argv[3]);
        T = atoi(argv[4]);
        P = atoi(argv[5]);
        S = atoi(argv[6]);
        TP = atoi(argv[7]);
	}

    initClienti(&th_clienti, K, C, T, P, S);

    initCassieri(&th_cassieri, &casse, K, TP);

	if(!th_cassieri || !th_clienti) {
        fprintf(stderr, "malloc fallita\n");
		exit(EXIT_FAILURE);
    }

	for (int i = 0; i < C; ++i) {
		if (pthread_join(th_clienti[i], NULL) == -1)
		{
			fprintf(stderr, "pthread_join failed\n");
		}
    }

	free(th_clienti);
    free(th_cassieri);

    return 0;
}