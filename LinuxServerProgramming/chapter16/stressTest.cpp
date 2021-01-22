#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

//请求内容
static const char* req="GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxxxxxxxxxx";

const int EVENT_NUM=10000;
const int BUF_SIZE=2048;

int setNonblock(int fd)
{
				int oldOption=fcntl(fd, F_GETFL);
				int newOption=oldOption|O_NONBLOCK;
				fcntl(fd, F_SETFL, newOption);
				return oldOption;
}

void addFd(int epollFd, int fd)
{
				epoll_event event;
				event.data.fd=fd;
				event.events=EPOLLOUT|EPOLLET|EPOLLERR;
				epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
				setNonblock(fd);
}

//向服务器写 n B 数据
bool writeNbytes(int sockFd, const char* buf, int n)
{
				printf("write %d bytes to socket %d\n", n, sockFd);
				int byteCnt=0;
				while(1)
				{
								byteCnt=send(sockFd, buf, n, 0);
								if(byteCnt==-1)
												return false;
								else if(byteCnt==0)
												return false;
								n-=byteCnt;
								buf+=byteCnt;
								if(n<=0)
												return true;
				}
}

//从服务器读取数据
bool readOnce(int sockFd, char *buf, int n)
{
				memset(buf, '\0', n);
				int byteCnt=0;
				byteCnt=recv(sockFd, buf, n-1, 0);
				if(byteCnt==-1)
								return false;
				else if(byteCnt==0)
								return false;
				printf("read %d bytes from socket %d\n", byteCnt, sockFd);
				return true;
}

//向服务器发起num个连接
void startConn(int epollFd, int num, const char *ip, int port)
{
				sockaddr_in serverAddr;
				bzero(&serverAddr, sizeof(serverAddr));
				serverAddr.sin_family=PF_INET;
				serverAddr.sin_port=htons(port);
				inet_pton(PF_INET, ip, &serverAddr.sin_addr);

				int res=0;
				
				for(int i=0; i<num; ++i)
				{
								sleep(1);
								int sockFd=socket(PF_INET, SOCK_STREAM, 0);
								printf("create 1 socket\n");
								if(sockFd<0)
												continue;
								if(connect(sockFd, (sockaddr*)&serverAddr, sizeof(serverAddr))==0)
								{
												printf("build the connection\n");
												addFd(epollFd, sockFd);
								}
								else
								{
												printf("error is %d\n", errno);
								}
				}
}

void closeConn(int epollFd, int sockFd)
{
				epoll_ctl(epollFd, EPOLL_CTL_DEL, sockFd, 0);
				close(sockFd);
}

int main(int argc, char* argv[])
{
				assert(argc==4);
				int epollFd=epoll_create(100);
				
				startConn(epollFd, atoi(argv[3]), argv[1], atoi(argv[2]));
				epoll_event events[EVENT_NUM];
				char buf[BUF_SIZE];

				while(1)
				{
								int cnt=epoll_wait(epollFd, events, EVENT_NUM, 2000);
								for(int i=0; i<cnt; ++i)
								{
												int sockFd=events[i].data.fd;
												if(events[i].events & EPOLLIN)
												{
																if(!readOnce(sockFd, buf, BUF_SIZE))
																				closeConn(epollFd, sockFd);
																epoll_event event;
																event.events=EPOLLOUT|EPOLLET|EPOLLERR;
																event.data.fd=sockFd;
																epoll_ctl(epollFd, EPOLL_CTL_MOD, sockFd, &event);
												}
												else if(events[i].events & EPOLLOUT)
												{
																if(!writeNbytes(sockFd, req, strlen(req)))
																				closeConn(epollFd, sockFd);
																epoll_event event;
																event.events=EPOLLIN|EPOLLET|EPOLLERR;
																event.data.fd=sockFd;
																epoll_ctl(epollFd, EPOLL_CTL_MOD, sockFd, &event);
												}
												else if(events[i].events & EPOLLERR)
												{
																closeConn(epollFd, sockFd);
												}
								}
				}
				return 0;
}
