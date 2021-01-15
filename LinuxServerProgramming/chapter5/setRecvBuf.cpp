#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
				if(argc<=3)
				{
								printf("ip port buf\n");
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);
				int buf=atoi(argv[3]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int len=sizeof(buf);
				setsockopt(listenSock, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
				getsockopt(listenSock, SOL_SOCKET, SO_RCVBUF, &buf, (socklen_t*)&len);
				printf("recv buf is set as %d bytes\n", buf);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				sockaddr_in clientAddr;
				socklen_t clientAddrLen=sizeof(clientAddr);

				int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
				if(confd<0)
				{
								printf("error is %d", errno);
				}
				else
				{
								char buffer[BUF_SIZE];
								memset(buffer, '\0', BUF_SIZE);
								while(recv(listenSock, buffer, BUF_SIZE-1, 0)>0)
								{}
								close(confd);
				}
				close(listenSock);
				return 0;
}
