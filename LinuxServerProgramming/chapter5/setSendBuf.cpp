#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 512

int main(int argc, char *argv[])
{
				if(argc<=3)
				{
								printf("ip port buf");
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

				int sock=socket(PF_INET, SOCK_STREAM, 0);
				assert(sock>=0);

				int len=sizeof(buf);
				setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
				getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buf, (socklen_t*)&len);
				printf("the tcp send buffer size after setting is %d\n", buf);

				if(connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr))!=-1)
				{
								char buffer[BUF_SIZE];
								memset(buffer, 'A', BUF_SIZE);
								send(sock, buffer, BUF_SIZE-1, 0);
				}
				close(sock);
				return 0;
}
