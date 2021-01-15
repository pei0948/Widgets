#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

int setnonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption | O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

void addfd(int epollfd, int fd)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN | EPOLLET;
				epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
				setnonblock(fd);
}

//信号处理函数
void sigHandler(int sig)
{
				//保留原来的errno,在函数结束后恢复,保证函数的可重入性
				int oldErrno=errno;
				int msg=sig;
				send(pipefd[1], (char *)&msg, 1, 0);
				errno=oldErrno;
}

//设置信号处理函数
void addSig(int sig)
{
				struct sigaction sa;
				bzero(&sa, sizeof(sa));
				sa.sa_handler=sigHandler;
				sa.sa_flags |= SA_RESTART;
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

				epoll_event events[MAX_EVENT_NUMBER];

				int epollfd=epoll_create(5);
				assert(epollfd!=-1);
				addfd(epollfd, listenSock);
				
				res=socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
				assert(res!=-1);
				setnonblock(pipefd[1]);
				addfd(epollfd, pipefd[0]);

				addSig(SIGHUP);
				addSig(SIGCHLD);
				addSig(SIGTERM);
				addSig(SIGINT);
				
				bool stopServer=false;
				while(!stopServer)
				{
								int cnt=epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
								if(cnt<0 && errno!=EINTR)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<cnt; ++i)
								{
												int sockfd=events[i].data.fd;
												if(sockfd==listenSock)
												{
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int confd=accept(sockfd, (sockaddr*)&clientAddr, &clientAddrLen);
																addfd(epollfd, confd);
												}
												else if(sockfd==pipefd[0] && (events[i].events & EPOLLIN))
												{
																int sig;
																char signals[1024];
																res=recv(pipefd[0], signals, 1023, 0);
																if(res==-1)
																{
																				continue;
																}
																else if(res==0)
																{
																				continue;
																}
																else
																{
																				for(int i=0; i<res; ++i)
																				{
																								switch(signals[i])
																								{
																												case SIGCHLD:
																												case SIGHUP:
																																break;
																												case SIGTERM:
																												case SIGINT:
																																stopServer=true;
																																break;
																								}
																				}
																}
												}
								}
				}
				printf("close fds\n");
				close(listenSock);
				close(pipefd[1]);
				close(pipefd[0]);
				return 0;
}
