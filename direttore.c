#include <stdio.h>
#include <pthread.h>
#include <conn.h>
#include "queue.h"
#include "global.h"

void apriCassa(Cassa_t *casse, int K)
{
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
	}

	Pthread_mutex_lock(&cassa_scelta->mtx);
	cassa_scelta->active = 0;
	Pthread_cond_signal(&cassa_scelta->cond);
	Pthread_mutex_unlock(&cassa_scelta->mtx);

	printf("Chiudo cassa %d\n", cassa_scelta->thid);
}

void Direttore(void *arg)
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
		for (int i = 0; i < K; i++)
		{
			int coda = length(casse[i].q);
			if (coda <= 1)
				count_max1cliente++;
			if (coda >= S2)
				count_minS2clienti++;
			if (isActive(&casse[i]) == 1)
				count_aperte++;
		}

		// printf("Aperte:%d\n", count_aperte);

		// Se il numero di casse con un cliente solo in fila supera S1
		// chiudo una cassa
		if (count_max1cliente >= S1)
			if (count_aperte > 1)
				chiudiCassa(casse, K);

		// Se esiste una cassa con almeno S2 clienti in fila
		// apro un'altra cassa
		if (count_minS2clienti > 0)
			if (count_aperte < K)
				apriCassa(casse, K);
	}
}

void cleanup()
{
	unlink(SOCKNAME);
}
int cmd(const char str[], char *buf)
{
	int tobc[2];
	int frombc[2];

	int notused;
	SYSCALL(notused, pipe(tobc), "pipe1");
	SYSCALL(notused, pipe(frombc), "pipe2");

	if (fork() == 0)
	{
		// chiudo i descrittori che non uso
		SYSCALL(notused, close(tobc[1]), "close");
		SYSCALL(notused, close(frombc[0]), "close");

		SYSCALL(notused, dup2(tobc[0], 0), "dup2 child (1)");	// stdin
		SYSCALL(notused, dup2(frombc[1], 1), "dup2 child (2)"); // stdout
		SYSCALL(notused, dup2(frombc[1], 2), "dup2 child (3)"); // stderr

		execl("/usr/bin/bc", "bc", "-l", NULL);
		return -1;
	}
	// chiudo i descrittori che non uso
	SYSCALL(notused, close(tobc[0]), "close");
	SYSCALL(notused, close(frombc[1]), "close");
	int n;
	SYSCALL(n, write(tobc[1], (char *)str, strlen(str)), "writen");
	SYSCALL(n, read(frombc[0], buf, BUFSIZE), "readn"); // leggo il risultato o l'errore
	SYSCALL(notused, close(tobc[1]), "close");			// si chiude lo standard input di bc cosi' da farlo terminare
	SYSCALL(notused, wait(NULL), "wait");
	return n;
}

int main(int argc, char *argv[])
{

	// Numero di casse
	const int K=2;

	// Soglia per la chiusura di una cassa
	const int S1 = 2;

	// Soglia per l'apertura di una cassa
	const int S2 = 10;

	// cancello il socket file se esiste
	cleanup();
	// se qualcosa va storto ....
	atexit(cleanup);

	int listenfd;
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
	SYSCALL(notused, listen(listenfd, 1), "listen");

	int connfd, n = 1;

	do
	{
		SYSCALL(connfd, accept(listenfd, (struct sockaddr *)NULL, NULL), "accept");

		msg_t message;
		SYSCALL(notused, readn(connfd, &message, sizeof(msg_t)), "readn");

		// printf("%d, %d\n", message.len, message.cassa_id);

		SYSCALL(notused, writen(connfd, &n, sizeof(int)), "writen");

		close(connfd);
	} while (1);

	return 0;
}