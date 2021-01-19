#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

int a=0;
int b=0;

pthread_mutex_t mutexA;
pthread_mutex_t mutexB;

void* another(void* arg)
{
				pthread_mutex_lock(&mutexB);
				printf("int child thread, got mutex B, waiting for mutex A\n");
				sleep(5);
				++b;
				pthread_mutex_lock(&mutexA);
				++a;
				pthread_mutex_unlock(&mutexA);
				pthread_mutex_unlock(&mutexB);
				pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
				pthread_t id;

				pthread_mutex_init(&mutexA, NULL);
				pthread_mutex_init(&mutexB, NULL);

				pthread_create(&id, NULL, another, NULL);

				pthread_mutex_lock(&mutexA);
				printf("in parent thread, got mutex A, waiting for mutex B\n");
				sleep(5);
				++a;
				pthread_mutex_lock(&mutexB);
				++b;
				pthread_mutex_unlock(&mutexB);
				pthread_mutex_unlock(&mutexA);

				pthread_join(id, NULL);
				pthread_mutex_destroy(&mutexA);
				pthread_mutex_destroy(&mutexB);
				
				return 0;
}
