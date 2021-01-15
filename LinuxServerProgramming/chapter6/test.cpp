#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define BUF_SIZE 5 

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

				int sock=socket(PF_INET, SOCK_STREAM, 0);
				assert(sock>=0);

				if(connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr))<0)
				{
								printf("errno is %d", errno);
								return 1;
				}
				else
				{
								char buffer[BUF_SIZE];
								memset(buffer, 'a', BUF_SIZE);
								buffer[BUF_SIZE-1]='\0';
								send(sock, buffer, BUF_SIZE-1, 0);
								while(recv(sock, buffer, BUF_SIZE-1, 0)>0)
								{
												printf("recv %s\n", buffer);
								}
				}
				close(sock);
				return 0;
}
