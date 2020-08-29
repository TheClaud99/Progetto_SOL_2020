#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <conn.h>
#include "queue.h"
#include "global.h"

typedef struct thInvia_args
{
	Cassa_t *cassa;
	long intervallo;
} thInvia_args_t;

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
	printf("Lunghezza coda alla cassa %d: %ld\n", cassa->thid, length(cassa->q));
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
	// Il cliente controlla la cassa con fila più corta
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

	// Si mette in fila mandando un segnale al cassiere
	Pthread_mutex_lock(&cassa_scelta->mtx);
	push(cassa_scelta->q, cliente);
	Pthread_cond_signal(&cassa_scelta->cond);
	Pthread_mutex_unlock(&cassa_scelta->mtx);

	// printf("%d messo in coda alla cassa %d\n", cliente->id, cassa_scelta->thid);
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
	// int S = ((threadClienteArgs_t *)arg)->S;

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

void apriCassa(Cassa_t *casse, int K)
{
	Cassa_t *cassa_scelta = NULL;
	int i = 0;

	// Prende la prima cassa chiusa della lista
	while(cassa_scelta == NULL && i < K)
	{
		// Se la cassa e' disattiva
		if (isActive(&casse[i]) == 0)
			cassa_scelta = casse + i;
		i++;
	}

	if (cassa_scelta == NULL)
	{
		fprintf(stderr, "nessuna cassa disattiva\n");
		exit(EXIT_FAILURE);
	}

	Pthread_mutex_lock(&cassa_scelta->mtx);
	cassa_scelta->active = 1;
	Pthread_cond_signal(&cassa_scelta->cond);
	Pthread_mutex_unlock(&cassa_scelta->mtx);
	printf("Apro cassa %d\n", cassa_scelta->thid);
}

void chiudiCassa(Cassa_t *casse, int K)
{
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

	Pthread_mutex_lock(&cassa_scelta->mtx);
	cassa_scelta->active = 0;
	Pthread_cond_signal(&cassa_scelta->cond);
	Pthread_mutex_unlock(&cassa_scelta->mtx);

	printf("Chiudo cassa %d\n", cassa_scelta->thid);
}

// thread cliente
void *Direttore(void *arg)
{
	Cassa_t *casse = ((threadDirettoreArgs_t *)arg)->casse;
	int K = ((threadDirettoreArgs_t *)arg)->K;
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

		// Se il numero di casse con un cliente solo in fila supera S1
		// chiudo una cassa
		if(count_max1cliente >= S1)
			if(count_aperte > 1)
				chiudiCassa(casse, K);

		// Se esiste una cassa con almeno S2 clienti in fila
		// apro un'altra cassa
		if(count_minS2clienti > 0)
			if(count_aperte < K)
				apriCassa(casse, K);
	}

	fflush(stdout);
	pthread_exit(NULL);
}

int sendCassa(Cassa_t cassa) {
	struct sockaddr_un serv_addr;
	int sockfd;
	
	SYSCALL(sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME) + 1);
	msg_t message = {length(cassa.q), cassa.thid};
	int notused;
	
	int operation;
	SYSCALL(notused, connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "connect");

	SYSCALL(notused, writen(sockfd, &message, sizeof(msg_t)), "writen");

	SYSCALL(notused, readn(sockfd, &operation, sizeof(int)), "read");

	printf("connection result: %d\n", operation);

	close(sockfd);

	return operation;
}

void *InviaCoda(void *args) {
	long intervallo = ((thInvia_args_t *)args)->intervallo;
	Cassa_t *cassa = ((thInvia_args_t *)args)->cassa;
	struct timespec t = {0, intervallo};
	while (1)
	{
		if(cassa->active == 1) {
			int operation = sendCassa(*cassa);
		}

		nanosleep(&t, NULL);
	}
}

// thread cassiere
void *Cassiere(void *arg)
{
	Cassa_t *cassa = (Cassa_t *)arg;
	pthread_t th_invia_coda;
	int intervallo = 500;
	thInvia_args_t inviaCodaARGS = {cassa, intervallo};
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

	if (pthread_create(&th_invia_coda, NULL, InviaCoda, &inviaCodaARGS) != 0)
	{
		fprintf(stderr, "pthread_create failed\n");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		Pthread_mutex_lock(&cassa->mtx);
		// Se la cassa è attiva serve il prossimo cliente
		if (cassa->active == 1)
		{
			if(length(cassa->q) > 0)
				ServiCliente(cassa, t_cassiere);
			else
				// Aspetta che un cliente si metta in coda o di venire disattivata
				Pthread_cond_wait(&cassa->cond, &cassa->mtx);
		}
		else
		{
			emptyQueue(cassa);
			printf("Coda svuotata\n");
			// Aspetta di essere riattivata
			Pthread_cond_wait(&cassa->cond, &cassa->mtx);
			printf("Cassa riaperta\n");
		}
		Pthread_mutex_unlock(&cassa->mtx);
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
	int K = 2;

	// Numero max di clienti nel supermercato allo stesso momento
	int C = 20;

	// Clienti che vengono fatti entrare quando si arriva a C-E clienti
	int E = 5;

	// Tempo impiegato da un cliente per fare acquisti t : 10 < t < T
	int T = 500;

	// Prodotti acquistati da un cliente p : 0 < p < P
	int P = 80;

	// Ogni quanti ms un cliente valuta se spostarti di cassa
	int S = 30;

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

	threadDirettoreArgs_t thDirettoreARGS = {K, S1, S2, casse};
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