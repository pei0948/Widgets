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
#include <pthread.h>
#include <sys/epoll.h>
#include <signal.h>

#include "TimerList.hpp"

#define FD_LIMIT 65535
#define MAX_EVENT_NUM 1024
#define TIMESLOT 5

static int pipefd[2];
static TimerList timerList;
static int epollfd=0;

int setNonblock(int fd)
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
				setNonblock(fd);
}

void sigHandler(int sig)
{
				int saveErrno=errno;
				int msg=sig;
				send(pipefd[1], (char *)&msg, 1, 0);
				errno=saveErrno;
}

void addSig(int sig)
{
				struct sigaction sa;
				bzero(&sa, sizeof(sa));
				sa.sa_handler=sigHandler;
				sa.sa_flags|=SA_RESTART;
				sigfillset(&sa.sa_mask);
				assert(sigaction(sig, &sa, NULL)!=-1);
}

void timerHandler()
{
				timerList.tick();
				alarm(TIMESLOT);//一次alarm滴啊用只会引起一次SIGALRM信号,所以这里重新定时,以不断触发SIGALRM
}

void cbFunc(ClientData *userData)
{
				if(!userData)
								return;
				epoll_ctl(epollfd, EPOLL_CTL_DEL, userData->sockfd, 0);
				close(userData->sockfd);
				printf("close fd %d\n", userData->sockfd);
//				delete userData;
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



				epoll_event events[MAX_EVENT_NUM];
				int epollfd=epoll_create(5);
				assert(epollfd!=-1);
				addfd(epollfd, listenSock);

				res=socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
				assert(res!=-1);
				setNonblock(pipefd[1]);
				addfd(epollfd, pipefd[0]);
				addSig(SIGALRM);
				addSig(SIGTERM);
				
				bool stopServer=false;

				ClientData *users=new ClientData[FD_LIMIT];
				bool timeout=false;

				alarm(TIMESLOT);//定时
				
				while(!stopServer)
				{
								int cnt=epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
								if(cnt<0 && errno!=EINTR)
								{
												printf("error is %d\n", errno);
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
																int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
																
																addfd(epollfd, confd);
																users[confd].address=clientAddr;
																users[confd].sockfd=confd;

																UtilTimer *timer=new UtilTimer();
																timer->userData=&users[confd];
																timer->cbFunc=cbFunc;
																time_t curr=time(NULL);
																timer->expire=3*TIMESLOT+curr;
																users[confd].timer=timer;
																timerList.addTimer(timer);
												}
												else if(sockfd==pipefd[0] && (events[i].events & EPOLLIN))
												{
																int sig;
																char signals[1024];
																res=recv(pipefd[0], signals, 1023, 0);
																if(res==-1)
																{
																				//handle the error
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
																												case SIGALRM:
																																timeout=true;
																																break;
																												case SIGTERM:
																																stopServer=true;
																																break;
																								}
																				}
																}
												}
												else if(events[i].events & EPOLLIN)
												{
																memset(users[sockfd].buf, '\0', BUF_SIZE);
																res=recv(sockfd, users[sockfd].buf, BUF_SIZE-1, 0);
																printf("get %d bytes: %s\n", res, users[sockfd].buf);
																UtilTimer *timer=users[sockfd].timer;
																if(res<0)
																{
																				if(errno!=EAGAIN)
																				{
																								cbFunc(&users[sockfd]);
																								if(timer)
																								{
																												timerList.delTimer(timer);
																								}
																				}
																}
																else if(res==0)
																{
																				cbFunc(&users[sockfd]);
																				if(timer)
																				{
																								timerList.delTimer(timer);
																				}
																}
																else
																{
																				//有数据可读时,调整定时器时间,延迟关闭
																				if(timer)
																				{
																								time_t cur=time(NULL);
																								timer->expire=cur+3*TIMESLOT;
																								printf("adjust timer once\n");
																								timerList.adjustTimer(timer);
																				}
																}
												}
												else
												{
												}
								}

								

								//最后处理定时事件,I/O事件具有更高优先级,但是这将导致定时任务不能精确按照预期时间运行
								if(timeout)
								{
												timerHandler();
												timeout=false;
								}
								
				}
				close(listenSock);
				close(pipefd[1]);
				close(pipefd[0]);
				delete []users;
				return 0;
}
