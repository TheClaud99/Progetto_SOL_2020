#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <conn.h>
#include <time.h>
#include "queue.h"
#include "global.h"

typedef struct thInvia_args
{
	Cassa_t *cassa;
	long intervallo;
} thInvia_args_t;

static FILE *logfile; //File containing stats of the execution

int calcolaPosizioneInCoda(Queue_t *q, int id)
{
	int pos = 1;
	Node_t *p = (Node_t *)q->tail;
	int found = 0;
	while (found != 1 && p != NULL)
	{
		if (((Cliente_t *)p->data)->id == id)
			found = 1;
		else
		{
			pos++;
			p = p->next;
		}
	}

	if (found == 1)
		return pos;

	return -1;
}

void removeClientFromQueue(Queue_t *q, int id)
{
	Node_t *p = (Node_t *)q->tail;
	Node_t *prec = NULL;
	int found = 0;
	while (found != 1 && p != NULL)
	{
		if (((Cliente_t *)p->data)->id == id)
			found = 1;
		else
		{
			prec = p;
			p = p->next;
		}
	}

	if (found == 1)
	{
		if (prec == NULL)
		{
			q->tail = q->tail->next;
			free(p);
		}
		else
		{
			prec->next = p->next;
			free(p);
		}
	}
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

long FaiAcquisti(Cliente_t *cliente, int T, int P)
{
	unsigned int seed = cliente->id;
	long r = 10 + rand_r(&seed) % (T - 10);
	// printf("Facendo acquisti per %ld ms \n", r);

	Pthread_mutex_lock(&cliente->mtx);
	msleep(r);
	cliente->nprod = rand_r(&seed) % P;
	Pthread_mutex_unlock(&cliente->mtx);

	return r;
}

void ServiCliente(Cassa_t *cassa, long t_cassiere)
{
	// printf("Lunghezza coda alla cassa %d: %ld\n", cassa->thid, length(cassa->q));
	// Prende il primo cliente in coda
	Cliente_t *cliente = pop(cassa->q);

	if (cliente != NULL)
	{
		// Acquisice la mutua esclusione sul cliente e lo serve
		Pthread_mutex_lock(&cliente->mtx);
		msleep(t_cassiere + cassa->TP * cliente->nprod);
		cliente->servito = 1;
		Pthread_cond_signal(&cliente->cond);
		Pthread_mutex_unlock(&cliente->mtx);

		printf("Cassa %d: Servito cliente %d\n", cassa->thid, cliente->id);
	}
}

Cassa_t *getMinCoda(Cassa_t *casse, int K)
{
	// Il cliente controlla la cassa con fila più corta
	Cassa_t *cassa_scelta = NULL;

	for (int i = 0; i < K; ++i)
	{
		// Se la cassa e' attiva e se la cassa scelta e' NULL oppure con coda più lunga viene scelta
		if ((cassa_scelta == NULL || length(cassa_scelta->q) > length(casse[i].q)) && isActive(&casse[i]))
			cassa_scelta = casse + i;
	}

	return cassa_scelta;
}

Cassa_t *getMinCodaNoMtx(Cassa_t *casse, int K)
{
	// Il cliente controlla la cassa con fila più corta
	Cassa_t *cassa_scelta = NULL;

	for (int i = 0; i < K; ++i)
	{
		// Se la cassa e' attiva e se la cassa scelta e' NULL oppure con coda più lunga viene scelta
		if ((cassa_scelta == NULL || cassa_scelta->q->qlen > casse[i].q->qlen) && casse[i].active == 1)
			cassa_scelta = casse + i;
	}

	return cassa_scelta;
}

void mettiInFila(Cliente_t *cliente, Cassa_t *cassa)
{
	// Si mette in fila mandando un segnale al cassiere
	Pthread_mutex_lock(&cassa->mtx);
	push(cassa->q, cliente);
	Pthread_cond_signal(&cassa->cond);
	Pthread_mutex_unlock(&cassa->mtx);
}

Cassa_t *scegliCassa(Cliente_t *cliente, Cassa_t *casse, int K)
{
	Cassa_t *cassa_scelta = getMinCoda(casse, K);

	if (cassa_scelta == NULL)
	{
		fprintf(stderr, "nessuna cassa attiva\n");
		exit(EXIT_FAILURE);
	}

	mettiInFila(cliente, cassa_scelta);
	// printf("%d messo in coda alla cassa %d\n", cliente->id, cassa_scelta->thid);

	return cassa_scelta;
}

int aspettaInCoda(Cliente_t *cliente, Cassa_t *casse, int K)
{
	int cambi_cassa = 0;
	long intervallo = 5000;
	struct timespec ts;
	Cassa_t *cassa_scelta = NULL;
	cassa_scelta = scegliCassa(cliente, casse, K);

	// Aspetta fino a che non è servito
	Pthread_mutex_lock(&cliente->mtx);
	while (cliente->servito == 0)
	{
		clock_gettime(CLOCK_REALTIME, &ts);
		summstotimespec(&ts, intervallo);
		// ts.tv_sec += 20;

		Pthread_cond_timedwait(&cliente->cond, &cliente->mtx, &ts);
		// Pthread_cond_wait(&cliente->cond, &cliente->mtx);

		// Se il cliente non è stato ancora servito, valuta se cambiare cassa
		if (!cliente->servito)
		{
			int posizione = calcolaPosizioneInCoda(cassa_scelta->q, cliente->id);
			Cassa_t *minCassa = getMinCoda(casse, K);
			if (minCassa->q->qlen < posizione)
			{
				cambi_cassa++;
				mettiInFila(cliente, minCassa);
				cassa_scelta = minCassa;
				printf("Cambio coda cliente %d a cassa %d\n", cliente->id, cassa_scelta->thid);
			}
		}
	}
	Pthread_mutex_unlock(&cliente->mtx);

	return cambi_cassa;
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
	int cambi_cassa;
	long tempo_acquisti;
	clock_t t;
	double tempo_in_coda;
   	
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

	tempo_acquisti = FaiAcquisti(cliente, T, P);
	t = clock();
	cambi_cassa = aspettaInCoda(cliente, casse, K);
	t = clock() - t;
	tempo_in_coda = ((double)t)/CLOCKS_PER_SEC; // calculate the elapsed time

	fprintf(logfile, "CUSTOMER -> | id customer:%d | n. bought products:%d | time in the supermarket: %0.3f s | time in queue: %0.3f s | n. queues checked: %d | \n", cliente->id, cliente->nprod, (double)tempo_acquisti / 1000, tempo_in_coda, cambi_cassa);

	fflush(stdout);
	pthread_exit(NULL);
}

void apriCassa(Cassa_t *casse, int K)
{

	if (casse == NULL)
		return;

	Cassa_t *cassa_scelta = NULL;
	int i = 0;

	// Prende la prima cassa chiusa della lista
	while (cassa_scelta == NULL && i < K)
	{
		// Se la cassa e' disattiva
		if (isActive(&casse[i]) == 0)
			cassa_scelta = casse + i;
		i++;
	}

	if (cassa_scelta == NULL)
	{
		fprintf(stderr, "nessuna cassa disattiva\n");
	}
	else
	{
		Pthread_mutex_lock(&cassa_scelta->mtx);
		cassa_scelta->active = 1;
		Pthread_cond_signal(&cassa_scelta->cond);
		Pthread_mutex_unlock(&cassa_scelta->mtx);
		printf("Apro cassa %d\n", cassa_scelta->thid);
	}
}

int chiudiCassa(Cassa_t *casse, int K)
{
	int conut_aperte = 0;

	Cassa_t *cassa_scelta = NULL;
	for (int i = 0; i < K; ++i)
	{
		// Se la cassa e' attiva e se la cassa scelta e' NULL oppure con coda più lunga viene scelta
		if (isActive(&casse[i]))
		{
			conut_aperte++;
			if (cassa_scelta == NULL || length(cassa_scelta->q) > length(casse[i].q))
				cassa_scelta = casse + i;
		}
	}

	if (cassa_scelta == NULL)
	{
		fprintf(stderr, "nessuna cassa attiva\n");
	}
	else
	{
		if (conut_aperte > 1)
		{
			Pthread_mutex_lock(&cassa_scelta->mtx);
			cassa_scelta->active = 0;
			Pthread_cond_signal(&cassa_scelta->cond);
			Pthread_mutex_unlock(&cassa_scelta->mtx);

			printf("Chiudo cassa %d\n", cassa_scelta->thid);

			conut_aperte--;
		}
	}

	return conut_aperte;
}

// thread direttore
void *ComunicazioneDirettore(void *args)
{
	Cassa_t *casse = ((threadDirettoreArgs_t *)args)->casse;
	int K = ((threadDirettoreArgs_t *)args)->K;

	struct sockaddr_un serv_addr = init_servaddr();
	int sockfd;
	int count_aperte = K;

	SYSCALL(sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");

	msg_t message = {0, DIRETTOREID};
	int notused;

	SYSCALL(notused, connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "connect to director");

	while (1)
	{
		int operation;

		SYSCALL(notused, writen(sockfd, &message, sizeof(msg_t)), "writen client");

		SYSCALL(notused, readn(sockfd, &operation, sizeof(int)), "readn client");

		// fprintf(stderr, "Connection result: %d\n", operation);

		if (operation == APRICASSA && count_aperte < K)
		{
			count_aperte++;
			printf("Apro\n");
			apriCassa(casse, K);
		}

		if (operation == CHIUDICASSA && count_aperte > 1)
		{
			count_aperte = chiudiCassa(casse, K);
			printf("Chiuso\n");
		}
	}

	close(sockfd);

	fflush(stdout);
	pthread_exit(NULL);
}

void *InviaCoda(void *args)
{
	long intervallo = ((thInvia_args_t *)args)->intervallo;
	Cassa_t *cassa = ((thInvia_args_t *)args)->cassa;

	struct sockaddr_un serv_addr = init_servaddr();
	int sockfd;

	SYSCALL(sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");

	msg_t message = {length(cassa->q), cassa->thid};
	int notused;

	SYSCALL(notused, connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "connect");

	while (1)
	{
		if (cassa->active == 1)
		{
			message.len = length(cassa->q);
			message.active = 1;
		} else {
			message.active = 0;
		}

		int operation;

		SYSCALL(notused, writen(sockfd, &message, sizeof(msg_t)), "writen client");

		SYSCALL(notused, readn(sockfd, &operation, sizeof(int)), "read");
		

		msleep(intervallo);
	}

	close(sockfd);
}

// thread cassiere
void *Cassiere(void *arg)
{
	Cassa_t *cassa = (Cassa_t *)arg;
	pthread_t th_invia_coda;
	int intervallo = 1;
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
			if (length(cassa->q) > 0)
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
	{
		if (pthread_create((*th + i), NULL, Cassiere, (*thARGS + i)) != 0)
		{
			fprintf(stderr, "pthread_create failed\n");
			exit(EXIT_FAILURE);
		}
		msleep(1);
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
static void gestore(int signum)
{
	printf("Ricevuto segnale %d\n", signum);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct sigaction s;
	/* inizializzo s a 0*/
	memset(&s, 0, sizeof(s));
	s.sa_handler = gestore; /* registro gestore */
	/* installo nuovo gestore s */
	ec_meno1(sigaction(SIGINT, &s, NULL), "Signal handler error");

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
	pthread_t th_comunicazione_direttore;
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

	if ((logfile = fopen("logfile.log", "w")) == NULL)
	{
		fprintf(stderr, "Stats file opening failed");
		exit(EXIT_FAILURE);
	}

	initCassieri(&th_cassieri, &casse, K, TP);

	threadDirettoreArgs_t thDirettoreARGS = {K, S1, S2, casse};
	if (pthread_create(&th_comunicazione_direttore, NULL, ComunicazioneDirettore, &thDirettoreARGS) != 0)
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

	fclose(logfile);
	free(th_clienti);
	free(th_new_clienti);
	free(th_cassieri);

	return 0;
}