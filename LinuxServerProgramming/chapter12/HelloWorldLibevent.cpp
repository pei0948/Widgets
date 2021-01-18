#include <sys/signal.h>
#include <event.h>

void cbSignal(int fd, short event, void *argc)
{
				struct event_base* base=(event_base*)argc;
				struct timeval delay={2, 0};
				printf("Caught an interrupt signal; exiting cleanly in two seconds...\n");
				event_base_loopexit(base, &delay);
}
void cbTimeout(int fd, short event, void *argc)
{
				printf("timeout\n");
}

int main(int argc, char *argv[])
{
				struct event_base *base=event_init();
				
				struct event* signalEvent=evsignal_new(base, SIGINT, cbSignal, base);
				event_add(signalEvent, NULL);

				timeval tv={1, 0};
				struct event* timeOutEvent=evtimer_new(base, cbTimeout, NULL);
				event_add(timeOutEvent, &tv);

				event_base_dispatch(base);

				event_free(timeOutEvent);
				event_free(signalEvent);
				event_base_free(base);
				return 0;
}
