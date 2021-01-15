#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[])
{
				if(argc<=2)
				{
								printf("usage: %s ip port", basename(argv[0]));
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=AF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(AF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(AF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				sleep(10);

				sockaddr_in clientAddr;
				socklen_t clientAddrLen=sizeof(clientAddr);
				bzero(&clientAddr, sizeof(clientAddr));

				int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
				if(confd<0)
				{
								printf("error is %d", errno);
				}
				else
				{
								char remote[INET_ADDRSTRLEN];
								printf("connected with ip:%s and port: %d", inet_ntop(AF_INET, &clientAddr.sin_addr, remote, INET_ADDRSTRLEN), ntohs(clientAddr.sin_port));
								close(confd);
				}
				close(listenSock);
				return 0;

}
