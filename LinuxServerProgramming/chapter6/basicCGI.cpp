#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
				if(argc<3)
				{
								printf("ip port\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				sockaddr_in clientAddr;
				bzero(&clientAddr, sizeof(clientAddr));
				socklen_t clientAddrLen=sizeof(clientAddr);

				int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
				if(confd>0)
				{
								close(STDOUT_FILENO);
								dup(confd);
								printf("abcd\n");
								close(confd);
				}
				close(listenSock);
				return 0;
}
