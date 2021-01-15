#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define BUF_SIZE 10

int setnonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption | O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

//将文件描述符fd上的EPOLLIN注册到epollfd指示的epoll内核事件表中,参数enableEt指定是否对fd启用ET模式
void addfd(int epollfd, int fd, bool enableEt)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN;
				if(enableEt)
				{
								event.events |= EPOLLET;
				}
				epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
				setnonblock(fd);
}

//lt工作流程
void lt(epoll_event *events, int number, int epollfd, int listenSock)
{
				char buf[BUF_SIZE];
				for(int i=0; i<number; ++i)
				{
								int sockfd=events[i].data.fd;
								if(sockfd==listenSock)
								{
												sockaddr_in clientAddr;
												bzero(&clientAddr, sizeof(clientAddr));
												socklen_t clientAddrLen=sizeof(clientAddr);
												int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
												addfd(epollfd, confd, false);//对confd禁用ET模式
								}
								else if(events[i].events & EPOLLIN)//只要socket读缓存中还有未读出的数据,这段代码就会被触发
								{
												printf("event trigger once\n");
												memset(buf, '\0', BUF_SIZE);
												int res=recv(sockfd, buf, BUF_SIZE-1, 0);
												if(res<=0)
												{
																close(sockfd);
																continue;
												}
												printf("get %d bytes: %s\n", res, buf);
								}
								else
								{
												printf("something else happens\n");
								}
				}
}

//et工作流程
void et(epoll_event *events, int number, int epollfd, int listenSock)
{
				char buf[BUF_SIZE];
				for(int i=0; i<number; ++i)
				{
								int sockfd=events[i].data.fd;
								if(sockfd==listenSock)
								{
												sockaddr_in clientAddr;
												bzero(&clientAddr, sizeof(clientAddr));
												socklen_t clientAddrLen=sizeof(clientAddr);
												int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
												addfd(epollfd, confd, true);//对confd开启ET模式
								}
								else if(events[i].events & EPOLLIN)//这段代码不会被重复触发,所以我们循环读取数据,以确保把socket读缓存区中的所有数据读出
								{
												printf("event trigger once\n");
												while(1)
												{
																memset(buf, '\0', BUF_SIZE-1);
																int res=recv(sockfd, buf, BUF_SIZE-1, 0);
																if(res<0)
																{
																				if(errno == EAGAIN || errno == EWOULDBLOCK)//数据被全部读取
																				{
																								printf("read later\n");
																								break;
																				}
																				close(sockfd);
																				break;
																}
																else if(res==0)
																{
																				close(sockfd);
																}
																else
																{
																				printf("get %d bytes: %s\n", res, buf);
																}
												}
								}
								else
								{
												printf("something else happend\n");
								}
				}
}

int main(int argc, char *argv[])
{
				if(argc<3)
				{
								printf("ip port\n");
								return 0;
				}
				const char *ip=argv[1];
				int port=atoi(argv[2]);

				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_port=htons(port);
				serverAddr.sin_family=PF_INET;
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				epoll_event events[MAX_EVENT_NUMBER];
				int epollfd=epoll_create(5);
				assert(epollfd!=-1);
				addfd(epollfd, listenSock, true);

				while(1)
				{
								res=epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
								if(res<0)
								{
												printf("epoll fails\n");
												break;
								}
								lt(events, res, epollfd, listenSock);//使用LT模式
								//et(events, res, epollfd, listenSock);//使用ET模式
				}
				close(listenSock);
				return 0;
}
