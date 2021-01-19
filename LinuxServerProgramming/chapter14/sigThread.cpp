#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define handleErrorEn(en, msg)\
				do{errno=en; perror(msg); exit(EXIT_FAILURE);}while(0)

static void *sigThread(void *arg)
{
				printf("enter thread\n");
				sigset_t *set=(sigset_t*)arg;
				int s, sig;
				for(;;)
				{
								s=sigwait(set, &sig);
								if(s!=0)
												handleErrorEn(s, "sigwait");
								printf("signal handling thread got signal %d\n", sig);
				}
}

int main(int argc, char* argv[])
{
				pthread_t thread;
				sigset_t set;
				int s;

				sigemptyset(&set);
				sigaddset(&set, SIGQUIT);
				sigaddset(&set, SIGUSR1);

				s=pthread_sigmask(SIG_BLOCK, &set, NULL);
				if(s!=0)
								handleErrorEn(s, "pthread_sigmask");

				s=pthread_create(&thread, NULL, sigThread, (void*)&set);
				if(s!=0)
								handleErrorEn(s, "pthread_create");
				
				pause();
}
