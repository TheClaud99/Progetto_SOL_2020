#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "utils.h"
#include "queue.h"

typedef struct Cassa
{
	int thid;
	int TP;				 // Tempo impiegato per ogni prodotto
	Queue_t *q;			 // Fila alla cassa
	char active;		 // Dice se la cassa è aperta
	pthread_mutex_t mtx; // Mutua esclusione per accedere ad active
	pthread_cond_t cond; // Condizione per segnalare che il cliente è stato servito
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

typedef struct threadDirettoreArgs
{
	int K;
	int T;
	int P;
	int S;
	int S1;
	int S2;
	Cassa_t *casse;
} threadDirettoreArgs_t;


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

void emptyQueue(Cassa_t *cassa)
{
	while (cassa->q->tail != cassa->q->head)
	{
		Cliente_t *cliente;
		if ((cliente = pop(cassa->q)) != NULL)
		{
			Pthread_mutex_lock(&cliente->mtx);
			Pthread_cond_signal(&cliente->cond);
			Pthread_mutex_unlock(&cliente->mtx);
		}
	}
}

void FaiAcquisti(Cliente_t *cliente, int T, int P)
{
	unsigned int seed = cliente->id;
	long r = 10 + rand_r(&seed) % (T - 10);
	// printf("Facendo acquisti per %ld ms \n", r);
	struct timespec t = {0, r};

	Pthread_mutex_lock(&cliente->mtx);
	nanosleep(&t, NULL);
	cliente->nprod = rand_r(&seed) % P;
	Pthread_mutex_unlock(&cliente->mtx);
}

void ServiCliente(Cassa_t *cassa, long t_cassiere)
{
	// Prende il primo cliente in coda
	Cliente_t *cliente = pop(cassa->q);

	if (cliente != NULL)
	{
		// Acquisice la mutua esclusione sul cliente e lo serve
		struct timespec t = {0, t_cassiere + cassa->TP * cliente->nprod};
		Pthread_mutex_lock(&cliente->mtx);
		nanosleep(&t, NULL);
		cliente->servito = 1;
		Pthread_cond_signal(&cliente->cond);
		Pthread_mutex_unlock(&cliente->mtx);

		printf("Cassa %d: Servito cliente %d\n", cassa->thid, cliente->id);
	}
}


void scegliCassa(Cliente_t *cliente, Cassa_t *casse, int K)
{
	// Il cliente si mette in coda alla cassa con fila più corta dopo averle controllate tutte
	Cassa_t *cassa_scelta = NULL;
	for (int i = 0; i < K; ++i)
	{
		// Se la cassa e' attiva e se la cassa scelta e' NULL oppure con coda più lunga viene scelta
		if ((cassa_scelta == NULL || length(cassa_scelta->q) > length(casse[i].q)) && isActive(&casse[i]))
			cassa_scelta = casse + i;
	}

	if (cassa_scelta == NULL)
	{
		fprintf(stderr, "nessuna cassa attiva\n");
		exit(EXIT_FAILURE);
	}

	push(cassa_scelta->q, cliente);

	printf("%d messo in coda alla cassa %d\n", cliente->id, cassa_scelta->thid);
}

void aspettaInCoda(Cliente_t *cliente, Cassa_t *casse, int K)
{
	// Aspetta fino a che non è servito
	Pthread_mutex_lock(&cliente->mtx);
	while (cliente->servito == 0)
	{
		scegliCassa(cliente, casse, K);
		Pthread_cond_wait(&cliente->cond, &cliente->mtx);
	}
	Pthread_mutex_unlock(&cliente->mtx);
}

// thread cliente
void *Cliente(void *arg)
{
	Cliente_t *cliente = malloc(sizeof(Cliente_t));
	Cassa_t *casse = ((threadClienteArgs_t *)arg)->casse;
	cliente->id = ((threadClienteArgs_t *)arg)->thid;
	int K = ((threadClienteArgs_t *)arg)->K;
	int T = ((threadClienteArgs_t *)arg)->T;
	int P = ((threadClienteArgs_t *)arg)->P;
	int S = ((threadClienteArgs_t *)arg)->S;

	if (pthread_mutex_init(&cliente->mtx, NULL) != 0)
	{
		fprintf(stderr, "pthread_mutex_init failed\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_cond_init(&cliente->cond, NULL) != 0)
	{
		fprintf(stderr, "pthread_cond_init failed\n");
		exit(EXIT_FAILURE);
	}

	FaiAcquisti(cliente, T, P);

	aspettaInCoda(cliente, casse, K);

	fflush(stdout);
	pthread_exit(NULL);
}

void apriCassa(Cassa_t *casse)
{
	// printf("Apro cassa");
}

void chiudiCassa(Cassa_t *casse, int K)
{
	Cassa_t *cassa_scelta = NULL;
	for (int i = 0; i < K; ++i)
	{
		// Se la cassa e' attiva e se la cassa scelta e' NULL oppure con coda più lunga viene scelta
		if ((cassa_scelta == NULL || length(cassa_scelta->q) > length(casse[i].q)) && casse[i].active)
			cassa_scelta = casse + i;
	}

	if (cassa_scelta == NULL)
	{
		fprintf(stderr, "nessuna cassa attiva\n");
		exit(EXIT_FAILURE);
	}

	Pthread_mutex_lock(&cassa_scelta->mtx);
	cassa_scelta->active = 0;
	Pthread_mutex_unlock(&cassa_scelta->mtx);

	printf("Chiudo cassa %d\n", cassa_scelta->thid);
}

// thread cliente
void *Direttore(void *arg)
{
	Cassa_t *casse = ((threadDirettoreArgs_t *)arg)->casse;
	int K = ((threadDirettoreArgs_t *)arg)->K;
	int T = ((threadDirettoreArgs_t *)arg)->T;
	int P = ((threadDirettoreArgs_t *)arg)->P;
	int S = ((threadDirettoreArgs_t *)arg)->S;
	int S1 = ((threadDirettoreArgs_t *)arg)->S1;
	int S2 = ((threadDirettoreArgs_t *)arg)->S2;

	int count_max1cliente;
	int count_minS2clienti;
	int count_aperte;

	printf("Thread direttore started\n");

	while (1)
	{
		count_max1cliente = 0;
		count_minS2clienti = 0;
		count_aperte = 0;
		for(int i = 0; i < K; i++)
		{
			int coda = length(casse[i].q);
			if (coda <= 1) count_max1cliente++;
			if (coda >= S2) count_minS2clienti++;
			if(isActive(&casse[i]) == 1)
				count_aperte++;
		}

		// printf("Aperte:%d\n", count_aperte);
		if(count_max1cliente >= S1)
			if(count_aperte < K)
				apriCassa(casse);

		if(count_minS2clienti > 0)
			if(count_aperte > 1)
				chiudiCassa(casse, K);
	}

	fflush(stdout);
	pthread_exit(NULL);
}

// thread cassiere
void *Cassiere(void *arg)
{
	Cassa_t *cassa = (Cassa_t *)arg;
	unsigned int seed = cassa->thid;
	long t_cassiere = 20 + rand_r(&seed) % 60;

	if (pthread_mutex_init(&cassa->mtx, NULL) != 0)
	{
		fprintf(stderr, "pthread_mutex_init failed\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_cond_init(&cassa->cond, NULL) != 0)
	{
		fprintf(stderr, "pthread_cond_init failed\n");
		exit(EXIT_FAILURE);
	}

	int i = 0;
	while (i < 100)
	{
		Pthread_mutex_lock(&cassa->mtx);
		// Se la cassa è attiva serve il prossimo cliente
		if (cassa->active == 1)
		{
			Pthread_mutex_unlock(&cassa->mtx);
			ServiCliente(cassa, t_cassiere);
		}
		else
		{
			emptyQueue(cassa);
			printf("Coda svuotata\n");
			// Aspetta di essere riattivata
			Pthread_cond_wait(&cassa->cond, &cassa->mtx);
			Pthread_mutex_unlock(&cassa->mtx);
		}
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

/* un gestore piuttosto semplice */
static void gestore (int signum) {
	printf("Ricevuto segnale %d\n",signum);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct sigaction s;
	/* inizializzo s a 0*/
	memset( &s, 0, sizeof(s) );
	s.sa_handler=gestore; /* registro gestore */
	/* installo nuovo gestore s */
	ec_meno1( sigaction(SIGINT,&s,NULL), "Signal handler error" );

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

	// Soglia per la chiusura di una cassa
	int S1 = 2;

	// Soglia per l'apertura di una cassa
	int S2 = 10;

	// Tempo per prodotto
	int TP = 10;

	pthread_t *th_clienti = NULL;
	pthread_t *th_new_clienti = NULL;
	pthread_t *th_cassieri = NULL;
	pthread_t th_direttore;
	Cassa_t *casse = NULL;

	if (argc == 10)
	{
		K = atoi(argv[1]);
		C = atoi(argv[2]) + 1;
		E = atoi(argv[3]);
		T = atoi(argv[4]);
		P = atoi(argv[5]);
		S = atoi(argv[6]);
		S1 = atoi(argv[7]);
		S2 = atoi(argv[8]);
		TP = atoi(argv[9]);
	}

	initCassieri(&th_cassieri, &casse, K, TP);

	threadDirettoreArgs_t thDirettoreARGS = {K, T, P, S, S1, S2, casse};
	if (pthread_create(&th_direttore, NULL, Direttore, &thDirettoreARGS) != 0)
	{
		fprintf(stderr, "pthread_create failed\n");
		exit(EXIT_FAILURE);
	}

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

		if (C - i == E)
			initClienti(&th_new_clienti, K, E, T, P, S, casse);
	}

	for (int i = 0; i < E; i++)
	{
		if (pthread_join(th_new_clienti[i], NULL) == -1)
		{
			fprintf(stderr, "pthread_join failed\n");
		}
	}

	for (int i = 0; i < K; i++)
	{
		if (pthread_join(th_cassieri[i], NULL) == -1)
		{
			fprintf(stderr, "pthread_join failed\n");
		}
	}

	free(th_clienti);
	free(th_new_clienti);
	free(th_cassieri);

	return 0;
}