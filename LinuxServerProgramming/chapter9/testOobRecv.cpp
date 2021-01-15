#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define BUF_SIZE 1024

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
				if(confd<0)
				{
								printf("error is %d", errno);
								close(listenSock);
								return 0;
				}

				char buf[BUF_SIZE];
				fd_set readFds;
				fd_set exceptionFds;
				FD_ZERO(&readFds);
				FD_ZERO(&exceptionFds);

				while(1)
				{
								FD_SET(confd, &readFds);
								FD_SET(confd, &exceptionFds);

								res=select(confd+1, &readFds, NULL, &exceptionFds, NULL);
								if(res<0)
								{
												printf("selection fails\n");
												break;
								}
								if(FD_ISSET(confd, &readFds))
								{

												memset(buf, '\0', BUF_SIZE);

												res=recv(confd, buf, BUF_SIZE-1, 0);
												if(res<=0)
												{
																break;
												}
												printf("get %d bytes of normal data: %s\n", res, buf);
								}
								if(FD_ISSET(confd, &exceptionFds))
								{

												memset(buf, '\0', BUF_SIZE);

												res=recv(confd, buf, BUF_SIZE-1, MSG_OOB);
												if(res<=0)
												{
																break;
												}
												printf("get %d bytes of oob data: %s\n", res, buf);
								}
				}
				close(confd);
				close(listenSock);
				return 0;
}
