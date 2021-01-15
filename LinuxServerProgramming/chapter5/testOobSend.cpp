#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
				if(argc<=2)
				{
								printf("usage: %s ip  port\n", basename(argv[0]));
								return 1;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=AF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(AF_INET, ip, &serverAddr.sin_addr);

				int sock=socket(AF_INET, SOCK_STREAM, 0);
				assert(sock>=0);

				if(connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr))<0)
				{
								printf("connection fails");
								return 1;
				}
				else
				{
								const char *oobData="abc";
								const char *normData="123";
								send(sock, normData, strlen(normData), 0);
								send(sock, oobData, strlen(oobData), MSG_OOB);
								send(sock, normData, strlen(normData), 0);
				}
				close(sock);
				return 0;
}
