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

#define MAX_EVENT_SIZE 1024
#define BUF_SIZE 1024

struct fds
{
				int epollfd;
				int sockfd;
};

int setnonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption | O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

void addfd(int epollfd, int fd, bool oneshot)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN | EPOLLET;
				if(oneshot)
				{
								event.events |= EPOLLONESHOT;
				}
				epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
				setnonblock(fd);
}

void resetOneshot(int epollfd, int fd)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLIN | EPOLLET | EPOLLONESHOT;
				epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void* worker(void *arg)
{
				int epollfd=((fds*)arg)->epollfd;
				int sockfd=((fds*)arg)->sockfd;
				printf("start new thread to receive data on fd %d\n", sockfd);
				char buf[BUF_SIZE];
				memset(buf, '\0', BUF_SIZE);
				while(1)
				{
								int res=recv(sockfd, buf, BUF_SIZE-1, 0);
								if(res==0)
								{
												close(sockfd);
												break;
								}
								else if(res<0)
								{
												if(errno == EAGAIN)
												{
																resetOneshot(epollfd, sockfd);
																printf("read later\n");
																break;
												}
								}
								else
								{
												printf("get content %s\n", buf);
												sleep(5);//休眠5秒,模拟数据处理
								}
				}
				printf("end thread receive data on %d\n", sockfd);
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
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int listenSock=socket(PF_INET, SOCK_STREAM, 0);
				assert(listenSock>=0);

				int res=bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
				assert(res!=-1);

				res=listen(listenSock, 5);
				assert(res!=-1);

				epoll_event events[MAX_EVENT_SIZE];
				int epollfd=epoll_create(5);
				assert(epollfd!=-1);

				addfd(epollfd, listenSock, false);//listenSock不能注册EPOLLONESHOT事件,否则应用程序只能处理一个客户连接,后续的客户连接请求将不再触发listenSock上的EPOLLIN事件

				while(1)
				{
								res=epoll_wait(epollfd, events, MAX_EVENT_SIZE, -1);
								if(res<0)
								{
												printf("epoll fails\n");
												break;
								}
								for(int i=0; i<res; ++i)
								{
												int sockfd=events[i].data.fd;
												if(sockfd==listenSock)
												{
																sockaddr_in clientAddr;
																bzero(&clientAddr, sizeof(clientAddr));
																socklen_t clientAddrLen=sizeof(clientAddr);
																int confd=accept(listenSock, (sockaddr*)&clientAddr, &clientAddrLen);
																addfd(epollfd, confd, true);
												}
												else if(events[i].events & EPOLLIN)
												{
																pthread_t thread;
																fds needSolveFds;
																needSolveFds.epollfd=epollfd;
																needSolveFds.sockfd=sockfd;
																pthread_create(&thread, NULL, worker, (void*)&needSolveFds);
												}
												else
												{
																printf("something else happens\n");
												}
								}
				}
				close(listenSock);
				return 0;
}
