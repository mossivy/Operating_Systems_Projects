#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include "webserver.h"
#include <errno.h>

#define MAX_REQUEST 100

int port, numThread;
int produced_fd = 0;
int consumed_fd = 0;
int numProducers = 3;
int request[MAX_REQUEST]; // shared buffer
pthread_t *workers;
pthread_t listener;
pthread_t *producers;
pthread_t controller;
// semaphores and a mutex lock
sem_t sem_full;
sem_t sem_empty;
// semaphore to check for how many processes have called process()
// this is used for crash handling
sem_t sem_process;
pthread_mutex_t mutex;
pthread_mutex_t mutex_producer;


void *consumer(void *arg) {
    int s;
    sem_wait(&sem_full);
    pthread_mutex_lock(&mutex);
    s = request[consumed_fd];
    consumed_fd = (consumed_fd + 1) % MAX_REQUEST;
    pthread_mutex_unlock(&mutex);
    sem_post(&sem_empty);
    sem_post(&sem_process);
    process(s);
    int status = 1;
    pthread_exit(&status);
}

void thread_control() {
    int began_process = 0;
    while (began_process != numThread) {
        sem_getvalue(&sem_process, &began_process);
    }
    // All threads have now attempted the process()
    for (int i = 0; i < numThread; i++) {
        int status;
        void* exitVal;
        status = pthread_tryjoin_np(workers[i], &exitVal);
        if (status == EBUSY) {
            i--;
        }
        else if (exitVal == NULL) {
            workers[i] = NULL;
            pthread_create(&(workers[i]), NULL, consumer, NULL);
            i--;
        }
    }
}

void *producer(void* sock) {
    while(1) {
        int s;
        pthread_mutex_lock(&mutex_producer);
        s = accept((int)sock, NULL, NULL);
        pthread_mutex_unlock(&mutex_producer);
        if (!(s < 0)) {//break;
            sem_wait(&sem_empty);
            pthread_mutex_lock(&mutex);
            request[produced_fd] = s;
s            produced_fd = (produced_fd + 1) % MAX_REQUEST;
            pthread_mutex_unlock(&mutex);
            sem_post(&sem_full);
        }
    }
}

void *req_handler(void *arg)
{

    int r;
		struct sockaddr_in sin;
		struct sockaddr_in peer;
		int peer_len = sizeof(peer);
		int sock;

		sock = socket(AF_INET, SOCK_STREAM, 0);

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons(port);
		r = bind(sock, (struct sockaddr *) &sin, sizeof(sin));
		if(r < 0) {
				perror("Error binding socket:");
				exit(0);
		}

		r = listen(sock, 5);
		if(r < 0) {
				perror("Error listening socket:");
				exit(0);
		}

		printf("HTTP server listening on port %d\n", port);


        for (int i = 0; i < numProducers; i++)
            pthread_create(&(producers[i]), NULL, producer, (void*)sock);
//        for (int i = 0; i < numProducers; i++)
//            pthread_join(producers[i], NULL);

//		close(sock);
}


int main(int argc, char *argv[])
{
		if(argc < 2 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000)
		{
				fprintf(stderr, "./webserver PORT(2001 ~ 49999) (#_of_threads) (crash_rate(%))\n");
				return 0;
		}

		int i;

		// port number
		port = atoi(argv[1]);

		// # of worker thread
		if(argc > 2)
				numThread = atoi(argv[2]);
		else numThread = 1;

		// crash rate
		if(argc > 3)
				CRASH = atoi(argv[3]);
		if(CRASH > 50) CRASH = 50;

        sem_init(&sem_empty, 0, MAX_REQUEST);
        sem_init(&sem_full, 0, 0);
        sem_init(&sem_process, 0, 0);

		printf("[pid %d] CRASH RATE = %d\%\n", getpid(), CRASH);

        workers = calloc(numThread, sizeof(pthread_t));
        producers = calloc(numProducers, sizeof(pthread_t));
        for (int i = 0; i < numThread; i++)
            pthread_create(&(workers[i]), NULL, consumer, NULL);
        pthread_create(&listener, NULL, req_handler, NULL);
        thread_control();
        while(1);
		return 0;
}
