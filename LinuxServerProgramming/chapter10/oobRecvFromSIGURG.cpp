#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#define BUF_SIZE 1024

static int confd;

void sigHandler(int sig)
{
				int oldErrno=errno;
				char buf[BUF_SIZE];
				memset(buf, '\0', BUF_SIZE-1);
				int res=recv(confd, buf, BUF_SIZE-1, MSG_OOB);
				printf("recv %d oob bytes: %s\n", res, buf);
				errno=oldErrno;
}
void addSig(int sig, void(*sigHandler)(int))
{
				struct sigaction sa;
				bzero(&sa, sizeof(sa));
				sa.sa_handler=sigHandler;
				sa.sa_flags|=SA_RESTART;
				sigfillset(&sa.sa_mask);
				assert(sigaction(sig, &sa, NULL)!=-1);
}

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
				
				confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);

				if(confd<0)
				{
								printf("errno is %d\n", errno);
								return 0;
				}
				else
				{
								addSig(SIGURG, sigHandler);
								//使用SIGURG之前,必须设置socket上的宿主进程或进程组
								fcntl(confd, F_SETOWN, getpid());

								char buf[BUF_SIZE];
								while(1)
								{
												memset(buf, '\0', BUF_SIZE-1);
												res=recv(confd, buf, BUF_SIZE-1, 0);
												if(res<=0)
												{
																break;
												}
												printf("get %d normal bytes: %s\n", res, buf);
								}
								close(confd);
				}
				close(listenSock);
				return 0;
}
