#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
				if(argc<=2)
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
				socklen_t clientAddrLen=sizeof(clientAddr);

				int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
				if(confd<0)
				{
								printf("errno is %d\n", errno);
				}
				else
				{
								char buffer[BUF_SIZE];
								memset(buffer, '\0', BUF_SIZE);
								res=recv(confd, buffer, BUF_SIZE-1, 0);
								printf("got %d normal bytes, %s\n", res, buffer);

								memset(buffer, '\0', BUF_SIZE);
								res=recv(confd, buffer, BUF_SIZE-1, MSG_OOB);
								printf("got %d oob bytes, %s\n", res, buffer);

								memset(buffer, '\0', BUF_SIZE);
								res=recv(confd, buffer, BUF_SIZE-1, 0);
								printf("got %d normal bytes, %s\n", res, buffer);

								close(confd);
				}
				close(listenSock);
				return 0;
}
