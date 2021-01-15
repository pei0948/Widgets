#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
using namespace std;

static bool stop=false;

static void handleSig(int sig)
{
				stop=true;
}

int main(int argc, char *argv[])
{
				if(argc<=3)
				{
								printf("usage: %s ip port backlog\n", basename(argv[0]));
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);
				int backlog=atoi(argv[3]);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				sockaddr_in address;
				bzero(&address, sizeof(address));

				address.sin_family=AF_INET;
				address.sin_port=htons(port);
				inet_pton(AF_INET, ip, &address.sin_addr);

				int res=bind(listenSock, (sockaddr*)&address, sizeof(address));
				assert(res!=-1);

				res=listen(listenSock, backlog);
				assert(res!=-1);

				while(!stop)
				{
								sleep(1);
				}

				close(listenSock);
				return 0;
}
