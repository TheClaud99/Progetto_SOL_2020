#include <stdio.h>
#include <pthread.h>
#include <conn.h>
#include "queue.h"
#include "global.h"

typedef struct threadFArgs
{
	int *casse;
	int K;
	int S1;
	int S2;
	long connfd;
} threadFArgs_t;

void cleanup()
{
	unlink(SOCKNAME);
}

int calcolaScelta(int *casse, int K, int S1, int S2)
{
	int count_max1cliente = 0;
	int count_minS2clienti = 0;

	for (int i = 0; i < K; i++)
	{
		if (casse[i] <= 1)
			count_max1cliente++;
		if (casse[i] >= S2)
			count_minS2clienti++;
	}

	// printf("Aperte:%d\n", count_aperte);

	// Se il numero di casse con un cliente solo in fila supera S1
	// chiudo una cassa
	if (count_max1cliente >= S1)
		return CHIUDICASSA;

	// Se esiste una cassa con almeno S2 clienti in fila
	// apro un'altra cassa
	if (count_minS2clienti > 0)
		return APRICASSA;

	return 0;
}

void *threadF(void *arg)
{
    assert(arg);
	int *casse = ((threadFArgs_t*)arg)->casse;
	int K = ((threadFArgs_t*)arg)->K;
	int S1 = ((threadFArgs_t*)arg)->S1;
	int S2 = ((threadFArgs_t*)arg)->S2;
    long connfd = ((threadFArgs_t*)arg)->connfd;

    do
    {
		int notused;
		msg_t message;
		SYSCALL(notused, readn(connfd, &message, sizeof(msg_t)), "readn");

		if(message.len == -1) break;

		casse[message.cassa_id - 1] = message.len;

		int scelta = calcolaScelta(casse, K, S1, S2);

		// printf("%d, %d\n", message.len, message.cassa_id);

		SYSCALL(notused, writen(connfd, &scelta, sizeof(int)), "writen");

    } while (1);
    close(connfd);
    return NULL;
}

void spawn_thread(long connfd, int *casse, int K, int S1, int S2)
{
    pthread_attr_t thattr;
    pthread_t thid;
	threadFArgs_t threadFARGS = {casse, K, S1, S2, connfd};

    if (pthread_attr_init(&thattr) != 0)
    {
        fprintf(stderr, "pthread_attr_init FALLITA\n");
        close(connfd);
        return;
    }
    // settiamo il thread in modalità detached
    if (pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_DETACHED) != 0)
    {
        fprintf(stderr, "pthread_attr_setdetachstate FALLITA\n");
        pthread_attr_destroy(&thattr);
        close(connfd);
        return;
    }
    if (pthread_create(&thid, &thattr, threadF, &threadFARGS) != 0)
    {
        fprintf(stderr, "pthread_create FALLITA");
        pthread_attr_destroy(&thattr);
        close(connfd);
        return;
    }
}


int main(int argc, char *argv[])
{

	// Numero di casse
	int K = 2;

	// Soglia per la chiusura di una cassa
	int S1 = 2;

	// Soglia per l'apertura di una cassa
	int S2 = 10;

	if (argc == 3)
	{
		K = atoi(argv[1]);
		S1 = atoi(argv[2]);
		S2 = atoi(argv[3]);
	}


	// cancello il socket file se esiste
	cleanup();
	// se qualcosa va storto ....
	atexit(cleanup);

	int listenfd;

	int *casse = malloc(K * sizeof(int));

	// creo il socket
	SYSCALL(listenfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket");

	// setto l'indirizzo
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME) + 1);

	int notused;
	// assegno l'indirizzo al socket
	SYSCALL(notused, bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), "bind");

	// setto il socket in modalita' passiva e definisco un n. massimo di connessioni pendenti
	SYSCALL(notused, listen(listenfd, MAXBACKLOG), "listen");

	do
	{
		long connfd;
		SYSCALL(connfd, accept(listenfd, (struct sockaddr *)NULL, NULL), "accept");

		spawn_thread(connfd, casse, K, S1, S2);
	} while (1);

	return 0;
}